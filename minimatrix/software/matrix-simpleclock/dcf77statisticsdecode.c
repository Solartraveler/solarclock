/* DCF77 Statistics decode
  (c) 2016 by Malte Marwedel
  www.marwedels.de/malte

  This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*/

#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>

#include "dcf77statisticsdecode.h"

/*
Required calculation + memory
assuming sample time of 8 mintues:
8min*60 = 480 bytes of data -> good for PC, ok for AVR
thesis comparisons:
time:52+24+8*24=268
date:12*31*100=37200
total:37468 -> good for PC, too much for AVR.

worst error: DCF77SIGNAL1*DCF77DATAMINUTES*38=4560 -> no 16bit overflow possible
average error perfect signal: worst error/2 = 2280
error on simple analysis with 20% bad signal: 912
date consists of 19 bits (6 day, 5 month, 8 year)
date "correction bits": 4 (1 parity + 3 day in week)
if error is already, more than 4 bits -> correction useless. So dont test if
year has an error more than 4 bits or if year + month has an error more than 4 bits
max error for 4 bits: DCF77SIGNAL1*DCF77DATAMINUTES*4 = 480
reducing allowed error: (DCF77SIGNAL1-DCF77SIGNAL0)*DCF77DATAMINUTES*4 = 240
result: number of required analysis for dates now 1/3 ~ 11000 full date analysis left
abort analysis if part date error is higher than current best value: approx 2000 full date analysis left
abort analysis even if day error is higher than current best value: approx 100 full analysis left, however some noisy data aborted -> increase allowed error to DCF77SIGNAL1*DCF77DATAMINUTES*4 = 480 -> all detected again with <250 full analysis
*/


typedef struct {
	uint8_t decimal;
	uint8_t parity;
} decimal_t;

const decimal_t g_decimal[TWODIGITYEARS] = {
{0x00, 0x00},
{0x01, 0x01},
{0x02, 0x01},
{0x03, 0x00},
{0x04, 0x01},
{0x05, 0x00},
{0x06, 0x00},
{0x07, 0x01},
{0x08, 0x01},
{0x09, 0x00},
{0x10, 0x01},
{0x11, 0x00},
{0x12, 0x00},
{0x13, 0x01},
{0x14, 0x00},
{0x15, 0x01},
{0x16, 0x01},
{0x17, 0x00},
{0x18, 0x00},
{0x19, 0x01},
{0x20, 0x01},
{0x21, 0x00},
{0x22, 0x00},
{0x23, 0x01},
{0x24, 0x00},
{0x25, 0x01},
{0x26, 0x01},
{0x27, 0x00},
{0x28, 0x00},
{0x29, 0x01},
{0x30, 0x00},
{0x31, 0x01},
{0x32, 0x01},
{0x33, 0x00},
{0x34, 0x01},
{0x35, 0x00},
{0x36, 0x00},
{0x37, 0x01},
{0x38, 0x01},
{0x39, 0x00},
{0x40, 0x01},
{0x41, 0x00},
{0x42, 0x00},
{0x43, 0x01},
{0x44, 0x00},
{0x45, 0x01},
{0x46, 0x01},
{0x47, 0x00},
{0x48, 0x00},
{0x49, 0x01},
{0x50, 0x00},
{0x51, 0x01},
{0x52, 0x01},
{0x53, 0x00},
{0x54, 0x01},
{0x55, 0x00},
{0x56, 0x00},
{0x57, 0x01},
{0x58, 0x01},
{0x59, 0x00},
{0x60, 0x00},
{0x61, 0x01},
{0x62, 0x01},
{0x63, 0x00},
{0x64, 0x01},
{0x65, 0x00},
{0x66, 0x00},
{0x67, 0x01},
{0x68, 0x01},
{0x69, 0x00},
{0x70, 0x01},
{0x71, 0x00},
{0x72, 0x00},
{0x73, 0x01},
{0x74, 0x00},
{0x75, 0x01},
{0x76, 0x01},
{0x77, 0x00},
{0x78, 0x00},
{0x79, 0x01},
{0x80, 0x01},
{0x81, 0x00},
{0x82, 0x00},
{0x83, 0x01},
{0x84, 0x00},
{0x85, 0x01},
{0x86, 0x01},
{0x87, 0x00},
{0x88, 0x00},
{0x89, 0x01},
{0x90, 0x00},
{0x91, 0x01},
{0x92, 0x01},
{0x93, 0x00},
{0x94, 0x01},
{0x95, 0x00},
{0x96, 0x00},
{0x97, 0x01},
{0x98, 0x01},
{0x99, 0x00}
};

//----------------------statistics method------------------------

static uint8_t isleapyear(uint8_t year) {
	if ((year & 3) == 0) {
		return 1;
	}
	return 0;
}


static uint8_t calcweekday(uint8_t day, uint8_t month, uint8_t yeartwodigit) {
	uint16_t dayofyear;
	uint16_t weekday;
	//calc day of year
	uint8_t leapyear = isleapyear(yeartwodigit);
	dayofyear = MAXDAYSINMONTH*(month-1);
	if (month > 2) {
		dayofyear = dayofyear - 3 + leapyear;
	}
	if (month > 4) { dayofyear--; }
	if (month > 6) { dayofyear--; }
	if (month > 9) { dayofyear--; }
	if (month > 11) { dayofyear--; }
	dayofyear += day;
	//calc weekday
	weekday = yeartwodigit + (yeartwodigit-1)/4 + dayofyear;
	if (yeartwodigit > 0) { //because year of reference (2000) is a leapyear
		weekday++;
	}
	//Usually saturday would be = 1 (because 1.1.2000 is a saturday), however since saturday 6th day of week, add 5.
	weekday = (weekday % 7)+5;
	if (weekday > 7) { //overflow
		weekday = weekday -7;
	}
	return weekday;
}

static uint8_t datadelta(const uint8_t a, const uint8_t b) {
	if (a > b) {
		return a - b;
	}
	return b - a;
}

static void makewdayparitythesis(uint8_t * thesis, uint8_t year, uint8_t month, uint8_t day) {
	uint8_t dayofweek = calcweekday(day, month, year);
	uint8_t parity;
	parity = g_decimal[day].parity;
	parity ^= g_decimal[month].parity;
	parity ^= g_decimal[year].parity;
	parity ^= g_decimal[dayofweek].parity;
	//day of week
	for (int i = 0; i < DCF77DAYOFWEEKSIZE; i++) {
		thesis[i] = DCF77SIGNAL0 + (dayofweek & 1)*DCF77SIGNALDELTA;
		dayofweek >>= 1;
	}
	//date parity
	thesis[3] = DCF77SIGNAL0 + (parity & 1)*DCF77SIGNALDELTA;
}

static uint16_t similarityminute(const uint8_t * data, uint8_t minute) {
	uint16_t error = 0;
	uint8_t i, j;
	//compare with data
	for (i = 0; i < DCF77DATAMINUTES; i++) {
		uint8_t thesism = g_decimal[minute].decimal | ((g_decimal[minute].parity)<<7);
		for (j = 0; j < DCF77MINUTESIZE; j++) {
			//printf(" compare(m): %i with %i\n", (thesism & 1) * DCF77SIGNALDELTA + DCF77SIGNAL0, data[21 + i * DCF77DIAGRAMSIZE + j]);
			error += datadelta((thesism & 1) * DCF77SIGNALDELTA + DCF77SIGNAL0, data[DCF77MINUTEDATA + i * DCF77DIAGRAMSIZE + j]);
			thesism >>= 1;
		}
		minute++;
	}
	return error;
}

static uint16_t similarityhour(const uint8_t * datamerged, uint8_t hour) {
	//1. make thesis
	uint16_t error = 0;
	int j;
	uint8_t thesish = g_decimal[hour].decimal | ((g_decimal[hour].parity) << 6);
	//2. compare with data
	for (j = 0; j < DCF77HOURSIZE; j++) {
		//printf("compare(h): %i with %i\n", (thesish & 1) * DCF77SIGNALMERGEDDELTA + DCF77SIGNALMERGED0, datamerged[29 + j]);
		error += datadelta((thesish & 1) * DCF77SIGNALMERGEDDELTA + DCF77SIGNALMERGED0, datamerged[DCF77HOURDATA + j]);
		thesish >>= 1;
	}
	return error;
}

static uint16_t similarityhourminute(const uint8_t * data, uint8_t hour, uint8_t minute) {
	uint16_t error = 0;
	uint8_t i, j;
	//1. make thesis
	uint16_t thesis;
	uint8_t thesism;
	uint8_t thesish = g_decimal[hour].decimal | ((g_decimal[hour].parity) << 6);
	//2. compare with data
	for (i = 0; i < DCF77DATAMINUTES; i++) {
		thesism = g_decimal[minute].decimal | ((g_decimal[minute].parity)<<7);
		thesis = thesism | (thesish << 8);
		for (j = 0; j < (DCF77MINUTESIZE + DCF77HOURSIZE); j++) {
			//printf(" compare(mh): %i with %i\n", (thesis & 1) * DCF77SIGNALDELTA + DCF77SIGNAL0, data[21 + i * DCF77DIAGRAMSIZE + j]);
			error += datadelta((thesis & 1) * DCF77SIGNALDELTA + DCF77SIGNAL0, data[DCF77MINUTEDATA + i * DCF77DIAGRAMSIZE + j]);
			thesis >>= 1;
		}
		//printf("\n");
		minute++;
		if (minute >= MINUTESINHOUR) {
			minute = 0;
			hour++;
			thesish = g_decimal[hour].decimal | ((g_decimal[hour].parity) << 6);
		}
		if (hour >= HOURSINDAY) {
			hour = 0;
		}
	}
	return error;
}

static uint16_t similarityday(const uint8_t * datamerged, uint8_t day) {
	//1. make thesis
	uint16_t error = 0;
	int j;
	uint8_t thesisd = g_decimal[day].decimal;
	//2. compare with data
	for (j = 0; j < DCF77DAYSIZE; j++) {
		//printf("compare(day): %i with %i\n", (thesish & 1) * DCF77SIGNALMERGEDDELTA + DCF77SIGNALMERGED0, datamerged[36 + j]);
		error += datadelta((thesisd & 1) * DCF77SIGNALMERGEDDELTA + DCF77SIGNALMERGED0, datamerged[DCF77DAYDATA + j]);
		thesisd >>= 1;
	}
	return error;
}

static uint16_t similaritymonth(const uint8_t * datamerged, uint8_t month) {
	//1. make thesis
	uint16_t error = 0;
	int j;
	uint8_t thesism = g_decimal[month].decimal;
	//2. compare with data
	for (j = 0; j < DCF77MONTHSIZE; j++) {
		//printf("compare(month): %i with %i\n", (thesism & 1) * DCF77SIGNALMERGEDDELTA + DCF77SIGNALMERGED0, datamerged[45 + j]);
		error += datadelta((thesism & 1) * DCF77SIGNALMERGEDDELTA + DCF77SIGNALMERGED0, datamerged[DCF77MONTHDATA + j]);
		thesism >>= 1;
	}
	return error;
}

static uint16_t similarityyear(const uint8_t * datamerged, uint8_t year) {
	//1. make thesis
	uint16_t error = 0;
	int j;
	uint8_t thesisy = g_decimal[year].decimal;
	//2. compare with data
	for (j = 0; j < DCF77YEARSIZE; j++) {
		//printf("compare(year): %i with %i\n", (thesisy & 1) * DCF77SIGNALMERGEDDELTA + DCF77SIGNALMERGED0, datamerged[50 + j]);
		error += datadelta((thesisy & 1) * DCF77SIGNALMERGEDDELTA + DCF77SIGNALMERGED0, datamerged[DCF77YEARDATA + j]);
		thesisy >>= 1;
	}
	return error;
}


static uint16_t similaritywdayparity(const uint8_t * datamerged, uint8_t year, uint8_t month, uint8_t day) {
	//1. make thesis
	uint8_t thesis[DCF77DAYOFWEEKSIZE+1];
	uint16_t error = 0;
	int j;
	makewdayparitythesis(thesis, year, month, day);
	//2. compare with data
	for (j = 0; j < DCF77DAYOFWEEKSIZE; j++) {
		//printf("compare(wday): %i with %i\n", thesis[j]*DCF77DATAMINUTES, datamerged[42 + j]);
		error += datadelta(thesis[j]*DCF77DATAMINUTES, datamerged[DCF77DAYOFWEEKDATA + j]);
	}
	error += datadelta(thesis[DCF77DAYOFWEEKSIZE]*DCF77DATAMINUTES, datamerged[DCF77DATEPARITYDATA]); //parity
	return error;
}

uint8_t dcf77_statisticsdecode(const uint8_t * data, uint8_t * minute, uint8_t * hour,
                               uint8_t *day, uint8_t * month, uint8_t * year2digit,
                               uint16_t * errorrate) {
	uint8_t i, j, k;
	//0. merge data, can speed up things if data did not change
	uint8_t datamerged[DCF77DIAGRAMSIZE];
	memset(datamerged, 0, sizeof(datamerged));
	for (i = DCF77MINUTEDATA; i < DCF77DIAGRAMSIZE; i++) { //data before second 21 are not important
		for (j = 0; j < DCF77DATAMINUTES; j++) {
			datamerged[i] += data[i+DCF77DIAGRAMSIZE*j];
		}
	}
	//1. search for the best hour + minute thesis
	//1.1: search for best minutes without hour change
	uint8_t bestminute = 0;
	uint16_t bestminuteerr = 0xFFFF;
	uint8_t besthour = 0;
	uint16_t besthourerr = 0xFFFF;
	uint8_t bestminute2 = 0;
	uint8_t besthour2 = 0;
	uint16_t bestcombinedmherr = 0xFFFF;
	uint16_t temperr;
	uint16_t besterr;
	uint16_t bestdateerr = DCF77MAXERRORDATE; //This speeds up analysis instead of 0xFFFF
	uint16_t yearerr;
	uint16_t montherr;
	uint16_t dayerr;
	uint16_t yearmontherr;
	uint16_t yearmonthdayerr;
	uint8_t bestday = 1, bestmonth = 1, bestyear = 0;
	uint16_t analyzeddates = 0;
	for (i = 0; i < MINUTESINHOUR - (DCF77DATAMINUTES -1); i++) {
		temperr = similarityminute(data, i);
		//printf("Error for minute %i: %i\n\n", i, temperr);
		if (temperr < bestminuteerr) {
			bestminuteerr = temperr;
			bestminute = i;
		}
	}
	//1.2: search for best minutes without hour change
	for (i = 0; i < HOURSINDAY; i++) {
		temperr = similarityhour(datamerged, i);
		//printf("Error for hour %i: %i\n\n", i, temperr);
		if (temperr < besthourerr) {
			besthourerr = temperr;
			besthour = i;
		}
	}
	//1.3: search for best minutes with hour change
	for (i = 0; i < HOURSINDAY; i++) {
		for (j = MINUTESINHOUR - (DCF77DATAMINUTES -1); j < MINUTESINHOUR; j++) {
			temperr = similarityhourminute(data, i, j);
			//printf("Error for hour %i minute %i: %i\n\n", i, j, temperr);
			if (temperr < bestcombinedmherr) {
				bestcombinedmherr = temperr;
				besthour2 = i;
				bestminute2 = j;
			}
		}
	}
	//1.4: get best value
	if (bestcombinedmherr < (bestminuteerr + besthourerr)) {
		bestminute = bestminute2;
		besthour = besthour2;
		besterr = bestcombinedmherr;
	} else {
		besterr = bestminuteerr + besthourerr;
	}
	*minute = bestminute;
	*hour = besthour;
	*errorrate = besterr;
	//2. search for best date
	if ((bestminute < MINUTESINHOUR - (DCF77DATAMINUTES -1)) || (besthour < (HOURSINDAY -1))) {
		for (i = 0; i < TWODIGITYEARS; i++) {
			yearerr = similarityyear(datamerged, i);
			if (yearerr >= bestdateerr) {
				continue;
			}
			for (j = 1; j <= MONTHSINYEAR; j++) {
				montherr = similaritymonth(datamerged, j);
				yearmontherr = yearerr + montherr;
				if (yearmontherr >= bestdateerr) {
					continue;
				}
				for (k = 1; k <= MAXDAYSINMONTH; k++) {
					dayerr = similarityday(datamerged, k);
					yearmonthdayerr = yearmontherr + dayerr;
					if (yearmonthdayerr >= bestdateerr) {
						continue;
					}
					temperr = yearmonthdayerr + similaritywdayparity(datamerged, i, j, k);
					analyzeddates++;
					if (temperr < bestdateerr) {
						bestdateerr = temperr;
						bestyear = i;
						bestmonth = j;
						bestday = k;
					}
				}
			}
		}
		besterr += bestdateerr;
	} else {
		return 1;
	}
	*day = bestday;
	*month = bestmonth;
	*year2digit = bestyear;
	*errorrate = besterr;
	if (bestdateerr < DCF77MAXERRORDATE) {
		return 2;
	}
	return 0;
}

