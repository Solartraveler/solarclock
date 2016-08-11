/* Matrix-Simpleclock
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

#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "../dcf77statisticsdecode.h"
#include "makedata.h"

typedef struct {
	uint8_t hour;
	uint8_t minute;
	uint8_t day;
	uint8_t month;
	uint16_t year;
	float errorrate;
	uint8_t outcomecode;
} testdate_t;


const testdate_t g_testdates[] = {
{11, 23, 24, 12, 2015, 0.0, 2},
{16, 58, 8,   9, 2010, 0.0, 2},
{8, 15,  31,  1, 2016, 0.05, 2},
{23, 58, 6,   5, 2044, 0.08, 1},
{6, 6,   6,   6, 2066, 0.10, 2},
{23, 42, 1,   2, 2034, 0.15, 2},
{11, 1, 11,  11, 2011, 0.20, 2},
{10, 57, 5,   1, 2020, 0.25, 2},
{15, 25, 1,   4, 2025, 0.30, 2},
{17, 18, 19, 11, 2030, 0.35, 2},
{19, 15, 9,   8, 2076, 0.40, 2},
{15, 56, 2,   2, 2022, 0.45, 2},
{8, 40, 13,   3, 2013, 0.50, 2}
};


int testdate(const testdate_t * date) {
	uint8_t data[SECONDSINMINUTE*DCF77DATAMINUTES];
	uint8_t datamod[DCF77DIAGRAMSIZE*DCF77DATAMINUTES];
	//1. generate test array
	makedataall(data, date->errorrate, date->year, date->month, date->day, date->hour, date->minute);
	for (int i = 0; i < DCF77DATAMINUTES; i++) {
		for (int j = 0; j < DCF77DIAGRAMSIZE; j++) {
			datamod[i*DCF77DIAGRAMSIZE + j] = data[DCF77SHORTENED + i*SECONDSINMINUTE + j];
			//printf("%c", datamod[i*DCF77DIAGRAMSIZE + j] + 'A');
		}
		//printf("\n");
	}
	//2. let it be analzyed
	uint8_t minute = 0, hour = 0, day = 0, month = 0, year2digit = 0;
	uint16_t errorrate = 0;
	uint8_t result = dcf77_statisticsdecode(datamod, &minute, &hour, &day, &month, &year2digit, &errorrate);
	//3. compare result
	int errors = 0;
	if (result != date->outcomecode) errors++;
	if (result > 0) {
		if (minute != date->minute) errors++;
		if (hour != date->hour) errors++;
		if (result > 1) {
			if (day != date->day) errors++;
			if (month != date->month) errors++;
			if ((year2digit + 2000) != date->year) errors++;
		}
	}
	if (errors) {
		printf("Testcase %02i:%02i %02i.%02i.%04i res:%i failed\n", date->hour, date->minute, date->day, date->month, date->year, date->outcomecode);
		printf("  got    %02i:%02i %02i.%02i.%04i res:%i, error: %i\n", hour, minute, day, month, year2digit+2000, result, errorrate);
		return 1;
	}
	return 0;
}

int testdcf77decoder(void) {
	int errors = 0;
	unsigned int i;
	for (i = 0; i < (sizeof(g_testdates)/sizeof(testdate_t)); i++) {
		errors += testdate(&(g_testdates[i]));
	}
	return errors;




}
