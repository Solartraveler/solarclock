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

#include <avr/interrupt.h>
#include <avr/io.h>
#include <util/delay_basic.h>
#include <string.h>
#include <stdio.h>

#include "displayRtc.h"
#include "main.h"
#include "rs232.h"
#include "config.h"

#include "menu-interpreter-config.h"
#include "menu-interpreter.h"


#define DISP_ROWS MENU_SCREEN_Y
#define DISP_COLUMS MENU_SCREEN_X
#define DISP_COLUM_BYTES ((DISP_COLUMS+7)/8)
#define DISP_TOTAL_BYTES (DISP_COLUM_BYTES*DISP_ROWS)
#define DISP_REFRESH_RATE 100

//limit where the comparator would only be incresed by 5 or less (F_CPU/F_RTC*6) = Minimum
//constant evaluates to 428:
//#define DISP_WITHIN_INT_LIMIT ((F_CPU*7/F_RTC)+1)
//constant evaluates to 366:
#define DISP_WITHIN_INT_LIMIT (F_CPU*6/F_RTC)


//Did not help agains flicker by loosing interrupts...
//#define SYNC_IN_INT

uint8_t disp_backbuffer[DISP_TOTAL_BYTES];
volatile uint8_t disp_buffer[DISP_TOTAL_BYTES];


volatile uint8_t disp_row;

volatile uint8_t disp_timing_active_delay_cycles;

volatile uint16_t disp_timing_active_rtc;
volatile uint16_t disp_timing_idle_rtc;
volatile uint16_t disp_timing_idle_line_rtc;
volatile uint16_t disp_rtc_comp; //copy of RTC.COMP

volatile uint8_t rtc_8thcounter;

#define DISP_RTC_PER (F_RTC/8)-1 //no need for volatile, because only written on init

uint8_t const disp_linebits[7] = {0, (1<<4), (1<<5), (1<<6), (1<<7), (1<<6), 0};


/* There are four different modes of operation, depending on the selected
   brighness.
   Maximum brightness: Set next line, update next interrupt value to the row ON
                   time. Never dark -> 5 interrupts per refresh
   High brighness: Set next line, update next interrupt value to the row ON
                   time. After 5 rows, leave OFF and update interrupt value
                   for the time of darkness of all rows.
                   -> 6 interrupts per refresh
   Low brightness: Set next line, wait within the interrupt, and set to off,
                   then set the next interrupt to the time of a single line
                   being dark. -> 5 interrupts per refresh
   LEDs off:       The compare interrupt is disabled, no wakeup

The whole interrupt handler can take up to
~594 clock cycles (including 428 wait cycles) excluding sync cycles (SYNC_IN_INT)
-> 166...200 cycles expected minimum.
sync might take thee clock crystal cycles -> + 183 cycles -> 777 cycles
->0.39ms

Trying to reduce this, as the sync cycles did not fix the random flicker problem:
594-428+366 = 532cycles -> 0.266ms

We need a value of less than ~0.3ms otherwise RFM12 communication wont
keep its timing @ 10kBaud.

*/
static void disp_update_line(void) {
	uint8_t disp_row_l = disp_row;
	uint16_t update_rtc_ticks = 0;
	//1. all rows (lines) off
	PORTE.OUTSET = (1<<6);
	PORTK.OUT = 0xFF;
	//update line bits out line
	if (disp_row_l < DISP_ROWS) {
		PORTF.OUT = disp_buffer[disp_row_l*DISP_COLUM_BYTES];
		PORTH.OUT = disp_buffer[disp_row_l*DISP_COLUM_BYTES + 1];
		PORTJ.OUT = disp_buffer[disp_row_l*DISP_COLUM_BYTES + 2];
		//no need to OR PORTK, because we know its 0xFF from the row off line
		PORTK.OUT = (disp_buffer[disp_row_l*DISP_COLUM_BYTES + 3]) | 0xF0;
	}
	disp_row_l++;
	//select the column
	if (disp_row_l != 5) {
		PORTK.OUTCLR = disp_linebits[disp_row_l]; //set LED row to on
	} else {
		PORTE.OUTCLR = disp_linebits[disp_row_l]; //set LED row to on
	}
	uint8_t disp_timing_active_delay_cycles_l = disp_timing_active_delay_cycles;
	if (disp_timing_active_delay_cycles_l < (DISP_WITHIN_INT_LIMIT>>2)) {
		//low brightness case
		update_rtc_ticks = disp_timing_idle_line_rtc;
		if (disp_timing_active_delay_cycles_l) {
			_delay_loop_2(disp_timing_active_delay_cycles_l);
		}
		if (disp_row_l != 5) {
			PORTK.OUTSET = disp_linebits[disp_row_l]; //set LED row to off
		} else {
			PORTE.OUTSET = disp_linebits[disp_row_l]; //set LED row to off
		}
		if (disp_row_l >= DISP_ROWS) { // value can be up to 6 if we switch from high to low brightness
			disp_row_l = 0;
		}
	} else {
		if (disp_row_l <= 5) {
			//happens in High brighness and Maximum brightness case
			update_rtc_ticks = disp_timing_active_rtc;
		} else {
			//only happens in High brighness case
			update_rtc_ticks = disp_timing_idle_rtc;
		}
		//update line position
		if (((disp_row_l == 5) && (!disp_timing_idle_rtc)) || (disp_row_l == 6)) {
			disp_row_l = 0; //faster than a modulo
		}
	}
	/*update rtc compare value
	  While updateing, the CPU may not go to sleep mode, so doing the update
	  at the beginning results in updateing while the code executes and thus
	  results in earlier being able to go to sleep again -> save power
	*/
	uint16_t rtccomp = disp_rtc_comp + update_rtc_ticks;
/*
	if (rtccomp < (RTC.CNT + 5)) {
		rtccomp = RTC.CNT + 5;
	}
*/
	if (rtccomp >= DISP_RTC_PER) {
		rtccomp -= DISP_RTC_PER;
	}
	while(RTC_STATUS & RTC_SYNCBUSY_bm);
	RTC.COMP = rtccomp;
	disp_rtc_comp = rtccomp;
	disp_row = disp_row_l;
#ifdef SYNC_IN_INT
	while(RTC_STATUS & RTC_SYNCBUSY_bm);
#endif
}

ISR(RTC_COMP_vect) {
	if (g_state.performanceUp == 0) {
		g_state.performanceUp = TCC1.CNT;
	}
	disp_update_line();
}

ISR(RTC_OVF_vect) {
	rtc_8thcounter++;
}


void menu_screen_set(SCREENPOS x, SCREENPOS y, unsigned char color) {
	if ((x < MENU_SCREEN_X) && (y < MENU_SCREEN_Y)) {
		x = MENU_SCREEN_X - x -1; //flip horizontal
		y = MENU_SCREEN_Y - y -1; //flip vertical
		unsigned char b = disp_backbuffer[y*DISP_COLUM_BYTES + x/8];
		if (color & 1) {
			b &= ~(1<< (x % 8));
		} else {
			b |= (1<<(x % 8)); //buffer conatins inverted pattern
		}
		disp_backbuffer[y*DISP_COLUM_BYTES + x/8] = b;
	}
}

/*
uint8_t const cBitsSet[16] = {
 0, 1, 1, 2,
 1, 2, 2, 3,
 1, 2, 3, 3,
 2, 3, 3, 4};
*/
uint8_t const cBitsClear[16] = {
 4, 3, 3, 2,
 3, 2, 2, 4,
 3, 2, 1, 1,
 2, 1, 1, 0};

void menu_screen_flush(void) {
	uint8_t i;
	uint8_t dotsOn = 0;
	for (i = 0; i < DISP_TOTAL_BYTES; i++) {
		//cleared bit = lighting dot
		disp_buffer[i] = disp_backbuffer[i];
		dotsOn += cBitsClear[disp_backbuffer[i] >> 4];
		dotsOn += cBitsClear[disp_backbuffer[i] & 0xF];
	}
	g_state.dotsOn = dotsOn;
}

void menu_screen_clear(void) {
	uint8_t i;
	for (i = 0; i < DISP_TOTAL_BYTES; i++) {
		disp_backbuffer[i] = 0xFF;
	}
}

void disp_configure_set(uint8_t brightness, uint16_t refreshrate) {
	if (g_settings.debugRs232 == 3) {
		char buffer[DEBUG_CHARS+1];
		buffer[DEBUG_CHARS] = '\0';
		snprintf_P(buffer, DEBUG_CHARS, PSTR("Disp:%i@%iHz\r\n"), brightness, refreshrate);
		rs232_sendstring(buffer);
	}
	if (brightness) {
		//scale from 0...255 but leave 254 out
		brightness--;
		if (brightness == 254) {
			brightness = 255;
		}
		uint16_t ton_cpu = (uint32_t)(F_CPU/(DISP_ROWS*refreshrate))*(uint32_t)brightness/255;
		uint16_t ton_rtc = (uint32_t)(F_RTC/(DISP_ROWS*refreshrate))*(uint32_t)brightness/255;
		uint16_t toff_rtc =          (F_RTC/(DISP_ROWS*refreshrate) - ton_rtc)*DISP_ROWS;
		uint16_t toff_line_rtc =     (F_RTC/(DISP_ROWS*refreshrate)) - ton_rtc;
		ton_cpu >>= 2; //value used for _delay_loop_2, which needs 4 cycles per iteration
		if (ton_cpu > 255) {
			ton_cpu = 255;
		}
		if (ton_rtc < 6) {
			ton_rtc = 6;
		}
		if (toff_rtc < 6) {
			toff_rtc = 6;
		}
		if ((toff_line_rtc < 6) && (toff_line_rtc)) {
			toff_line_rtc = 6;
		}
		cli();
		disp_timing_active_rtc = ton_rtc;
		disp_timing_idle_rtc = toff_rtc;
		disp_timing_active_delay_cycles = ton_cpu;
		disp_timing_idle_line_rtc = toff_line_rtc;
		if ((disp_row == 5) && ((ton_cpu < (DISP_WITHIN_INT_LIMIT>>2)) ||
		    (toff_rtc == 0))) {
			//flicker free hight bright -> low bright transition
			//flicker free hight bright -> max bright transition
			disp_row = 4;
		}
		sei();
		if (g_settings.debugRs232 == 3) {
			char buffer[DEBUG_CHARS+1];
			buffer[DEBUG_CHARS] = '\0';
			snprintf_P(buffer, DEBUG_CHARS, PSTR("tOnR:%u tOffR:%u tOnC:%u tOffLr:%u\r\n"), ton_rtc, toff_rtc, ton_cpu, toff_line_rtc);
			rs232_sendstring(buffer);
		}
		RTC_INTCTRL |= RTC_COMPINTLVL_HI_gc;
	} else {
		//switch LEDs completely off (on is done by the ISR automatically)
		RTC_INTCTRL &= ~RTC_COMPINTLVL_HI_gc;
		PORTE.OUTSET = (1<<6);
		PORTF.OUT = 0xFF;
		PORTH.OUT = 0xFF;
		PORTJ.OUT = 0xFF;
		PORTK.OUT = 0xFF;
		PORTK.OUT = 0xFF;
	}
}

static void totem_port(PORT_t * port) {
	uint8_t i;
	for (i = 0; i < 8; i++) {
		register8_t * pinctrl = (&(port->PIN0CTRL)) + i;
		*pinctrl = PORT_OPC_TOTEM_gc;
	}
}

uint8_t disp_rtc_setup(void) {
	/*column drivers: low = active

	row drivers: low = active. only one row may be active at any time
	Rows 0-5: PortK: 4-7, PortE: 6.
	*/
	//set as output
	PORTE.OUTSET = (1<<6);
	PORTF.OUT = 0xFF;
	PORTH.OUT = 0xFF;
	PORTJ.OUT = 0xFF;
	PORTK.OUT = 0xFF;
	PORTE.DIRSET = (1<<6);
	PORTF.DIR = 0xFF;
	PORTH.DIR = 0xFF;
	PORTJ.DIR = 0xFF;
	PORTK.DIR = 0xFF;
	//disable pullups
	PORTQ.PIN0CTRL = PORT_OPC_TOTEM_gc; //external 32kHz crystal
	PORTQ.PIN1CTRL = PORT_OPC_TOTEM_gc; //external 32kHz crystal
	PORTE.PIN6CTRL = PORT_OPC_TOTEM_gc;
	totem_port(&PORTF);
	totem_port(&PORTH);
	totem_port(&PORTJ);
	totem_port(&PORTK);
	PR_PRGEN &= ~PR_RTC_bm; //activate power of the RTC
	OSC_XOSCCTRL = OSC_X32KLPM_bm | OSC_XOSCSEL1_bm; //low power + 32KHz
	OSC_CTRL |= OSC_XOSCEN_bm; //enable oscillator
	//initialize the screen buffer
	menu_screen_clear(); //set empty drawing screen
	menu_screen_flush(); //copy to framebuffer
	uint8_t timeout = 0;
	while (!(OSC_STATUS & OSC_XOSCRDY_bm) && (timeout < 100)) {
		_delay_loop_2(F_CPU/400); //oscillator start
		timeout++;
	}
	//now the rtc as refresh source
	PMIC.CTRL |= PMIC_HILVLEN_bm |PMIC_MEDLVLEN_bm | PMIC_LOLVLEN_bm;
	if (OSC_STATUS & OSC_XOSCRDY_bm) {
		disp_configure_set(10, 100); //sets the interrupt level for the compare interrupt
		CLK_RTCCTRL = CLK_RTCSRC_TOSC32_gc | CLK_RTCEN_bm;
		while(RTC_STATUS & RTC_SYNCBUSY_bm);
		RTC_PER = DISP_RTC_PER;
		disp_rtc_comp = 10; //some dummy init value
		RTC_COMP = 10; //some dummy init value
		RTC_CTRL = RTC_PRESCALER_DIV1_gc;
		while(RTC_STATUS & RTC_SYNCBUSY_bm);
		RTC_INTCTRL |= RTC_OVFINTLVL_LO_gc; //overflow. compare int gets set by brightness command
	} else {
		rs232_sendstring_P(PSTR("32kHz oscillator failed\r\n"));
		return 1;
	}
	//select as internal reference clock
/*
	char buffer[DEBUG_CHARS+1];
	buffer[DEBUG_CHARS] = '\0';
	snprintf(buffer, DEBUG_CHARS, "x %u, %u\r\n", (uint16_t)DFLLRC2M_CALA, (uint16_t)DFLLRC2M_CALB);
	rs232_sendstring(buffer);
	Does not work for unknown reason, likely because of missing secure write enable
	//errata workaround
	OSC_CTRL |= 1<<1; //enable 32khz oscillator
	while (!(OSC_STATUS & OSC_RC32MRDY_bm));
	OSC_DFLLCTRL |= 1;
	DFLLRC32M_CTRL = 2; //enable calibration of internal 2MHz oscillator
	//end errata workaround
	OSC_DFLLCTRL |= 1; //use external 32.768kHz crystal
	DFLLRC2M_CTRL = 1; //enable calibration of internal 2MHz oscillator
	*/
	return 0;
}

#ifdef SYNC_IN_INT
//if the interrupt waits for sync itself, no problem to shut down in main thread
void rtc_waitsafeoff(void) {
}

#else
/*
There are problems:
  If the clock goes to idle but sync is running, updateing RTC.COMP might fail
  -> row flicker.
  Just waiting is not sufficient -> because after waiting there could be a new
  interrupt, resulting in waiting again and loosing sync again.
*/
void rtc_waitsafeoff(void) {
	//wait for sync will take up to 3 RTC clock cycles -> 183 CPU cycles.
	//make sure there is some time until the next int
	uint8_t rtccnt;
	uint8_t rtcmax = RTC.PER;
	if (rtcmax > 2) {
		do {
			while(RTC_STATUS & RTC_SYNCBUSY_bm);
			rtccnt = RTC.CNT;
		} while (((uint8_t)(rtccnt + 2) >= RTC.COMP) || ((rtccnt + 2) >= rtcmax));
	}
}
#endif
