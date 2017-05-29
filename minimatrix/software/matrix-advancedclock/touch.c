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

#include <util/delay_basic.h>
#include <avr/pgmspace.h>
#include <avr/interrupt.h>
#include <avr/sleep.h>
#include <avr/io.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "main.h"
#include <util/delay.h>
#include "touch.h"
#include "rs232.h"
#include "debug.h"

#define TOUCH_POWERREG PR_PRPF
#define TOUCH_POWERBIT PR_TC0_bm
#define TOUCH_TIMER TCF0

void touch_sample(uint8_t channel, uint16_t * cha, uint16_t * chb) {
	DEBUG_FUNC_ENTER(touch_sample);
	//uint16_t result = 0;
	//configure i/o for discharge
	PORTA.PIN2CTRL = PORT_OPC_TOTEM_gc;
	PORTA.PIN3CTRL = PORT_OPC_TOTEM_gc;
	PORTA.PIN4CTRL = PORT_OPC_TOTEM_gc;
	PORTA.PIN5CTRL = PORT_OPC_TOTEM_gc;
	PORTA.PIN6CTRL = PORT_OPC_TOTEM_gc;
	PORTA.DIRSET = 0x7C;
	PORTA.OUTCLR = 0x7C;
	//disable power reduction
	PR_PRPA &= ~PR_AC_bm; //clock comparator
	uint8_t evsyspowerstate = PR_PRGEN;
	PR_PRGEN &= ~PR_EVSYS_bm; //clock event system
	TOUCH_POWERREG &= ~TOUCH_POWERBIT; //clock timer F0
	//configure analog comparator
	ACA_AC0CTRL = AC_ENABLE_bm | AC_INTMODE0_bm | AC_INTMODE1_bm; //enable, int on rising
	ACA_AC1CTRL = AC_ENABLE_bm | AC_INTMODE0_bm | AC_INTMODE1_bm; //enable, int on rising
	ACA_AC0MUXCTRL = ((channel + 3) << AC_MUXPOS_gp) | AC_MUXNEG_SCALER_gc;
	ACA_AC1MUXCTRL = ((channel + 4) << AC_MUXPOS_gp) | AC_MUXNEG_SCALER_gc;
	ACA_CTRLA = 0; //no AC out
//	ACA_CTRLB = 1; // 17% of Vcc
	ACA_WINCTRL = 0;
	//setup event to count time with timer
	EVSYS.CH0MUX = 0; //no routing on discharge
	EVSYS.CH0CTRL = 0; //no quadrature decoder
	EVSYS.CH1MUX = 0; //no routing on discharge
	EVSYS.CH1CTRL = 0; //no quadrature decoder
	//configure timerF0
	TOUCH_TIMER.CTRLA = 0; //stop timer
	TOUCH_TIMER.CTRLB = TC0_CCAEN_bm | TC0_CCBEN_bm; //compare capture A, B
	TOUCH_TIMER.CTRLC = 0;
	TOUCH_TIMER.CTRLD = TC_EVACT_CAPT_gc | TC_EVSEL_CH0_gc; //capture on event 0 (+1)
	TOUCH_TIMER.CTRLE = 0; //normal 16bit counter
	TOUCH_TIMER.INTCTRLA = 0;
	TOUCH_TIMER.INTCTRLB = 0;
	TOUCH_TIMER.CNT = 0x0; //reset counter
	TOUCH_TIMER.PER = 0xFFFF; //count to maximum value
	//wait for discharge of cap
/*
	while ((ACA_STATUS & (AC_AC0STATE_bm | AC_AC1STATE_bm )) && (result != 0xFFFF)) {
		result++;
	}
*/
	_delay_ms(0.02); //wait 20us
	//reset to count up again
	//ACA_CTRLB = 50; //90% of Vcc
	//ACA_CTRLB = 31; //50% of Vcc
	ACA_CTRLB = 15; //25% of Vcc (is a lot faster to measure)
	//while ((ACA_STATUS & (AC_AC0STATE_bm | AC_AC1STATE_bm )));
	ACA_STATUS = AC_AC0IF_bm | AC_AC1IF_bm; //clear interrupt flags
	EVSYS.CH0MUX = EVSYS_CHMUX_ACA_CH0_gc; //analog comparator as source
	EVSYS.CH1MUX = EVSYS_CHMUX_ACA_CH1_gc; //analog comparator as source
	cli();
	PORTA.PIN3CTRL = PORT_ISC_INPUT_DISABLE_gc;
	PORTA.PIN4CTRL = PORT_ISC_INPUT_DISABLE_gc;
	PORTA.PIN5CTRL = PORT_ISC_INPUT_DISABLE_gc;
	PORTA.PIN6CTRL = PORT_ISC_INPUT_DISABLE_gc;
	PORTA.DIRCLR = 0x78;
	TOUCH_TIMER.INTFLAGS = TC1_CCAIF_bm | TC1_CCBIF_bm;
	TOUCH_TIMER.CTRLA = TC_CLKSEL_DIV1_gc; //start counting
	PORTA.OUTSET = 4; //start charging the caps
	sei();
	while (((TOUCH_TIMER.INTFLAGS & (TC1_CCAIF_bm | TC1_CCBIF_bm)) !=
	       (TC1_CCAIF_bm | TC1_CCBIF_bm)) && (TOUCH_TIMER.CNT < 8000)) {
	}
	if (TOUCH_TIMER.INTFLAGS & TC1_CCAIF_bm) {
		*cha = TOUCH_TIMER.CCA;
	} else {
		*cha = 0xFFFF;
	}
	if (TOUCH_TIMER.INTFLAGS & TC1_CCBIF_bm) {
		*chb = TOUCH_TIMER.CCB;
	} else {
		*chb = 0xFFFF;
	}
	//start discharging the cap
	ACA_AC0CTRL = 0;
	ACA_AC1CTRL = 0;
	PORTA.OUTCLR = 4; //discharge caps
	//enable power reduction
	PR_PRPA |= PR_AC_bm;
	TOUCH_POWERREG |= TOUCH_POWERBIT;
	if (evsyspowerstate & PR_EVSYS_bm) {
		PR_PRGEN |= PR_EVSYS_bm;
	}
	DEBUG_FUNC_LEAVE(touch_sample);
}


#define CHANNELS 4
#define SAMPLES 5

uint8_t touchKeysRead(void) {
	uint16_t rawvaluestable[CHANNELS];
	static uint16_t rawvaluestableHistory[CHANNELS] = {0,0,0,0};
	static uint8_t safeUpdate[CHANNELS] = {255,255,255,255}; //limits continous press to 31secs and forces an update on first call
	uint8_t i, j;
	uint8_t result = 0;
	//print current results
	char buffer[64];
	uint16_t raw0, raw1;
	for (i = 0; i < CHANNELS; i++) {
		rawvaluestable[i] = 0;
	}
	for (j = 0; j < SAMPLES; j++) {
		for (i = 0; i < CHANNELS/2; i++) {
			touch_sample(i*2, &raw0, &raw1);
			if (rawvaluestable[i*2] < raw0) {
				rawvaluestable[i*2] = raw0;
			}
			if (rawvaluestable[i*2+1] < raw1) {
				rawvaluestable[i*2+1] = raw1;
			}
		}
	}
	if (g_settings.debugRs232 == 4) {
#if CHANNELS == 4
		sprintf(buffer, "AC: %05u %05u %05u %05u\r\n",
		   rawvaluestable[0],
		   rawvaluestable[1],
		   rawvaluestable[2],
		   rawvaluestable[3]);
#elif CHANNELS == 3
		sprintf(buffer, "AC: %05u %05u %05u\r\n",
		   rawvaluestable[0],
		   rawvaluestable[1],
		   rawvaluestable[2]);
#elif CHANNELS == 2
		sprintf(buffer, "AC: %05u %05u\r\n",
		   rawvaluestable[0],
		   rawvaluestable[1]);
#endif
		rs232_sendstring(buffer);
	}
	//Debug value for display:
	g_state.keyDebugAd = rawvaluestable[1];
	//analyze result
	for (i = 0; i < CHANNELS; i++) {
		if ((rawvaluestable[i] >  rawvaluestableHistory[i] + 250) &&
				(rawvaluestable[i] <  rawvaluestableHistory[i] + 2000) &&
			  (rawvaluestable[i] > 1000) && (rawvaluestable[i] < 10000) &&
			  (safeUpdate[i] < 255)) {
			result = i + 1;
			safeUpdate[i]++;
		} else {
			rawvaluestableHistory[i] = rawvaluestable[i];
			safeUpdate[i] = 0;
		}
	}
	if (result) {
		if (g_settings.debugRs232) {
			sprintf(buffer, "Pressed: %c\r\n", result + 'A' - 1);
			rs232_sendstring(buffer);
		}
	}
	return result;
}

