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

#include <stdio.h>
#include <stdint.h>

#ifndef UNIT_TEST

#include <avr/io.h>
#include <avr/wdt.h>
#include <avr/pgmspace.h>

#include "adc.h"
#include "main.h"
#include "menu-interpreter.h"
#include "rs232.h"
#include "logger.h"

#else

#include "testcases/pccompat.h"

void update_voltageAndCurrent(void);
void charger_enable(void);
void charger_disable(void);

#endif

#include "charger.h"
#include "config.h"

#define CHARGER_MV_MAX 3400
#define CHARGER_MA_MAX 200
#define CHARGER_MA_MANUAL_MAX 400
#define CHARGER_MV_BAT_LOW 2300

#ifndef UNIT_TEST

void charger_calib(void) {
	uint32_t v1 = 0;
	uint32_t v2 = 0;
	uint32_t i;
	PORTA.PIN0CTRL = PORT_OPC_TOTEM_gc | PORT_ISC_INPUT_DISABLE_gc;
	PORTA.PIN7CTRL = PORT_OPC_TOTEM_gc | PORT_ISC_INPUT_DISABLE_gc;
	adca_startup();
	waitms(1);
	uint16_t results[4];
	//with *4096, max 256 12bit samples may be collected
	for (i = 0; i < 125; i++) {
		adca_getQuad(0, 7, 0, 7, ADC_REFSEL_INT1V_gc, results);
		//v1 += adca_get2(0, ADC_REFSEL_INT1V_gc); //positive input
		//v2 += adca_get2(7, ADC_REFSEL_INT1V_gc); //negative input
		if (results[0] > v1) {
			v1 = results[0];
		}
		if (results[1] > v2) {
			v2 = results[1];
		}
		if (results[2] > v1) {
			v1 = results[2];
		}
		if (results[3] > v2) {
			v2 = results[3];
		}
		wdt_reset();
	}
	//int16_t d = adca_getDelta(7, 0, ADC_REFSEL_INT1V_gc, ADC_CH_GAIN_1X_gc);
/*
	adca_getDelta(7, 0, ADC_REFSEL_VCC_gc, ADC_CH_GAIN_1X_gc);
	adca_getDelta(7, 0, ADC_REFSEL_VCC_gc, ADC_CH_GAIN_1X_gc);
	int16_t d = adca_getDelta(7, 0, ADC_REFSEL_VCC_gc, ADC_CH_GAIN_1X_gc);
*/
	adca_stop();
	g_state.chargerResistoroffset = v1*4096/v2;
	if (g_settings.debugRs232) {
		char buffer[DEBUG_CHARS+1];
		buffer[DEBUG_CHARS] = '\0';
		snprintf_P(buffer, DEBUG_CHARS, PSTR("Charger resistor offset: %lu/4096\r\n"), g_state.chargerResistoroffset);
		rs232_sendstring(buffer);
	}
}

static void update_voltageAndCurrent(void) {
	uint8_t i;
	int32_t v1 = 0;
	int32_t v1adj;
	int32_t v2 = 0;
	PORTA.PIN0CTRL = PORT_OPC_TOTEM_gc | PORT_ISC_INPUT_DISABLE_gc;
	PORTA.PIN7CTRL = PORT_OPC_TOTEM_gc | PORT_ISC_INPUT_DISABLE_gc;
	adca_startup();
	waitms(1);
	uint16_t results[4];
	for (i = 0; i < 32; i++) {
		adca_getQuad(0, 7, 0, 7, ADC_REFSEL_INT1V_gc, results);
		//adca_getQuad(0, 7, 0, 7, ADC_REFSEL_VCC_gc, results);
		//adca_getDelta(ADC_CH_MUXPOS_PIN0_gc, ADC_CH_MUXNEG_PIN7_gc, ADC_REFSEL_INT1V_gc, ADC_CH_GAIN_1X_gc);
		//v1 += results[0] + results[2];
		//v2 += results[1] + results[3];
		if (results[0] > v1) {
			v1 = results[0];
		}
		if (results[1] > v2) {
			v2 = results[1];
		}
		if (results[2] > v1) {
			v1 = results[2];
		}
		if (results[3] > v2) {
			v2 = results[3];
		}
	}
	adca_stop();
	//v1 *= 16; //for simulating 16 samples only
	//v2 *= 16;
	/* resistor divider:

WRONG calculations:
    smd code 1003  = 100kOhm and 224 = 220kOhm -> voltage divider = 10/33.
    vref = 1V*10/11 -> 0.000222V per digit
    scale by 33/10 -> 0.0007326V per digit
    exact scaling in V: result * 1/1365
    using 16 samples and wanting mV: * 25/546
    Calibrated resistor: 97,8mV @ 101mA = 0.968Ohm (1Ohm nominal value)
    new scaling: *101/97.8 -> = 0.0472857
    -> aprox 591/12500 because of 16 samples

RIGHT calculations:
    smd code 1003  = 100kOhm and 224 = 220kOhm -> voltage divider = 10/32.
    vref = 10/11 bandgap (1.1V) = 1.0V -> 0.0002442V per digit
    scale by 32/10 -> 0.00078144V per digit
    exact scaling in V: result * 16/20475
    using 16 samples and wanting mV: * 40/819
    Calibrated resistor: 97,8mV @ 101mA = 0.968Ohm (1Ohm nominal value)
    new scaling: *101/97.8 -> = 0.050438
    -> aprox 630/12500-> because of 16 samples

But the wrong values simply produce better results...

New resistors: 100kOhm and 360kOhm -> voltage divider = 10/46
    vref = 10/11 bandgap (1.1V) = 1.0V -> 0.0002442V per digit
    scale by 46/10 -> 0.00112387V per digit
    exact scaling in V: result * 23/20465
    exact scaling in mV: result * 23000/20465 = 4600/4093
    Resistor R46: 2.2Ohm
With this values:
     measured: 3320mV real: 3060mV (charging 150mA) -> error factor: 1,085
     measured: 3180mV real: 2910mV (charging 100mA) -> error factor: 1,092
     measured: 2995mV real: 2730mV (charging 50mA) -> error factor: 1,097
     measured: 2820mV real: 2550mV (non charging) -> error factor: 1,106
     measured: 2610mV real: 2330mV (non charging) -> error factor: 1,120
     measured: 2360mV real: 2100mV (non charging) -> error factor: 1,124
     measured: 2270mV real: 2000mV (non charging) -> error factor: 1,135
     measured: 2110mV real: 1860mV (non charging) -> error factor: 1,134
     measured: 2015mV real: 1760mV (non charging) -> error factor: 1,145
     measured: 1963mV real: 1720mV (non charging) -> error factor: 1,141

get polynomial with octave
   x = [3320 3180 2995 2820 2610 2360 2270 2110 2015 1963]
   y = [3060 2910 2730 2550 2330 2100 2000 1860 1760 1720]
   p = polyfit(x,y,2);
   x1 = linspace(0, 3600);
   y1 = polyval(p,x1);
   figure
   plot(x,y,'o')
   hold on
   plot(x1,y1)
   hold off
   p
       = 0.98803  -231.60295
   -> correction polynom val*0.98803 -231.60295
   -> rounded: val * 988/1000 - 232

  combined: val * (988 * 4600) / (1000 * 4095) - 232
  combined: val * 4544800 / 4095000 - 232
  combined: val * 1748 / 1575 - 232
  Own multimeter measurement:  109,5mV R46 delta @ 50,2mA -> 2.181Ohm
                                37.6mV R46 delta @ 17.2mA -> 2.186Ohm
	*/
	//g_state.batVoltage = v1 * 4600/4095; // old with *16: 25/546;
	g_state.batVoltage = v1 * 1748 / 1575 - 232;
	//adjust v1/v2 rating according to the calibration
	v1adj = v1 * 4096 / g_state.chargerResistoroffset;
	if (g_settings.debugRs232 == 7) {
		char buffer[DEBUG_CHARS+1];
		buffer[DEBUG_CHARS] = '\0';
		snprintf_P(buffer, DEBUG_CHARS, PSTR("v1: %lu, v1adj: %lu v2: %li delta: %li\r\n"), v1, v1adj, v2, (int32_t)(v1adj - v2));
		rs232_sendstring(buffer);
	}
	//calculate with adjuested values
	//we allow negative values to try to reduce the noise
	//g_state.chargerCurrent = (v1adj - v2) * 591 / 12500;
	//g_state.chargerCurrent = (v1adj - v2) * 4600 / 9009; // div by 2.2 to get mA ((4600/4095) * (10/22)
	//with polynomial values above... (constant -232 cancels out in equation)
	g_state.chargerCurrent = (v1adj - v2) * 1748 / 3465; // div by 2.2 to get mA ((1748/1575) * (10/22)
}


static void charger_enable(void) {
	PORTQ.DIRSET = 8;
	PORTQ.PIN3CTRL = PORT_OPC_TOTEM_gc;
	PORTQ.OUTSET = 8;
	g_state.chargerState = 1;
}

static void charger_disable(void) {
	PORTQ.DIRSET = 8;
	PORTQ.PIN3CTRL = PORT_OPC_TOTEM_gc;
	PORTQ.OUTCLR = 8;
	g_state.chargerState = 0;
}


#endif

/*
Mode of operation:
By default, the charger is on.
If there is a too high voltage or too high current, the charger goes off
Also if the charged amount is too high, the charger switches off.
*/
void charger_update(void) {
	update_voltageAndCurrent();
	uint8_t newstate = 0; //always off
	uint32_t chargerMasMax = g_settings.batteryCapacity;
	chargerMasMax *= (60UL*60UL); //mAh -> mAs
	//--------------------- logic for determining the charger state --------------
	if (g_settings.chargerMode == 2) { //always on
		if (g_state.batteryCharged < (chargerMasMax*2)) {
			if (g_state.batVoltage < CHARGER_MV_MAX) {
				if (g_state.chargerCurrent < CHARGER_MA_MANUAL_MAX) {
					newstate = 1; //permit charge
				}
			}
		} else {
			g_settings.chargerMode = 0; //back to automatic, if charging is 2*MAX
		}
		if (g_state.batteryCharged > chargerMasMax) {
			if (g_state.chargerCd < (60UL*60UL)) {
				g_state.chargerCd = 60UL*60UL; //no automatic charging for at least one hour
			}
		}
	} else if (g_settings.chargerMode == 0) { //auto mode
		if (g_state.batVoltage <= CHARGER_MV_MAX) {
			if (g_state.chargerCurrent <= CHARGER_MA_MAX) {
				if (g_state.batteryCharged < chargerMasMax) {
					if (g_state.chargerCd == 0) {
						newstate = 1;
					}
				} else if (g_state.chargerCd < (60UL*60UL)) {
					g_state.chargerCd = 60UL*60UL; //no charging for 1hour
				}
			} else if (g_state.chargerCd < 60) {
				//if critical current wait at least 60seconds before new charger try
				g_state.chargerCd = 60;
			}
		} else if (g_state.chargerCd < 60) {
			//if critical voltage wait at least 60seconds before new charger try
			g_state.chargerCd = 60;
		}
	}
	if (g_state.gradcelsius10 > 320) { //do not charge if above 32°C...
		newstate = 0;
	}
	if (newstate) {
		charger_enable();
	} else {
		charger_disable();
	}
	//--------------------- update charging current and battery state ------------
	//only add samples for every minute (try to cancel out noise (can be > or < 0))
	g_state.chargerCharged60 += g_state.chargerCurrent;
	g_state.chargerCharged60iter++;
	uint16_t efficiency = 0;
	//if 60 samples, add to charged value if positive
	//prevents a zero charge to decrement the charged value
	if (g_state.chargerCharged60iter >= 60) {
		uint64_t temp = g_state.consumption - g_state.consumptionBefore60;
		temp /= 1000; //uAs -> mAs
		uint32_t consumption60 = temp; //[mAs] of discharge within the last minute
		if (g_state.chargerCharged60 > (int32_t)consumption60) {
			//we have net income
			uint32_t income60 = g_state.chargerCharged60 - consumption60; //[mAs]
			if (income60 > (g_settings.batteryCapacity*30UL)) { //income60/60s -> mA, if mA > 0.5*C
				efficiency = 128; //maximum efficiency
			} else if (income60 < g_settings.batteryCapacity) { //income60/60s -> mA, if mA > 0.016*C
				efficiency = 0;   //trickle charge, no net income
			} else {
				/*0.1C -> 60 percent, 0.5C -> 100 percent -> gradient 1. offset 0.5
				   should result in 128 if income60/60 = batteryCapacity*0.5
				   eff = 128*((income60/60)/(batteryCapacity) + 0.5);
				   eff = (income60*128)/(batteryCapacity*60) + 64;
				   eff = (income60*32)/(batteryCapacity*15) + 64;
				   scale by two for more accurate results: term*2 + 64*2. Divieder 256
				*/
				efficiency = (income60*64UL)/(g_settings.batteryCapacity*15UL) + 128;
			}
			g_state.batteryCharged += (income60 * (uint32_t)efficiency) / 256;
		} else {
			if (g_state.chargerCharged60 > 0) {
				consumption60 -= g_state.chargerCharged60; //here its <= than consumption60, so no underflow possible
			}
			//discharge by estimated consumption
			if (g_state.batteryCharged > consumption60) {
				g_state.batteryCharged -= consumption60;
			} else {
				g_state.batteryCharged = 0; //we should be emtpy now? power loss close?
			}
		}
		g_state.chargerCharged60iter = 0;
		g_state.chargerCharged60 = 0;
		g_state.consumptionBefore60 = g_state.consumption;
	}
	if (g_state.chargerCurrent >= 8) { //everything below 8 mA could be noise...
		g_state.chargerIdle = 0;
	} else {
		if (g_state.chargerIdle < 0xFFFF) {
			g_state.chargerIdle++;
		}
	}
	if (g_state.chargerCd) {
		g_state.chargerCd--;
		if (g_state.chargerCd == 0) {
			//limit charged value to maximum after charging has been stopped long enough
			if (g_state.batteryCharged > chargerMasMax) {
				g_state.batteryCharged = chargerMasMax;
			}
		}
	}
	//--------- low battery waring message ------------
	if (g_state.batVoltage <= CHARGER_MV_BAT_LOW) {
		if (g_state.batLowWarningCd == 0) {
			menu_keypress(101); //enable low bat warning
			g_state.batLowWarningCd = 10;
		}
		if (g_state.batteryCharged) {
			loggerlog_batreport();
			g_state.batteryCharged = 0; //battery needs whole charge!
		}
	}
	if (g_state.batLowWarningCd) {
		if (g_state.batLowWarningCd == 9) {
			menu_keypress(102); //disable low bat warning
		}
		g_state.batLowWarningCd--;
	}
	//------------- DEBUG -----------------------------
	if (g_settings.debugRs232 == 7) {
		char buffer[DEBUG_CHARS+1];
		buffer[DEBUG_CHARS] = '\0';
		uint16_t mah = g_state.batteryCharged/(60UL*60UL);
		snprintf_P(buffer, DEBUG_CHARS, PSTR("Charger: %umV %imA %i:%limAs %lumAs (%umAh) Mode:%u State:%u CoutDown:%u Idle:%u LoBat:%u eff:%u\r\n"),
		  g_state.batVoltage, g_state.chargerCurrent,
		  g_state.chargerCharged60iter, (long int)g_state.chargerCharged60,
		  (long unsigned int)g_state.batteryCharged, mah, g_settings.chargerMode, g_state.chargerState,
		  g_state.chargerCd, g_state.chargerIdle, g_state.batLowWarningCd, efficiency);
		rs232_sendstring(buffer);
	}
}
