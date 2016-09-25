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

#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "../timeconvert.h"

int testdayofyear(uint8_t day, uint8_t month, uint16_t year, uint16_t dayofyearshould) {
	uint16_t dayofyearis = dayofyear(day-1, month-1, year) + 1;
	if (dayofyearis != dayofyearshould) {
		printf("Testcase dayofyear %02i.%02i.%04i failed. Should :%i is :%i\n", day, month, year, dayofyearshould, dayofyearis);
		return 1;
	}
	return 0;
}

int testmonthdayfromdayinyear(uint16_t dayinyear, uint16_t year, uint8_t monthshould, uint8_t dayshould) {
	uint8_t monthis, dayis;
	monthdayfromdayinyear(dayinyear-1, year, &monthis, &dayis);
	monthis++;
	dayis++;
	if ((monthis != monthshould) || (dayis != dayshould)) {
		printf("Testcase monthdayfromdayinyear %03i %04i failed. Should :%i.%i, is :%i.%i\n", dayinyear, year, dayshould, monthshould, dayis, monthis);
		return 1;
	}
	return 0;
}

int testsecondssince2000(uint16_t year, uint32_t secondsshould) {
	year -= 2000;
	uint32_t secondsis = secondssince2000(year);
	if (secondsshould != secondsis) {
		printf("Testcase secondssince2000 %u failed. should: %u is %u\n", year, secondsshould, secondsis);
		return 1;
	}
	return 0;
}

int testyearsince2000(uint32_t timestamp, uint16_t yearshould, uint16_t doyshould) {
	uint16_t doyis;
	uint16_t yearis = 2000 + yearsince2000(timestamp, &doyis);
	doyis++;
	if ((doyis != doyshould) || (yearis != yearshould)) {
		printf("Testcase yearsince2000 %u failed. should: %u,%u is %u, %u\n", timestamp, yearshould, doyshould, yearis, doyis);
		return 1;
	}
	return 0;
}
int testcalcweekdayfromtimestamp(uint32_t timestamp2000, uint8_t dowshould) {
	uint8_t dowis = calcweekdayfromtimestamp(timestamp2000) + 1;
	if (dowshould != dowis) {
		printf("Testcase calcweekdayfromtimestamp %u failed. Should: %i, is: %i\n", timestamp2000, dowshould, dowis);
		return 1;
	}
	return 0;
}

int testtimeconvert(void) {
	int errors = 0;
	//day of year
	errors += testdayofyear(1,1,2000, 1);
	errors += testdayofyear(2,1,2000, 2);
	errors += testdayofyear(29,2,2016, 60);
	errors += testdayofyear(28,2,2017, 59);
	errors += testdayofyear(1,3,2000, 61);
	errors += testdayofyear(1,3,2008, 61);
	errors += testdayofyear(1,3,2015, 60);
	errors += testdayofyear(31,12,2000, 366);
	errors += testdayofyear(31,12,2001, 365);
	errors += testdayofyear(31,12,2004, 366);
	errors += testdayofyear(4,9,2016, 248);
	//month day from day in year
	errors += testmonthdayfromdayinyear(1, 2000, 1, 1);
	errors += testmonthdayfromdayinyear(366, 2000, 12, 31);
	errors += testmonthdayfromdayinyear(59, 2005, 2, 28);
	errors += testmonthdayfromdayinyear(60, 2000, 2, 29);
	errors += testmonthdayfromdayinyear(61, 2000, 3, 1);
	errors += testmonthdayfromdayinyear(60, 2007, 3, 1);
	errors += testmonthdayfromdayinyear(365, 2010, 12, 31);
	//seconds since the year 2000
	errors += testsecondssince2000(2000, 0);
	errors += testsecondssince2000(2001, 366*24*60*60);
	errors += testsecondssince2000(2002, 366*24*60*60 + 365*24*60*60);
	errors += testsecondssince2000(2004, 366*24*60*60 + 365*24*60*60*3);
	errors += testsecondssince2000(2005, 366*24*60*60*2 + 365*24*60*60*3);
	errors += testsecondssince2000(2016, 366*24*60*60*4 + 365*24*60*60*12);
	//yearsince2000
	errors += testyearsince2000(0, 2000, 1);
	errors += testyearsince2000(366*24*60*60-1, 2000, 366);
	errors += testyearsince2000(366*24*60*60, 2001, 1);
	errors += testyearsince2000(366*24*60*60 + 365*24*60*60-1, 2001, 365);
	errors += testyearsince2000(366*24*60*60 + 365*24*60*60, 2002, 1);
	//calculate the day of the week
	errors += testcalcweekdayfromtimestamp(0, 6);
	errors += testcalcweekdayfromtimestamp(1*24*60*60-1, 6);
	errors += testcalcweekdayfromtimestamp(1*24*60*60, 7);
	errors += testcalcweekdayfromtimestamp(7*24*60*60, 6);
	errors += testcalcweekdayfromtimestamp(8*24*60*60, 7);
	errors += testcalcweekdayfromtimestamp(9*24*60*60, 1);
	errors += testcalcweekdayfromtimestamp(10*24*60*60, 2);
	errors += testcalcweekdayfromtimestamp(11*24*60*60, 3);
	errors += testcalcweekdayfromtimestamp(366*24*60*60, 1); //1.1.2001
	errors += testcalcweekdayfromtimestamp(366*24*60*60 + 365*24*60*60, 2); //1.1.2002
	errors += testcalcweekdayfromtimestamp(secondssince2000(16), 5); //1.1.2016
	errors += testcalcweekdayfromtimestamp(secondssince2000(16) + 24*60*60, 6); //2.1.2016
	errors += testcalcweekdayfromtimestamp(secondssince2000(16) + 247*24*60*60, 7); //4.9.2016
	errors += testcalcweekdayfromtimestamp(secondssince2000(16) + ((uint32_t)dayofyear(4-1, 9-1, 2016))*24*60*60, 7);
	return errors;
}
