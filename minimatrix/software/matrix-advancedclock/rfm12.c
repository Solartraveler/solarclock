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

/*
Pin connection:
PC3 = NInt (output on AVR side)
PC4 = NSEL (output on AVR side)
PC5 = SDI (output on AVR side)
PC6 = SDO (input on AVR side)
PC7 = SCK (output on AVR side)
PB1 = NIRQ (input on AVR side)
PE5 = NRes (output on AVR side)

RF12 samples on the risign edge and wants MSB first,
looks like the interface could work with up to 20MHz.
So, no problem if we have a prescaler of 4 -> 2MHz CPU -> 500kHz,
32MHz CPU -> 8MHz Clock
The software implementation waits 1µs after every clock level change ->
max ~500kHz clock speed for all cpu frequencies. Low frequencies will be
significant slower due the limited processing speed of the loop:
1µs @ 2MHz -> just two cycles -> loop will process a long way.


Some settings and parameters are copied from
Benedikt K:
http://www.mikrocontroller.net/topic/71682?goto=3896183

The used protocoll here should be finally compatible with his protocol.
Protocol of the bytes is:
0...4: Sync data: 0xAA + 0xAA + 0xAA + 0x2D + 0xD4
5:     Status byte: bit 0 set: received ok
6:     Number of bytes to send
7...N: Data bytes: Only if number of bytes is > 0.
       If at least one data byte, first byte(7) is an id.
       Receiver accepts package when ID is different to the previous one.
       Transmitter increases ID if the reception has been actknowledged or the
       the actknowledge is not received within some timeout.
N+1:   CRC of data bytes 5...N
N+2:   0

https://lowpowerlab.com/forum/index.php?topic=115.0
The observation fits with my own one: 0x8201 sets it to low power mode.
I used 0x8201 and now it sleeps and draws only about 10µA

*/

#include <avr/io.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <avr/interrupt.h>
#include <ctype.h>

#include "main.h"
#include <util/delay.h>

#include "rs232.h"
#include "config.h"
#include "debug.h"

/*
RFM12 (not RFM12B) needs pullups on FSK DATA nFFS
*/
//on PORTC:
#define RFM12_NINT 3
#define RFM12_SELECT 4
#define RFM12_MOSI 5
#define RFM12_MISO 6
#define RFM12_CLK 7
//on PORTE:
#define RFM12_RESET 5
//on PORTB:
#define RFM12_NIRQ 1

#define RFM12_TIMER TCE1
#define RFM12_TIMER_INT TCE1_OVF_vect
#define RFM12_TIMER_POWER PR_PRPE
#define RFM12_TIMER_POWER_BIT PR_TC1_bm

#define RFM12_DATABUFFERSIZE 128

volatile char rfm12_rxbuffer[RFM12_DATABUFFERSIZE];
volatile uint8_t rfm12_rxbufferidx;

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

static uint8_t rfm12_ready(void) {
	uint8_t status = 0;
	PORTC.OUTCLR = (1<<RFM12_SELECT);
	asm("nop");
	asm("nop");
	if (PORTC.IN & (1<<RFM12_MISO)) {
		status = 1;
	}
	PORTC.OUTCLR = (1<<RFM12_SELECT);
	return status;
}


//the irq pin gets low if the rfm12 has detected something interesting
//negate -> 1 = interesting.
static uint8_t rfm12_irqstatus(void) {
	return ((~(PORTB.IN >> RFM12_NIRQ)) & 1);
}

uint16_t rfm12_status(void) {
	return rfm12_command(0x0); //status read
}

uint16_t rfm12_showstatus(void) {
	uint8_t irqstate0 = rfm12_irqstatus();
	uint16_t status = rfm12_status();
	uint8_t irqstate1 = rfm12_irqstatus();
	if (g_settings.debugRs232 == 6) {
		char buffer[DEBUG_CHARS+1];
		buffer[DEBUG_CHARS] = '\0';
		snprintf_P(buffer, DEBUG_CHARS, PSTR("RFM12 stat: 0x%x irq %u->%u\r\n"), status, irqstate0, irqstate1);
		rs232_sendstring(buffer);
	}
	return status;
}

/*The interrupt has little purpose. It wakes up the RFM12 and can be
read by the status bit 0x800. So we can check if the RFM12 works.
A low pin sets the status bit. Setting the pin to high, holds the status bit
undil the next status read is performed.
*/
void rfm12_fireint(void) {
	PORTC.OUTCLR = (1<<RFM12_NINT);
	_delay_us(100.0);
	PORTC.OUTSET = (1<<RFM12_NINT);
}

ISR(PORTB_INT0_vect) {
	uint8_t status = rfm12_status();
	if (status & 0x8000) {
		//we got some data!
		uint16_t data = rfm12_command(0xB000);
		if (rfm12_rxbufferidx < RFM12_DATABUFFERSIZE) {
			rfm12_rxbuffer[rfm12_rxbufferidx] = (uint8_t)data;
			rfm12_rxbufferidx++;
		}
	}
}

char rfm12_update(void) {
	DEBUG_FUNC_ENTER(rfm12_update);
	char bufcop[RFM12_DATABUFFERSIZE];
	uint8_t s, i;
	if ((RFM12_TIMER_POWER & RFM12_TIMER_POWER_BIT) == 0) {
		if (rfm12_rxbufferidx) {
			//RFM12_TIMER.INTCTRLA &= ~TC_OVFINTLVL_LO_gc; //disable timer interrupt
			cli();
			memcpy(bufcop, (char*)rfm12_rxbuffer, rfm12_rxbufferidx);
			s = rfm12_rxbufferidx;
			rfm12_rxbufferidx = 0;
			sei();
			//RFM12_TIMER.INTCTRLA |= TC_OVFINTLVL_LO_gc; //enable timer interrupt
			if (g_settings.debugRs232 == 6) {
				rs232_sendstring_P(PSTR("RFM12 got: "));
				for (i = 0; i < s; i++) {
					rs232_puthex(bufcop[i]);
					if (isalnum(bufcop[i])) {
						rs232_putchar('-');
						rs232_putchar(bufcop[i]);
					}
					rs232_putchar(' ');
				}
				rs232_sendstring_P(PSTR("\r\n"));
			}
		}
		if (rfm12_ready()) {
			rs232_putchar('r');
		}
		rfm12_showstatus();
		//RFM12_TIMER.INTCTRLA &= ~TC_OVFINTLVL_LO_gc; //disable timer interrupt
		rfm12_command(0xCA81); // restart syncword detection:
		rfm12_command(0xCA83); // enable FIFO
		//RFM12_TIMER.INTCTRLA |= TC_OVFINTLVL_LO_gc; //enable timer interrupt
	}
	DEBUG_FUNC_LEAVE(rfm12_update);
	return 0;
}


ISR(RFM12_TIMER_INT) {
	if ((rfm12_ready()) && (rfm12_rxbufferidx < RFM12_DATABUFFERSIZE)) {
		rfm12_rxbuffer[rfm12_rxbufferidx] = rfm12_command(0xB000);
		rfm12_rxbufferidx++;
	}
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
	PORTC.PIN6CTRL = PORT_OPC_PULLUP_gc; //MISO, high impedance when select is high -> prevent floating pin
	PORTC.PIN7CTRL = PORT_OPC_TOTEM_gc;
	PORTB.PIN1CTRL = PORT_OPC_TOTEM_gc; //the interrupt input
	PORTC.DIRSET = (1<<RFM12_SELECT) | (1<<RFM12_MOSI) | (1<<RFM12_CLK) | (1<<RFM12_NINT);
	PORTC.OUTSET = (1<<RFM12_SELECT); //set ss to high = disable = sync device
	PORTC.OUTSET = (1<<RFM12_NINT);
	PORTB.OUTCLR = (1<<RFM12_NIRQ);
}

void rfm12_standby(void) {
	DEBUG_FUNC_ENTER(rfm12_standby);
	/*
	After power on, the consumption is appox 2mA.
	*/
	RFM12_TIMER.CTRLA = TC_CLKSEL_OFF_gc;
	RFM12_TIMER_POWER |= (RFM12_TIMER_POWER_BIT); //power the timer off
	rfm12_portinit();
	rfm12_reset();
	rfm12_command(0x8201); //write power management register with ext clock = off
	rfm12_showstatus();
	PORTC.OUTSET = (1<<RFM12_NINT); //wakeup is by a negative pulse
	rfm12_showstatus();
	DEBUG_FUNC_LEAVE(rfm12_standby);
}

void rfm12_init(void) {
	DEBUG_FUNC_ENTER(rfm12_init);
	rfm12_portinit();
	rfm12_reset();
	uint8_t irqstate0 = rfm12_irqstatus();
	if (!(rfm12_showstatus() & 0x4000) || (rfm12_showstatus() & 0x4000)) {
		rs232_sendstring_P(PSTR("RFM12 not pesent\r\n"));
		rfm12_standby(); //set ports back to idle...
		return;
	}
	uint8_t irqstate1 = rfm12_irqstatus();
	if ((!irqstate0) || (irqstate1)) {
		rs232_sendstring_P(PSTR("RFM12 nIRQ pin not working\r\n"));
	}
	rfm12_fireint();
	if (!(rfm12_showstatus() & 0x800) || (rfm12_showstatus() & 0x800)) {
		rs232_sendstring_P(PSTR("RFM12 nINT not working\r\n"));
		rfm12_standby(); //set ports back to idle...
		return;
	}
	//Low duty cycle command: Low battery detector and microcontroller clock divier command
	rfm12_command(0xC000); //low bat and MCU clock divider command: same as init value
	rfm12_showstatus();
	//configuration setting command: 12pF, 433MHz, enable data register, enable fifo
	rfm12_command(0x80D7);
	rfm12_showstatus();
	//data filter command: clock recovery auto lock control, slow mode, digital filter, data good level = 3
	rfm12_command(0xC2AB);
	rfm12_showstatus();
	//fifo and reset mode command: 8 bit rx = isr, synchron pattern, fifo fill after sync pattern detected, no high sensitive reset
	rfm12_command(0xCA81); //Set FIFO mode
	rfm12_showstatus();
	//low duty-cycle command: disable low duty
	rfm12_command(0xC800); //disable low duty cycle (once set, we dont get into low power again)
	//afc command: Keep the offset value independently from the state of he VDI signal, -10kHz to +7.5Khz range limit, no strome edge, high accuracy mode, enable frequency offset register, enable calculation of the offeset frequency
	rfm12_command(0xC4F7); //AFC settings: autotuning: -10kHz...+7,5kHz
	rfm12_showstatus();
	//----set channel. with benedict code, default: 1----
	rfm12_command(0xA000 | 1373); //benedict code, channel 1. frequency setting command, 1361: 433.4025MHz
	rfm12_showstatus();
	rfm12_command(0x9800 | (4<<4)); //tx configuration power command: power = 0dB,  mod = 4 -> (4+1)*15kHz = 75kHz frequency shift
	rfm12_showstatus();
	rfm12_command(0x9000 | (5<<5)); //receiver control command: not VDI output, fast valid identificator, bandwith=5->134kHz, gain= 0db, drssi=0->-103dBm threshold
	rfm12_showstatus();
	//----set baudrate of RFM12: default with benedict code: 20000----
	rfm12_command(0xC600 | 16); //data rate command: 16 results in ~21552b/s
	rfm12_showstatus();
	//---set to RX mode---
	//rfm12_command(0x82C8); //enable receiver chain + receiver baseband + crystal oscillator + crystal output
	rfm12_command(0x82CB); //enable receiver chain + receiver baseband + crystal oscillator + no crystal output + no battery detector + wakeup timer
	rfm12_showstatus();
	rfm12_command(0xCA81); //Set FIFO mode (again? why?)
	rfm12_showstatus();
	rfm12_command(0xCA83); //to restart detection: clear bit 1 and set bit 1
	rfm12_showstatus();
	//wake-up timer test wakeup
	rfm12_command(0xE014); //set wake up timer to 20ms
	rfm12_command(0x82C9); //enable all old settings + disable wakeup (0x2)
	rfm12_command(0x82CB); //enable all old settings + enable wakeup (0x2)
	waitms(30);
	if (!(rfm12_showstatus() & 0x1000)) {
		rs232_sendstring_P(PSTR("RFM12 wakeup not working\r\n"));
		rfm12_standby(); //set ports back to idle...
		return;
	}
	rfm12_command(0x82C9); //enable all old settings + disable wakeup (0x2)
	//enable polling timer
	RFM12_TIMER_POWER &= ~(RFM12_TIMER_POWER_BIT); //power the timer
	RFM12_TIMER.CTRLA = TC_CLKSEL_DIV1_gc; //divide by 1
	RFM12_TIMER.CTRLB = 0x00; // select Modus: Normal
	RFM12_TIMER.CTRLC = 0;
	RFM12_TIMER.CTRLD = 0; //no capture
	RFM12_TIMER.CTRLE = 0; //normal 16bit counter
	RFM12_TIMER.PER = (F_CPU/3000);   //we use approx 20kBaud -> 2500 byte/s -> sample 3000 times to get everything
	RFM12_TIMER.CNT = 0x0;    //reset counter
	RFM12_TIMER.INTCTRLA = TC_OVFINTLVL_LO_gc; // low prio interupt of overflow
	//initialize interrupt control
/*
	PORTB.PIN1CTRL = PORT_OPC_TOTEM_gc | PORT_ISC_FALLING_gc;
	PORTB.INTCTRL = 0x1; //low prio interrupt on int0
	PORTB.INT0MASK = 0x2; //use pin1
*/
	if (g_settings.debugRs232 == 6) {
		rs232_sendstring_P(PSTR("RFM12 init done\r\n"));
	}
	DEBUG_FUNC_LEAVE(rfm12_init);
}

