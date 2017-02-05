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

/*
Pin connection:
PC3 = NInt (output on AVR side)
PC4 = NSEL (output on AVR side)
PC5 = SDI (output on AVR side)
PC6 = SDO (input on AVR side)
PC7 = SCK (output on AVR side)

RF12 samples on the risign edge and wants MSB first,
looks like the interface could work with up to 20MHz.
So, no problem if we have a prescaler of 4 -> 2MHz CPU -> 500kHz,
32MHz CPU -> 8MHz Clock


Some settings and parameters are copied from
Benedikt K:
http://www.mikrocontroller.net/topic/71682?goto=3896183


https://lowpowerlab.com/forum/index.php?topic=115.0
The observation fits with my own one: 0x8201 sets it to low power mode.
I used 0x8201 and now it sleeps and draws only about 10µA
*/

#include <avr/io.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "main.h"
#include <util/delay.h>

#include "rs232.h"
#include "config.h"

#define RFM12_NINT 3
#define RFM12_SELECT 4
#define RFM12_MOSI 5
#define RFM12_MISO 6
#define RFM12_CLK 7
#define RFM12_RESET 5

#if 0
//uses hardware (not working)
uint16_t rfm12_command(uint16_t data) {
	uint16_t indata;
	PR_PRPC &= ~PR_SPI_bm;
	PORTC.OUTCLR = (1<<RFM12_SELECT);
	PORTC.PIN6CTRL = PORT_OPC_TOTEM_gc; //now the RFM12 drives the pin
	SPIC.CTRL = SPI_ENABLE_bm | SPI_MASTER_bm | SPI_MODE_0_gc | SPI_PRESCALER_DIV4_gc;
	SPIC.INTCTRL = SPI_INTLVL_OFF_gc;
	//Reading status + reading data = clear status bits
	SPIC.STATUS;
	SPIC.DATA;
	//shift out data
	SPIC.DATA = data >> 8;
	while (!SPIC.STATUS); //wait for shift of first byte
	indata = SPIC.DATA << 8;
	SPIC.DATA = data && 0xFF;
	while (!SPIC.STATUS); //wait for shift of second byte
	indata |= SPIC.DATA;
	PORTC.OUTSET = (1<<RFM12_SELECT);
	PORTC.PIN6CTRL = PORT_OPC_PULLUP_gc; //RFM12 is high impedance again
	//PR_PRPC |= PR_SPI_bm;
	return indata;
}
#else
//use software (working)
uint16_t rfm12_command(uint16_t outdata) {
	uint16_t indata = 0;
	PORTC.OUTCLR = (1<<RFM12_CLK);
	PORTC.OUTCLR = (1<<RFM12_SELECT);
	PORTC.PIN6CTRL = PORT_OPC_TOTEM_gc; //now the RFM12 drives the pin
	//shift out and get data
	uint8_t i;
	/* The RFM12 samples the pin on the rising clock edge and changes the
	   Pin on the falling clock edge. The first bit is there before any clock.
	*/
	for (i = 0; i < 16; i++) {
		indata <<= 1;
		//write data
		if (outdata & 0x8000) {
			PORTC.OUTSET = (1<<RFM12_MOSI);
		} else {
			PORTC.OUTCLR = (1<<RFM12_MOSI);
		}
		_delay_us(1.0);
		//rise clock
		PORTC.OUTSET = (1<<RFM12_CLK);
		//read data
		if (PORTC.IN & (1<<RFM12_MISO)) {
			indata |= 1;
		}
		_delay_us(1.0);
		//clock to low
		PORTC.OUTCLR = (1<<RFM12_CLK);
		outdata <<= 1;
	}
	_delay_us(1.0); //give some time to the RFM12 for sampling
	PORTC.OUTSET = (1<<RFM12_SELECT);
	PORTC.PIN6CTRL = PORT_OPC_PULLUP_gc; //RFM12 pin is high impedance again
	return indata;
}
#endif

uint16_t rfm12_status(void) {
	return rfm12_command(0x0); //status read
}

void rfm12_reset(void) {
	//send reset (PE5)
	PORTE.PIN5CTRL = PORT_OPC_TOTEM_gc;
	PORTE.DIRSET = (1<<RFM12_RESET);
	PORTE.OUTCLR = (1<<RFM12_RESET);
	waitms(1); //1µs requirement
	PORTE.OUTSET = (1<<RFM12_RESET);
	waitms(200); //1ms requirement (100ms according to microcontroller.net)
}

void rfm12_portinit(void) {
	PORTC.PIN3CTRL = PORT_OPC_TOTEM_gc;
	PORTC.PIN4CTRL = PORT_OPC_TOTEM_gc;
	PORTC.PIN5CTRL = PORT_OPC_TOTEM_gc;
	PORTC.PIN6CTRL = PORT_OPC_PULLUP_gc; //MISO
	PORTC.PIN7CTRL = PORT_OPC_TOTEM_gc;
	PORTC.DIRSET = (1<<RFM12_SELECT) | (1<<RFM12_MOSI) | (1<<RFM12_CLK) | (1<<RFM12_NINT);
	PORTC.OUTSET = (1<<RFM12_SELECT) | (1<<RFM12_NINT); //set ss to high = disable = sync device
}

uint16_t rfm12_showstatus(void) {
	uint16_t status = rfm12_status();
	if (g_settings.debugRs232 == 6) {
		char buffer[DEBUG_CHARS+1];
		buffer[DEBUG_CHARS] = '\0';
		snprintf_P(buffer, DEBUG_CHARS, PSTR("RFM12 stat: 0x%x\r\n"), status);
		rs232_sendstring(buffer);
	}
	return status;
}

void rfm12_standby(void) {
	/*
	After power on, the consumption is appox 2mA.
	*/
	rfm12_portinit();
	rfm12_reset();
	rfm12_command(0x8201); //write power management register with ext clock = off
	rfm12_showstatus();
	rfm12_showstatus();
	SPIC.CTRL &= ~SPI_ENABLE_bm;
	PR_PRPC |= PR_SPI_bm;
}

