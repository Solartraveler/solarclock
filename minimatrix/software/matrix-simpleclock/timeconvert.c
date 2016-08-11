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

#include <stdint.h>

const uint16_t g_dayoffset[12] = {0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334};

static uint8_t isleapyear(uint16_t year) {
	if ((((year % 4) == 0) && !(year % 100 == 0)) || ((year % 400) == 0)) {
		return 1;
	}
	return 0;
}

uint16_t dayofyear(uint8_t day, uint8_t month, uint16_t year) {
	if (month < 12) {
		uint16_t dayofyear = g_dayoffset[month]+day;
		if (isleapyear(year)) {
			if (month > 2) {
				dayofyear++;
			}
		}
		return dayofyear;
	}
	return 0;
}

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
