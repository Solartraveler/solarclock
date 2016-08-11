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
Write PA OFF (8208)
Write Osc OFF (8200)

0x8208 does not put it in the low power mode and neither does 0x8200
I used 0x8201 and now it sleeps and draws only about 10uA


*/

#include <avr/io.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "main.h"
#include "rs232.h"
#include "config.h"

#define RFM12_NINT 3
#define RFM12_SELECT 4
#define RFM12_MOSI 5
#define RFM12_MISO 6
#define RFM12_CLK 7
#define RFM12_RESET 5

uint16_t rfm12_command(uint16_t data) {
	uint16_t indata;
	PR_PRPC &= ~PR_SPI_bm;
	PORTC.OUTCLR = (1<<RFM12_SELECT);
	//SPIC.CTRL = SPI_ENABLE_bm | SPI_MASTER_bm | SPI_MODE_0_gc | SPI_PRESCALER_DIV4_gc;
	SPIC.CTRL = SPI_ENABLE_bm | SPI_MASTER_bm | SPI_MODE_0_gc | SPI_PRESCALER_DIV64_gc;
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
	PR_PRPC |= PR_SPI_bm;
	return indata;
}

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

void rfm12_fireint(void) {
	PORTC.OUTCLR = (1<<RFM12_NINT);
	waitms(1);
	PORTC.OUTSET = (1<<RFM12_NINT);
}

#if 0
void rfm12_init(void) {
	rfm12_portinit();
	rfm12_reset();

	rfm12_showstatus();
	rfm12_showstatus();
	//Low battery detector and microcontroller clock divier command
	rfm12_command(0xC000); //low bat and MCU clock divider command: same as init value
	rfm12_showstatus();
	//configuration setting command: 12pF, 433MHz, enable data register, enable fifo
	rfm12_command(0x80D7);
	rfm12_showstatus();
	//data filter command: clock recovery auto lock control, slow mode, digital filter, data good level = 3
	rfm12_command(0xC2AB);
	rfm12_showstatus();
	//fifo and reset mode command: 8 bit rx = isr, synchron pattern, no fifo enable after syncron pattern, no high sensitive reset
	rfm12_command(0xCA81); //Set FIFO mode
	rfm12_showstatus();
	//wake-up timer command: disable wakeup
	rfm12_command(0xE000);
	rfm12_showstatus();
	//low duty-cycle command: disable low duty
	//rfm12_command(0xC800); //disable low duty cycle (once set, we dont get into low power again)
	//afc command: Keep the foffset value independently from the state of he VDI signal, -10kHz to +7.5Khz range limit, no strome edge, high accuracy mode, enable frequency offset register, enable calculation of the offeset frequency
	rfm12_command(0xC4F7); //AFC settings: autotuning: -10kHz...+7,5kHz
	rfm12_showstatus();
	//----set channel. with benedict code, default: 1----
	rfm12_command(0xA000 | 1361); //frequency setting command, 433.4025MHz
	rfm12_showstatus();
	rfm12_command(0x9800 | (4<<4)); //tx configuration power command: power = 0dB,  mod = 4 -> (4+1)*15kHz = 75kHz frequency shift
	rfm12_showstatus();
	rfm12_command(0x9400 | (5<<5)); //receiver control command: VDI output, fast valid identificator, bandwith=5->134kHz, gain= 0db, drssi=0->-103dBm threshold
	rfm12_showstatus();
	//----set baudrate of RFM12: default with benedict code: 20000----
	rfm12_command(0xC600 | 16); //data rate command: 16 results in ~21552b/s
	rfm12_showstatus();
	//---set to RX mode---
	rfm12_command(0x82C9); //enable receiver chain + receiver baseband + crystal oscillator + no crystal output
	rfm12_showstatus();
	rfm12_command(0xCA81); //Set FIFO mode (again? why?)
	rfm12_showstatus();
	waitms(1);
	rfm12_command(0xCA83); //and now with fifo enable after sync pattern. why not previous?
	rfm12_showstatus();
}
#endif
