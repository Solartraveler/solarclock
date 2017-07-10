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
*/

#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "../finecalib.h"
#include "../config.h"

int8_t g_direction;

void rtc_finecalib(int8_t direction) {
	g_direction = direction;
}

int testfinecalib1(int16_t timeCalib,           int16_t acu,         int32_t cycRound,
                    int8_t expectedDir, int16_t expectedAcu, int32_t expectedCycRound) {
	g_settings.timeCalib = timeCalib;
	g_state.accumulatedErrorCycles = acu;
	g_state.badCyclesRoundingError = cycRound;
	g_direction = 0;
	updateFineCalib();
	if ((g_direction != expectedDir) || (g_state.accumulatedErrorCycles != expectedAcu) || (g_state.badCyclesRoundingError != expectedCycRound)) {
		printf("Testcase testfinecalib1(%i, %i, %i) failed. Time Calib should %i is %i. Acu should %i is %i. CycRound should %i is %i\n",
		       timeCalib, acu, cycRound, expectedDir, g_direction, expectedAcu, g_state.accumulatedErrorCycles, expectedCycRound, g_state.badCyclesRoundingError);
		return 1;
	}
	return 0;
}

int testfinecalib(void) {
	int errors = 0;
	errors += testfinecalib1(0, 0, 0, 0, 0, 0);
	errors += testfinecalib1(0, 100, 123, 0, 100, 123);
	errors += testfinecalib1(1, 0, 0, 0, 0, 32768); //goes 1ms/day too fast
	errors += testfinecalib1(1000, 0, 0, 0, 22, 1088000); //goes 1s/day too fast
	errors += testfinecalib1(10000, 0, 0, 0, 227, 800000); //goes 10s/day too fast
	errors += testfinecalib1(20000, 0, 0, 0, 455, 160000); //goes 20s/day too fast
	errors += testfinecalib1(10000, 227, 800000, 0, 455, 160000); //goes 10s/day too fast
	errors += testfinecalib1(10000, 455, 160000, 1, 202, 960000); //goes 10s/day too fast
	errors += testfinecalib1(-10000, 0, 0, 0, -227, -800000); //goes -10s/day too slow
	errors += testfinecalib1(-10000, -455, -160000, -1, -202, -960000); //goes 10s/day too fast
	return errors;
}
