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
#include "touch.h"
#include "rs232.h"
#include "debug.h"

#define TOUCH_POWERREG PR_PRPF
#define TOUCH_POWERBIT PR_TC0_bm
#define TOUCH_TIMER TCF0

void touch_sample(uint8_t channel, uint16_t * cha, uint16_t * chb) {
	DEBUG_FUNC_ENTER(touch_sample);
	uint16_t result = 0;
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
	_delay_loop_2(F_CPU/80000); //wait 50us
	//reset to count up again
	ACA_CTRLB = 50; //90% of Vcc
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
	result = 0;
	while (((TOUCH_TIMER.INTFLAGS & (TC1_CCAIF_bm | TC1_CCBIF_bm)) !=
	       (TC1_CCAIF_bm | TC1_CCBIF_bm)) && (result < 3000)) {
		result++;
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


int compareuint16(const void * pa, const void * pb) {
	uint16_t a = *((uint16_t *)pa);
	uint16_t b = *((uint16_t *)pb);
	if (a > b) {
		return 1;
	}
	if (a < b) {
		return -1;
	}
	return 0;
}

#define SAMPLES 11
#define HISTORYDEPTH 8
#define CHANNELS 4

uint8_t touch_test(void) {
	static uint8_t historyindex = 0;
	static uint16_t rawvalueshistory[CHANNELS][HISTORYDEPTH];
	static uint8_t deadtime = 0;
	uint16_t rawvaluestable[CHANNELS][SAMPLES];
	uint8_t i, j;
	uint8_t result = 0;
	for (i = 0; i < SAMPLES; i++) {
/*
		for (j = 0; j < CHANNELS/2; j++) {
			touch_sample(j*2, &(rawvaluestable[j*2][i]), &(rawvaluestable[j*2+1][i]));
		}
*/
		uint16_t dummy;
		for (j = 0; j < CHANNELS; j++) {
			touch_sample(j, &(rawvaluestable[j][i]), &dummy);
		}
	}
	//get median value
	for (i = 0; i < CHANNELS; i++) {
		qsort(rawvaluestable[i], sizeof(uint16_t), SAMPLES, &compareuint16);
		rawvalueshistory[i][historyindex] = rawvaluestable[i][SAMPLES/2];
	}
	//print current results
	char buffer[64];

#if CHANNELS == 8
	sprintf(buffer, "AC: %05u %05u %05u %05u %05u %05u %05u %05u\r\n",
	   rawvalueshistory[0][historyindex],
	   rawvalueshistory[1][historyindex],
	   rawvalueshistory[2][historyindex],
	   rawvalueshistory[3][historyindex],
	   rawvalueshistory[4][historyindex],
	   rawvalueshistory[5][historyindex],
	   rawvalueshistory[6][historyindex],
	   rawvalueshistory[7][historyindex]);
#elif CHANNELS == 4
	sprintf(buffer, "AC: %05u %05u %05u %05u\r\n",
	   rawvalueshistory[0][historyindex],
	   rawvalueshistory[1][historyindex],
	   rawvalueshistory[2][historyindex],
	   rawvalueshistory[3][historyindex]);
#elif CHANNELS == 3
	sprintf(buffer, "AC: %05u %05u %05u\r\n",
	   rawvalueshistory[0][historyindex],
	   rawvalueshistory[1][historyindex],
	   rawvalueshistory[2][historyindex]);
#elif CHANNELS == 2
	sprintf(buffer, "AC: %05u %05u\r\n",
	   rawvalueshistory[0][historyindex],
	   rawvalueshistory[1][historyindex]);
#endif
	rs232_sendstring(buffer);
	//analyze result
	uint8_t oklevel[CHANNELS];
	if (deadtime == 0) {
		uint8_t lowestok = HISTORYDEPTH;
		uint8_t equalok = 0;
		for (i = 0; i < CHANNELS; i++) {
			int16_t historydelta[HISTORYDEPTH-1];
			oklevel[i] = 0;
			for (j = 0; j < HISTORYDEPTH-1; j++) {
				historydelta[j] = rawvalueshistory[i][(historyindex + j) % HISTORYDEPTH] - rawvalueshistory[i][(historyindex + j + 1) % HISTORYDEPTH];
			}
			for (j = 0; j <  HISTORYDEPTH-1; j++) {
				if ((historydelta[j] < 4) && (historydelta[j] > -4)) {
					oklevel[i]++;
				}
			}
			if (oklevel[i] < 5) {
				if (oklevel[i] < lowestok) {
					result = i+1;
					deadtime = 6;
					lowestok = oklevel[i];
					equalok = 0;
				} else if ((oklevel[i] == lowestok) && !((result-1 == 2) && (i == 2))) {
					equalok = 1;
				}
			}
		}
		if (equalok) { //cant decide which is lowest -> better next time
			deadtime = 0;
			result = 0;
		}
#if 0
#if CHANNELS == 4
		sprintf(buffer, "%u %u %u %u\r\n",
		(uint16_t)oklevel[0], (uint16_t)oklevel[1], (uint16_t)oklevel[2], (uint16_t)oklevel[3]);
#elif CHANNELS == 3
		sprintf(buffer, "%u %u %u\r\n", (uint16_t)oklevel[0], (uint16_t)oklevel[1], (uint16_t)oklevel[2]);
#elif CHANNELS == 2
		sprintf(buffer, "%u %u\r\n", (uint16_t)oklevel[0], (uint16_t)oklevel[1]);
#endif
		rs232_sendstring(buffer);
#endif
	} else {
		deadtime--;
	}
	//decrement history index buffer
	if (historyindex == 0) {
		historyindex = HISTORYDEPTH;
	}
	historyindex--;
	return result;
}
