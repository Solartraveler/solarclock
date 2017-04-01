/* Matrix-Simpleclock
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

#include <avr/io.h>
#include <stdio.h>
#include <string.h>
#include <avr/interrupt.h>

#include "main.h"

#include <util/delay_basic.h>
#include <avr/pgmspace.h>
#include <avr/sleep.h>
#include <avr/wdt.h>
#include <stdlib.h>

#include "dcf77.h"
#include "rs232.h"


uint8_t g_calib_running;
int16_t g_calib_best_delta;
uint8_t g_calib_best_settings;

uint16_t g_counter_speedstart;

//first caputure will switch to second capture register
ISR(TCD1_CCA_vect) {
	EVSYS.CH3MUX = EVSYS_CHMUX_RTC_OVF_gc;
	EVSYS.CH2MUX = 0;
	TCD1.INTFLAGS = TC1_CCAIF_bm | TC1_CCBIF_bm; //clear interrupt flag
}

ISR(TCD1_CCB_vect) {
	TCD1.INTFLAGS = TC1_CCAIF_bm | TC1_CCBIF_bm; //clear interrupt flag
	EVSYS.CH3MUX = 0;
}

//this routine will take 1/8...2/8th seconds
//note: rtc overflow happens 8 times per second
void calibrate_iterate(void) {
	//1. set up timer to count, set up event system to captue on overflow of rc ovl
	EVSYS.CH2MUX = 0;
	EVSYS.CH3MUX = 0;
	EVSYS.CH3CTRL = 0; //no quadrature decoder
	EVSYS.CH2CTRL = 0; //no quadrature decoder

	TCD1.CTRLA = 0; //stop counter
	TCD1.CTRLB = TC1_CCAEN_bm | TC1_CCBEN_bm; // select Modus: capture on CCA
	TCD1.CTRLC = 0;
	TCD1.CTRLD = TC_EVACT_CAPT_gc | TC_EVSEL_CH2_gc; //capture on event 2
	TCD1.CTRLE = 0; //normal 16bit counter
	TCD1.INTFLAGS = TC1_CCAIF_bm | TC1_CCBIF_bm; //clear interrupt flag
	TCD1.INTCTRLA = 0; // no interrupt
	TCD1.INTCTRLB = TC_CCAINTLVL_LO_gc | TC_CCBINTLVL_LO_gc;
	TCD1.PER = 0xFFFF;   //maximum periode
	TCD1.CNT = 0x0;    //reset counter
	TCD1.CTRLA = TC_CLKSEL_DIV8_gc; //divide by 8 -> rollover once possible
	//2. setup event to count time with timer
	//we need two events because we capture twice in different couters
	EVSYS.CH2MUX = EVSYS_CHMUX_RTC_OVF_gc; //no routing on discharge
}

void calibrate_start(void) {
	PR_PRGEN &= ~PR_EVSYS_bm; //clock event system
	PR_PRPD &= ~PR_TC1_bm; //clock timer D1
	g_calib_running = 1;
	g_calib_best_delta = 0x7FFF;
	g_calib_best_settings = DFLLRC2M_CALA;
	calibrate_iterate();
}

int16_t calibrate_update(void) {
	if (!g_calib_running) {
		return 0x7FFF; //7FFF = invalid value
	}
	if ((EVSYS.CH2MUX == 0) && (EVSYS.CH3MUX == 0)) { //2. isr has disabled interrupts
		uint16_t starttick = TCD1.CCA;
		uint16_t stoptick = TCD1.CCB;
		//calibration value calculation
		uint32_t delta = stoptick - starttick;
		if (starttick > stoptick) {
			delta = starttick - stoptick;
		}
		//multiply by 8: because TCD1 divider = 8, divider by 8: cause rc overflows every 1/8th second
		uint32_t deltap = delta*8*10000/(F_CPU/8); //delta in 100/th percent
		int16_t deltas = deltap - 10000; //positive = too fast, negative = too slow
		if (abs(deltas) < abs(g_calib_best_delta)) {
			g_calib_best_delta = deltas;
			g_calib_best_settings = DFLLRC2M_CALA;
			//adjust values
			if (deltas > 0) {
				DFLLRC2M_CALA--; //slow down
			} else {
				DFLLRC2M_CALA++; //speed up
			}
			calibrate_iterate(); //start next round
		} else { //calibration done, use previous minimized value
			DFLLRC2M_CALA = g_calib_best_settings;
			g_calib_running = 0;
			TCD1.CTRLA = TC_CLKSEL_OFF_gc;
			PR_PRGEN |= PR_EVSYS_bm; //stop clock event system
			PR_PRPD |= PR_TC1_bm; //stop clock timer D0
			//debugprint
			char buffer[DEBUG_CHARS+1];
			buffer[DEBUG_CHARS] = '\0';
			snprintf_P(buffer, DEBUG_CHARS, PSTR("Cal done: error=%i/100 %%\r\n"), deltas);
			rs232_sendstring(buffer);
			return deltas;
		}
	}
	return 0x7FFF;
}

void clock_highspeed(void) {
	uint8_t timeout = 0;
	OSC_CTRL &= ~OSC_PLLEN_bm; //otherwise scaler cant be changed
	//enable PLL
	OSC_PLLCTRL = 8; //from 2 to 16MHz (requires >= 2.2V, however the device seems to go much faster under "comfortable" temperatures)
	OSC_CTRL |= OSC_PLLEN_bm; //start pll
	//wait until stable
	while (!(OSC_STATUS & OSC_PLLRDY_bm) && (timeout < 100)) {
		waitms(1);
		timeout++;
	}
	//switch if stable
	if (OSC_STATUS & OSC_PLLRDY_bm) {
		cli();
		CCP = 0xD8; //change protection
		CLK_CTRL = CLK_SCLKSEL_PLL_gc;
		if (USART.CTRLB) { //scale UART divider by 8, BSCALE = 3 -> 2^3 = 8
			USART.BAUDCTRLB |= 3 << USART_BSCALE_gp;
		}
		//change prescaler of timers
		if (TCC0.CTRLA == TC_CLKSEL_DIV1_gc) { //beep frequency
			TCC0.CTRLA = TC_CLKSEL_DIV8_gc;
		}
		if (TCD0.CTRLA == TC_CLKSEL_DIV1_gc) { //dcf77 sample
			TCD0.CTRLA = TC_CLKSEL_DIV8_gc;
		}
		if (TCE1.CTRLA == TC_CLKSEL_DIV1_gc) { //rfm12 sample
			TCE1.CTRLA = TC_CLKSEL_DIV8_gc;
		}
		if (TCD1.CTRLA == TC_CLKSEL_DIV8_gc) { //rc calibration value
			TCD1.CTRLA = TC_CLKSEL_DIV64_gc;
		}
		g_counter_speedstart = TCC1.CNT;
		sei();
	}
}

void clock_normalspeed(void) {
	//switch to 2MHz internal RC
	cli();
	CCP = 0xD8; //change protection
	CLK_CTRL = CLK_SCLKSEL_RC2M_gc;
	if (USART.CTRLB) { //scale UART divider by 8, BSCALE = 3
		USART.BAUDCTRLB &= ~USART_BSCALE_gm;
	}
	//change prescalers back to normal
	if (TCC0.CTRLA == TC_CLKSEL_DIV8_gc) { //beep frequency
		TCC0.CTRLA = TC_CLKSEL_DIV1_gc;
	}
	if (TCD0.CTRLA == TC_CLKSEL_DIV8_gc) { //dcf77 sample
		TCD0.CTRLA = TC_CLKSEL_DIV1_gc;
	}
	if (TCE1.CTRLA == TC_CLKSEL_DIV8_gc) { //rfm12 sample
		TCE1.CTRLA = TC_CLKSEL_DIV1_gc;
	}
	if (TCD1.CTRLA == TC_CLKSEL_DIV64_gc) { //rc calibration value
		TCD1.CTRLA = TC_CLKSEL_DIV8_gc;
	}
	//fix performance counter value
	TCC1.CNT = ((TCC1.CNT - g_counter_speedstart) / 8)  + g_counter_speedstart;
	sei();
	//disable PLL
	OSC_CTRL &= ~OSC_PLLEN_bm;
}


/*
static void setExternalCrystal(void) {
	uint8_t timeout = 0;
	//set to crystal (4MHZ)
	OSC_XOSCCTRL = OSC_FRQRANGE_2TO9_gc | OSC_XOSCSEL_XTAL_16KCLK_gc; //2-9MHz
	OSC_CTRL |= OSC_XOSCEN_bm; //enable external oscillator
	while (!(OSC_STATUS & OSC_XOSCRDY_bm) && (timeout < 100)) {
		waitms(1); //oscillator start
		timeout++;
	}
	if (OSC_STATUS & OSC_XOSCRDY_bm) {
		CCP = 0xD8; //change protection
		CLK_CTRL = CLK_SCLKSEL_XOSC_gc; //set to new source
		OSC_CTRL &= ~OSC_RC2MEN_bm; //stop internal oscillator to save power
	}
}
*/
/*
static void set32kExtOsc(void) {
	uint8_t timeout = 0;
	OSC_XOSCCTRL = OSC_X32KLPM_bm | OSC_XOSCSEL1_bm; //low power + 32KHz
	OSC_CTRL |= OSC_XOSCEN_bm;
	while (!(OSC_STATUS & OSC_XOSCRDY_bm) && (timeout < 100)) {
		waitms(100); //oscillator start
		timeout++;
	}
	if (OSC_STATUS & OSC_XOSCRDY_bm) {
		CCP = 0xD8; //change protection
		CLK_CTRL = CLK_SCLKSEL_XOSC_gc; //set to new source
		OSC_CTRL &= ~OSC_RC2MEN_bm; //stop internal oscillator to save power
		textbuffer[0] = 'K';
		textbuffer[1] = '\0';
	} else {
		textbuffer[0] = 'F';
		textbuffer[1] = '\0';
	}
}
*/

/*
static void set32kOsc(void) {
	uint8_t timeout = 0;
	OSC_CTRL |= OSC_RC32KEN_bm;
	while (!(OSC_STATUS & OSC_RC32KRDY_bm) && (timeout < 100)) {
		waitms(10); //oscillator start
		timeout++;
	}
	if (OSC_STATUS & OSC_RC32KRDY_bm) {
		CCP = 0xD8; //change protection
		CLK_CTRL = CLK_SCLKSEL_RC32K_gc; //set to new source
		OSC_CTRL &= ~OSC_RC2MEN_bm; //stop internal oscillator to save power
		textbuffer[0] = 'K';
		textbuffer[1] = '\0';
	} else {
		textbuffer[0] = 'F';
		textbuffer[1] = '\0';
	}
}
*/

	//CCP = 0xD8;
	//CLK_PSCTRL = 3<<2; //div by 4
