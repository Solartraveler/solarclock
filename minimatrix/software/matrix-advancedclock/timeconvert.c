/* Matrix-Advancedclock
  (c) 2014-2016 by Malte Marwedel
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

#include <stdint.h>

const uint16_t g_dayoffset[12] = {0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334};

static uint8_t isleapyear(uint16_t year) {
	if ((((year % 4) == 0) && !(year % 100 == 0)) || ((year % 400) == 0)) {
		return 1;
	}
	return 0;
}

/*
day must be given 0...30
month must be given 0...11
*/
uint16_t dayofyear(uint8_t day, uint8_t month, uint16_t year) {
	if (month < 12) {
		uint16_t dayofyear = g_dayoffset[month]+day;
		if (isleapyear(year)) {
			if (month > 1) { //2= march
				dayofyear++;
			}
		}
		return dayofyear;
	}
	return 0;
}

/*
returns the month and day from the day in year and the year.
input:
year: can be 2 or 4 digits, since its only used for leap year calculation
dayinyear: 0...365
output:
month is in the range 0...11
day is in the range 0...30
So to show something proper, add +1 later.
*/
void monthdayfromdayinyear(uint16_t dayinyear, uint16_t year, uint8_t * month, uint8_t * day) {
	uint8_t leapyear = isleapyear(year);
	uint8_t i;
	*month = 0;
	*day = dayinyear;
	for (i = 1; i < 12; i++) {
		uint16_t monthlimit = g_dayoffset[i];
		if (i > 1) {
			monthlimit += leapyear; //either add 0 or 1.
		}
		if (dayinyear < monthlimit) {
			break;
		}
		*month = i;
		*day = dayinyear - monthlimit;
	}
}

uint32_t secondssince2000(uint8_t year2digits) {
	uint32_t seconds = (365L*60L*60L*24L)*year2digits;
	//add 60*60*24 for each leap year passed
	uint8_t leapyears = (year2digits + 3) /4;
	leapyears -= year2digits / 100;
//	leapyears += year2digits / 400; cant happen because only two digits are given
	seconds += (uint32_t)leapyears*60L*60L*24L;
	return seconds;
}

/*
  input: seconds since 1.1.2000 0:0
  output: years since 1.1.2000 -> 0:2000 1:2001 etc
          day of year: 0: First day, 1: second day...
*/
uint8_t yearsince2000(uint32_t seconds, uint16_t * dofy) {
	uint16_t days = seconds / (60L*60L*24L); //time in days
	//years = days/365.2425
	uint8_t years = (uint32_t)days * 10000L / 3652425L;
	if (dofy) {
		uint16_t tdofy = days - (uint32_t)years * 3652425L / 10000L;
		if (years) {
			tdofy--; //year 2000 is a leapyear too
		}
		*dofy = tdofy;
	}
	return years;
}

//0 = monday, 6 = sunday.
static uint8_t calcweekday(uint16_t dayofyear, uint8_t yeartwodigit) {
	//calc weekday
	uint16_t weekday = yeartwodigit + dayofyear;
	if (yeartwodigit > 0) {
		weekday += (yeartwodigit-1)/4 + 1; //+1 because year of reference (2000) is a leapyear
	}
	//Usually saturday would be = 0 (because 1.1.2000 is a saturday), however since saturday 6th day (5) of week, add 5.
	weekday += 5;
	weekday %= 7;
	return weekday;
}

//gets the weekday from seconds timestamp since 1.1.2000. 0 = moday, 6 = sunday
uint8_t calcweekdayfromtimestamp(uint32_t timestamp2000) {
	uint16_t dofy;
	uint8_t y2digit = yearsince2000(timestamp2000, &dofy);
	return calcweekday(dofy, y2digit);
}

static uint32_t secondsSinceYearBeginning(uint8_t day, uint8_t month, uint16_t year) {
	return dayofyear(day, month, year) * 60*60*24;
}

//returns 1 if the time should be the summer time.
//if the state cant be determined, wasSummertime is returned
uint8_t isSummertime(uint32_t timestamp2000, uint8_t wasSummertime) {
/*the rule:
  a) Last sunday of march, it should be summer time after 2:00
  b) Last sunday of october, it should be winter time after 3:00, so
     the time between 2 and 3 can be undefined without additional information,
     in this case wasSummertime is returned to avoid switching winter and summer
     time back and forth
*/
	//0. get current year
	uint16_t dofy;
	uint8_t y2digit = yearsince2000(timestamp2000, &dofy);
	uint32_t timestampFirstDayInYear = secondssince2000(y2digit);
	//1. get timestamp for last day of march and october
	uint32_t timestampLastDayMarch = timestampFirstDayInYear + secondsSinceYearBeginning(30, 2, y2digit);
	uint32_t timestampLastDayOctober = timestampFirstDayInYear + secondsSinceYearBeginning(30, 9, y2digit);
	//2. get day in week of last days in months
	uint8_t dayinweekLastDayMarch = calcweekdayfromtimestamp(timestampLastDayMarch);
	uint8_t dayinweekLastDayOctober = calcweekdayfromtimestamp(timestampLastDayOctober);
	//3. get last sunday in month
	uint8_t lastSundayMarch = 30; //proper if last day of month = sunday
	if (dayinweekLastDayMarch < 6) //if not a sunday
		lastSundayMarch -= dayinweekLastDayMarch + 1; //monday = 0 -> substract one -> get sunday, tuesday = 1 -> ubstract two -> get sunday
	uint8_t lastSundayOctober = 30; //proper if last day of month = sunday
	if (dayinweekLastDayOctober < 6) //if not a sunday
		lastSundayOctober -= dayinweekLastDayOctober + 1;
	//4. get new timestamp of last sundays
	uint32_t timestampLastSundayMarch = timestampFirstDayInYear + secondsSinceYearBeginning(lastSundayMarch, 2, y2digit);
	uint32_t timestampLastSundayOctober = timestampFirstDayInYear + secondsSinceYearBeginning(lastSundayOctober, 9, y2digit);
	//5. offset to 2:00
	uint32_t summertimestart = timestampLastSundayMarch + 60*60*2;
	uint32_t summertimestop = timestampLastSundayOctober + 60*60*2;
	uint32_t wintertimestart = summertimestop + 60*60; //one hour later 3:00
	if (timestamp2000 < summertimestart) {
		return 0; //wintertime at beginning of the year
	}
	if (timestamp2000 >= wintertimestart) {
		return 0; //wintertime at end of the year
	}
	if (timestamp2000 < summertimestop) {
		return 1; //summertime
	}
	return wasSummertime; //undefined hour.
}

