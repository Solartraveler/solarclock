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

#include <stdio.h>
#include <avr/io.h>
#include <avr/interrupt.h>

#include "sound.h"
#include "main.h"
#include "rs232.h"

volatile uint32_t g_soundcycles;
volatile uint16_t g_soundton;

ISR(TCC0_OVF_vect) {
	uint32_t soundcycles = g_soundcycles;
	soundcycles--;
	g_soundcycles = soundcycles;
	if (soundcycles == 0) {
		TCC0.CTRLA = TC_CLKSEL_OFF_gc; //off again
		PORTC.PIN0CTRL = PORT_OPC_PULLUP_gc;
		TCC0.CTRLB = 0; //restore port functionality
		PORTC.DIRCLR = 1;
		PORTC.OUTCLR = 1;
		PR_PRPC |= PR_TC0_bm | PR_SPI_bm | PR_HIRES_bm; //disable the timer + spi
		TCC0.CCA = 1;
	} else if (soundcycles < TCC0.CCA) {
			TCC0.CCA--;
	} else if (TCC0.CCA < g_soundton) {
		TCC0.CCA++;
	}
}

void sound_beep(uint8_t volume, uint16_t frequency, uint16_t duration) {
	//set output pin Pc0
	TCC0.CTRLA = TC_CLKSEL_OFF_gc; //stop timer
	if (g_soundcycles == 0) {
		PORTC.OUTCLR = 1;
		PORTC.DIRSET = 1;
		PORTC.PIN0CTRL = PORT_INVEN_bm;
		PR_PRPC &= ~(PR_TC0_bm | PR_SPI_bm | PR_HIRES_bm); //power the timer, beeper on spi pins
		TCC0.CTRLB = TC0_CCAEN_bm | TC0_WGMODE0_bm | TC0_WGMODE1_bm; //single slope on A
		TCC0.CCA = 1; //min volume
	}
	g_soundcycles = (uint32_t)duration*(uint32_t)frequency/1000;
	TCC0.CNT = 0x0;    //reset counter
	if ((F_CPU)/frequency > 100) {
		g_soundton = (F_CPU/frequency)/(8*(256-volume)); //25% max volume limit
		if (TCC0.CCA > g_soundton) {
			TCC0.CCA = g_soundton;
		}
		TCC0.PER = (F_CPU)/frequency;   //new timer frequency value
		if (g_soundton == 0) g_soundton = 1;
		TCC0.INTCTRLA = TC_OVFINTLVL_LO_gc;
		TCC0.CTRLA = TC_CLKSEL_DIV1_gc; //start timer, divide by 1
	}
}
