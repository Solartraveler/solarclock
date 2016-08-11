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

#include "../config.h"
#include "../charger.h"


typedef int testfunc(void);

//#define DEBUG

#ifdef DEBUG
#define dbgprint printf
#else
#define dbgprint(...)
#endif

//---------------- helper functions ----------------------

int g_chargerstate = -1;
int g_nextvoltage;
int g_nextcurrent;

#define CHECK(X) if (!(X)) { printf("Failed at %s:%i\n", __FUNCTION__, __LINE__); return 1;}


void charger_enable(void) {
	g_chargerstate = 1;
}

void charger_disable(void) {
	g_chargerstate = 0;
}

void update_voltageAndCurrent(void) {
	g_state.chargerCurrent = g_nextcurrent;
	g_state.batVoltage= g_nextvoltage;
}

void tc_runseconds(int seconds, int consumption) {
	while (seconds) {
		g_state.consumption += consumption;
		charger_update();
		seconds--;
	}
}

//-------------- testfunctions ----------------------


int tc_slowdischarge(void) {
	g_settings.batteryCapacity = 800; //[mAh]
	g_state.batteryCharged = g_settings.batteryCapacity * 60 * 60; //[mAs]
	const unsigned int consumption = 300; //[µA]
	g_nextvoltage = 2400;//mV
	int secondsuntilempty = ((unsigned long long)g_settings.batteryCapacity * 60 * 60 * 1000) / consumption;
	tc_runseconds(secondsuntilempty - 1, consumption);
	//now there should be the capacity for one second left in the battery
	dbgprint("tc_slowdischarge: cap left: %umAs\n", g_state.batteryCharged);
	CHECK(g_state.batteryCharged*1000 == consumption*60); //engery left for last minute
	tc_runseconds(100, consumption);
	CHECK(g_state.batteryCharged == 0);
	CHECK(g_chargerstate == 1);
	return 0;
}

int tc_fastdischarge(void) {
	g_settings.batteryCapacity = 800; //[mAh]
	g_state.batteryCharged = g_settings.batteryCapacity * 60 * 60; //[mAs]
	const unsigned int consumption = 20000; //[µA]
	g_nextvoltage = 2400;//mV
	int secondsuntilempty = ((unsigned long long)g_settings.batteryCapacity * 60 * 60 * 1000) / consumption;
	dbgprint("tc_fastdischarge: Battery will last: %is\n", secondsuntilempty);
	tc_runseconds(secondsuntilempty - 1, consumption);
	//now there should be the capacity for one second left in the battery
	dbgprint("tc_fastdischarge: cap left: %umAs\n", g_state.batteryCharged);
	CHECK(g_state.batteryCharged * 1000 == consumption * 60); //energy left for last minute
	tc_runseconds(100, consumption);
	CHECK(g_state.batteryCharged == 0);
	CHECK(g_chargerstate == 1);
	return 0;
}

int tc_batteryempty(void) {
	g_settings.batteryCapacity = 800; //[mAh]
	g_state.batteryCharged = (g_settings.batteryCapacity-1) * 60 * 60; //[mAs]
	const int consumption = 20000; //[µA]
	g_nextvoltage = 1700;//mV
	tc_runseconds(1, consumption);
	CHECK(g_state.batteryCharged == 0);
	tc_runseconds(1, consumption); //charger needs a cycle to update the Charged value
	CHECK(g_chargerstate == 1);
	CHECK(g_state.batteryCharged == 0);
	return 0;
}

int tc_overtemperatureAutoOff(void) {
	g_settings.batteryCapacity = 800; //[mAh]
	g_state.batteryCharged = g_settings.batteryCapacity * 60 * 60 / 2; //[mAs], half filled
	const int consumption = 20000; //[µA]
	g_nextvoltage = 2400;//mV
	tc_runseconds(1, consumption);
	CHECK(g_chargerstate == 1);
	g_state.gradcelsius10 = 350; //35°C
	tc_runseconds(1, consumption);
	CHECK(g_chargerstate == 0);
	return 0;
}

int tc_overtemperatureManualOff(void) {
	g_settings.batteryCapacity = 800; //[mAh]
	g_settings.chargerMode = 2; //always on
	g_state.batteryCharged = g_settings.batteryCapacity * 60 * 60 / 2; //[mAs], half filled
	const int consumption = 20000; //[µA]
	g_nextvoltage = 2400;//mV
	tc_runseconds(1, consumption);
	CHECK(g_chargerstate == 1);
	g_state.gradcelsius10 = 350; //35°C
	tc_runseconds(1, consumption);
	CHECK(g_chargerstate == 0);
	return 0;
}

int tc_overvoltageAutoOff(void) {
	g_settings.batteryCapacity = 800; //[mAh]
	g_state.batteryCharged = g_settings.batteryCapacity * 60 * 60 / 2; //[mAs], half filled
	const int consumption = 20000; //[µA]
	g_nextvoltage = 2400;//mV
	tc_runseconds(1, consumption);
	CHECK(g_chargerstate == 1);
	g_nextvoltage = 3500;//mV
	tc_runseconds(1,consumption);
	CHECK(g_chargerstate == 0);
	g_nextvoltage = 3000;//mV
	tc_runseconds(30, consumption);
	CHECK(g_chargerstate == 0); //stay off for some time
	tc_runseconds(60, consumption);
	CHECK(g_chargerstate == 1); //restart if voltage is good again
	return 0;
}

int tc_overvoltageManualOff(void) {
	g_settings.batteryCapacity = 800; //[mAh]
	g_settings.chargerMode = 2; //always on
	g_state.batteryCharged = g_settings.batteryCapacity * 60 * 60 / 2; //[mAs], half filled
	const int consumption = 20000; //[µA]
	g_nextvoltage = 2400;//mV
	tc_runseconds(1, consumption);
	CHECK(g_chargerstate == 1);
	g_nextvoltage = 3500;//mV
	tc_runseconds(1, consumption);
	CHECK(g_chargerstate == 0);
	g_nextvoltage = 3000;//mV
	tc_runseconds(1, consumption);
	CHECK(g_chargerstate == 1); //no long wait if in manual mode
	return 0;
}


int tc_overcurrentAutoOff(void) {
	g_settings.batteryCapacity = 800; //[mAh]
	g_state.batteryCharged = g_settings.batteryCapacity * 60 * 60 / 2; //[mAs], half filled
	const int consumption = 20000; //[µA]
	g_nextvoltage = 2400;//[mV]
	tc_runseconds(1, consumption);
	CHECK(g_chargerstate == 1);
	g_nextcurrent = 250;//[mA]
	tc_runseconds(1, consumption);
	CHECK(g_chargerstate == 0);
	g_nextcurrent = 0;//[mA]
	tc_runseconds(30, consumption);
	CHECK(g_chargerstate == 0); //stay off for some time
	tc_runseconds(60, consumption);
	CHECK(g_chargerstate == 1); //restart if cuurent is good again
	return 0;
}

int tc_overcurrentManualOff(void) {
	g_settings.batteryCapacity = 800; //[mAh]
	g_settings.chargerMode = 2; //always on
	g_state.batteryCharged = g_settings.batteryCapacity * 60 * 60 / 2; //[mAs], half filled
	const int consumption = 20000; //[µA]
	g_nextvoltage = 2400;//[mV]
	tc_runseconds(1, consumption);
	CHECK(g_chargerstate == 1);
	g_nextcurrent = 450; //[mA]
	tc_runseconds(1, consumption);
	CHECK(g_chargerstate == 0);
	g_nextcurrent = 0; //[mA]
	tc_runseconds(1, consumption);
	CHECK(g_chargerstate == 1); //no long wait if in manual mode
	return 0;
}

int tc_selfconsume(void) {
	g_settings.batteryCapacity = 800; //[mAh]
	g_settings.chargerMode = 2; //always on
	g_state.batteryCharged = g_settings.batteryCapacity * 60 * 60 / 2; //[mAs], half filled
	unsigned int startcharged = g_state.batteryCharged;
	const int consumption = 10000; //[µA]
	g_nextvoltage = 2400;//[mV]
	g_nextcurrent = 10; //[mA]
	tc_runseconds(60*60*24, consumption); //24[h]
	dbgprint("tc_selfconsume: start: %umAs, now: %umAs\n", startcharged, g_state.batteryCharged);
	CHECK(g_chargerstate == 1);
	CHECK(g_state.batteryCharged == startcharged);
	return 0;
}

int tc_slowcharge(void) {
	g_settings.batteryCapacity = 800; //[mAh]
	g_settings.chargerMode = 0; //auto
	g_state.batteryCharged = g_settings.batteryCapacity * 60 * 60 / 2; //[mAs], half filled
	unsigned int startcharged = g_state.batteryCharged;
	const int consumption = 10000; //[µA]
	g_nextvoltage = 2400;//[mV]
	g_nextcurrent = 90; //[mA] 80mA for charger
	unsigned int timetocharge = 60 * 60 *8;
	//expected to need 8 hours for 50% charging
	tc_runseconds(timetocharge-1, consumption); //8[h] -1[s]
	dbgprint("tc_slowcharge: charged: %umAs, expected < : %umAs\n", g_state.batteryCharged, startcharged * 2);
	CHECK(g_state.batteryCharged < startcharged * 2);
	CHECK(g_chargerstate == 1);
	tc_runseconds(1, consumption); //missing second
	unsigned int expectfull = startcharged * 2;
	unsigned int expect = expectfull * 97 / 100; //97%
	dbgprint("tc_slowcharge: charged: %umAs, expected >= : %umAs\n", g_state.batteryCharged, expect);
	CHECK(g_state.batteryCharged >= expect); //can get a little lower due efficiency value rounding 97% accurate
	unsigned int i;
	dbgprint("tc_slowcharge: max to add for rounding: %i\n", timetocharge * 4 / 100);
	for (i = 0; i < (timetocharge * 5 / 100); i++) { //might not be more than 4% left
		tc_runseconds(1, consumption);
		if (g_state.batteryCharged >= expectfull) {
			break;
		}
	}
	dbgprint("tc_slowcharge: charged: %umAs, expected >= : %umAs\n", g_state.batteryCharged, expectfull);
	CHECK(g_state.batteryCharged >= expectfull);
	tc_runseconds(1, consumption); //let charged values propagate
	CHECK(g_chargerstate == 0);
	g_nextcurrent = 0; //now the charger is off
	for (i = 0; i < (60*10); i++) { //decrease overcharged
		tc_runseconds(1, consumption);
		if (g_state.batteryCharged <= expectfull) {
			break;
		}
	}
	CHECK(g_state.batteryCharged <= expectfull);
	tc_runseconds(60*59, consumption);
	CHECK(g_chargerstate == 0);
	tc_runseconds(60*2, consumption);
	CHECK(g_chargerstate == 1); //enable charger again after one hour
	return 0;
}

int tc_fastcharge(void) {
	g_settings.batteryCapacity = 360; //[mAh]
	g_settings.chargerMode = 0; //auto
	g_state.batteryCharged = 0; //[mAs] battery empty
	unsigned int maxcharged = g_settings.batteryCapacity * 60 * 60; //[mAs]
	const int consumption = 10000; //[uAs]
	g_nextvoltage = 2400;//[mV]
	g_nextcurrent = 190; //[mA] 180mA for charger
	//expected to need 2 hours for full charging
	tc_runseconds(60*60*2-1, consumption); //2[h] -1[s]
	CHECK(g_state.batteryCharged < maxcharged);
	CHECK(g_chargerstate == 1);
	tc_runseconds(1, consumption); //missing second
	dbgprint("tc_fastcharge: charged: %umAs, expected: %umAs\n", g_state.batteryCharged, maxcharged);
	CHECK(g_state.batteryCharged == maxcharged);
	tc_runseconds(1, consumption); //let chared values propagate
	CHECK(g_chargerstate == 0); //disabling the charger due full charge is delayed by a minsecond because of order of update
	g_nextcurrent = 0; //now the charger is off
	tc_runseconds(60*59, consumption);
	CHECK(g_chargerstate == 0);
	tc_runseconds(60*2, consumption);
	CHECK(g_chargerstate == 1); //enable charger again after one hour
	return 0;
}

int tc_alwaysoff(void) {
	g_settings.batteryCapacity = 800; //[mAh]
	g_settings.chargerMode = 1; //always off
	g_state.batteryCharged = g_settings.batteryCapacity * 60 * 60 / 10; //[mAs], 10% filled
	const int consumption = 20000; //[µA]
	g_nextvoltage = 1800;//[mV]
	g_nextcurrent = 0; //[mA]
	tc_runseconds(60*60*24, consumption); //24[h]
	CHECK(g_chargerstate == 0);
	CHECK(g_state.batteryCharged == 0);
	return 0;
}

int tc_gotoautofrommanualmode(void) {
	g_settings.batteryCapacity = 400; //[mAh]
	g_settings.chargerMode = 2; //always on
	g_state.batteryCharged = g_settings.batteryCapacity * 60 * 60; // full
	const int consumption = 0; //[µA]
	g_nextvoltage = 2400;//[mV]
	g_nextcurrent = 200; //[mA]
	unsigned int overcharged = g_state.batteryCharged * 2;
	tc_runseconds(60*60*4+1, consumption); //4[h]+1s
	dbgprint("tc_gotoautofrommanualmode: charged: %umAs, expected: %umAs\n", g_state.batteryCharged, overcharged);
	CHECK(g_chargerstate == 0);
	CHECK(g_state.batteryCharged >=  overcharged);
	CHECK(g_settings.chargerMode == 0); //had switched back to auto mode
	return 0;
}

testfunc * chargertests[] = {&tc_slowdischarge, &tc_fastdischarge,
  &tc_batteryempty,
  &tc_overtemperatureAutoOff, &tc_overtemperatureManualOff,
  &tc_overvoltageAutoOff, &tc_overvoltageManualOff,
  &tc_overcurrentAutoOff, &tc_overcurrentManualOff,
  &tc_selfconsume, &tc_slowcharge, &tc_fastcharge,
  &tc_alwaysoff, &tc_gotoautofrommanualmode
};

int testcharger(void) {
	int errors = 0;
	unsigned int i;
	for (i = 0; i < (sizeof(chargertests)/sizeof(testfunc*)); i++) {
		memset(&g_settings, 0, sizeof(g_settings));
		memset(&g_state, 0, sizeof(g_state));
		g_nextvoltage = 0;
		g_nextcurrent = 0;
		errors += chargertests[i]();
	}
	return errors;
}
