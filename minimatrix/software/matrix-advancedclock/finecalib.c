/* Matrix-Advancedclock
  (c) 2014-2017 by Malte Marwedel
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


 This file is separate from main.c to make testing easy.

*/

#include <stdio.h>
#include <stdint.h>

#include "config.h"
#include "displayRtc.h"

#ifndef UNIT_TEST

#include "main.h"
#include "rs232.h"

#else

#include "testcases/pccompat.h"

#endif

//cycles the rct will be doing more or less per minute when rtc_finecalib != 0
//CORRECTINGCYCLES = 32768 / 32768/4095*60 = 480
#define CORRECTINGCYCLES (F_RTC / DISP_RTC_PER * 60)

/*
Maximum increment or decerement per minute:
+-20s allowed calibration for 24h:
32768cyc/s*+-20s=+-655360cyc = +-0.023%
+-655360cyc/24/60 = +-455cyc/min  - correcting F_RTC cycles every minute
A 20ppm crystal may require +-0.02% calibration -> fits

As once clock has an error of 22sec/day -> increment allowed calibration
range to +-30s.

*/

//expected to be called every minute
void updateFineCalib(void) {
	int32_t thousandBadCyclesDay = (int32_t)g_settings.timeCalib * F_RTC + g_state.badCyclesRoundingError; //negative value -> too much cycles, positive -> missing cycles
	g_state.badCyclesRoundingError = thousandBadCyclesDay % (1000L*24L*60L);
	int32_t badCyclesMinute = thousandBadCyclesDay / (1000L*24L*60L);
	int16_t aec = g_state.accumulatedErrorCycles + badCyclesMinute;
	if (g_settings.debugRs232 == 0xB) {
		char buffer[DEBUG_CHARS+1];
		buffer[DEBUG_CHARS] = '\0';
		snprintf_P(buffer, DEBUG_CHARS, PSTR("badCyc=%li, roundE=%li, aec=%i\r\n"), (long int)thousandBadCyclesDay, (long int)g_state.badCyclesRoundingError, aec);
		rs232_sendstring(buffer);
	}
	//printf("thousandBadCyclesDay=%i, roundingError=%i, minute=%i, aec=%i\n", thousandBadCyclesDay, g_state.badCyclesRoundingError, badCyclesMinute, aec);
//	printf("CORRECTINGCYCLES=%i, F_RTC=%i, DIP_RTC_PER=%i\n", CORRECTINGCYCLES, F_RTC, DISP_RTC_PER);
	if (aec <= (-CORRECTINGCYCLES)) {
		if (g_settings.debugRs232 == 0xB) {
			rs232_sendstring_P(PSTR("Slow down\r\n"));
		}
		if (aec <= (-CORRECTINGCYCLES*2)) {
			rtc_finecalib(2); //slow down a lot
			aec += CORRECTINGCYCLES*2;
		} else {
			rtc_finecalib(1); //slow down
			aec += CORRECTINGCYCLES;
		}
	} else if (aec >= CORRECTINGCYCLES) {
		if (g_settings.debugRs232 == 0xB) {
			rs232_sendstring_P(PSTR("Speed up\r\n"));
		}
		if (aec >= CORRECTINGCYCLES*2) {
			rtc_finecalib(-2); //speed up a lot
			aec -= CORRECTINGCYCLES*2;
		} else {
			rtc_finecalib(-1); //speed up
			aec -= CORRECTINGCYCLES;
		}
	} else {
		rtc_finecalib(0); //ideal speed
	}
	g_state.accumulatedErrorCycles = aec;
}
