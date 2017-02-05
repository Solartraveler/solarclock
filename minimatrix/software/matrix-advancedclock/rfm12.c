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

TX POS  RX POS
0...4   ------: Sync data: 0xAA + 0xAA + 0xAA + 0x2D + 0xD4
5       0     : Status byte: bit 0 set: received ok
6       1     : Number of data bytes to send (id not counted!)
7...N   2...M : Data bytes: Only if number of bytes is > 0.
                If at least one data byte, first byte(7) is an id.
                Receiver accepts package when ID is different to the previous
                one.
                Transmitter increases ID if the reception has been actknowledged
                or the the actknowledge is not received within some timeout.
N+1     M+1   : CRC of bytes 5...N/0...M
N+2     ------: 0 - not received
Speciality in protocol: If one byte is 0x0 or 0xFF, the next byte will be
completely ignored (stuff byte).
The default values will result in a re-sent after 10ms if no ACT has been
received.

Sample reception:
00: Status
AA: Stuff byte because previous byte is 0
01: Number of data bytes to send = 1
05: The 5th packet to send
77: The Data - ascii w
2F: CRC (excluding stuff byte and excluding CRC itself)
E2: Garbage

https://lowpowerlab.com/forum/index.php?topic=115.0
The observation fits with my own one: 0x8201 sets it to low power mode.
I used 0x8201 and now it sleeps and draws only about 10µA

State machine in intterrupt timer:

Mode0: Listening, wait for RX -> Mode1
                        or Data to send -> Mode2
Mode1: Get data until package complete or too long.
        Package ID equal to the last one or CRC wrong? -> Mode0
        Otherwise Mode 2 with act bit set
Mode2: Build new Package (act bit set or not set)
       Set to TX mode -> Mode3
Mode3: Sent until package is out -> Mode4
Mode4: Wait until TX->RX timeout done -> Mode0
*/
#ifdef UNIT_TEST
//required for rand_r:
#define _POSIX_SOURCE
#endif


#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <stdint.h>

#ifndef UNIT_TEST
#include <avr/interrupt.h>
#include <avr/io.h>
#include <util/crc16.h>
#include <avr/cpufunc.h>
#include <avr/pgmspace.h>
#include "main.h"
#include <util/delay.h>
#include "rs232.h"

#define OPTIMIZER static inline

#else

#include "testcases/testrfm12.h"
#include "testcases/pccompat.h"


#define OPTIMIZER

#endif

#include "rfm12.h"
#include "config.h"
#include "debug.h"
#include "displayRtc.h"

/*
RFM12 (not RFM12B) would need pullups on FSK DATA nFFS (not used here)
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

#define RFM12_DATABUFFERSIZE 64



//#define RFM12_DEBUG

//these variables are only used within the interrupt routine, so no volatile
//for non interrupt code writeback-always, read always code required
uint8_t rfm12_rxbuffer[RFM12_DATABUFFERSIZE];
uint8_t rfm12_rxbufferidx;
uint8_t rfm12_txbuffer[RFM12_DATABUFFERSIZE];
uint8_t rfm12_txbufferidx;
uint8_t rfm12_txbufferlen;
uint8_t rfm12_waitcycles; //used for timing control and timeout
uint8_t rfm12_skipnext;
uint8_t rfm12_lastrxid;
uint8_t rfm12_lasttxid;
uint8_t rfm12_mode;
uint8_t rfm12_actreq;
uint8_t rfm12_actgot; //if 1, our last data package has been received successfully
uint8_t rfm12_retriesleft; //counts down the nuber of times the package is tried to be resend
//uint32_t fits to the unsigned long on the avr architecture and the unsigned int on the amd64 architecture as call for rand_r
uint32_t rfm12_randstate; //used for retransmission timeout. Avoids two senders always retrying at the same time.

//decoded packet (write by interrupt, read by update call, main never reads more than 4 chars)
volatile uint8_t rfm12_rxdata[RFM12_DATABUFFERSIZE];
volatile uint8_t rfm12_rxdataidx;

//send buffer (used as FIFO, get data from print function, forward to interrupt)
volatile uint8_t rfm12_txdata[RFM12_DATABUFFERSIZE];
volatile uint8_t rfm12_txdataidxwp; //write from print
volatile uint8_t rfm12_txdataidxrp; //read from interrupt

volatile uint16_t rfm12_timeoutstatus; //rfm12 status bits in the case of a timeout, for debug only

//some statistics
uint32_t rfm12_txpackets; //only counts packets with data. Simple act packages are not counted
uint32_t rfm12_txretries;
uint32_t rfm12_crcerrors; //rx
uint32_t rfm12_txaborts;

//internal state variable
uint8_t rfm12_passstate; //if 3, commands are accepted



#ifndef UNIT_TEST
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
		asm("nop");
		asm("nop");
		//rise clock
		PORTC.OUTSET = (1<<RFM12_CLK);
		//read data
		if (PORTC.IN & (1<<RFM12_MISO)) {
			indata |= 1;
		}
		asm("nop");
		asm("nop");
		//clock to low
		PORTC.OUTCLR = (1<<RFM12_CLK);
		outdata <<= 1;
	}
	asm("nop");
	asm("nop"); //give some time to the RFM12 for sampling
	PORTC.OUTSET = (1<<RFM12_SELECT);
	PORTC.PIN6CTRL = PORT_OPC_PULLUP_gc; //RFM12 pin is high impedance again
	return indata;
}

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
OPTIMIZER uint8_t rfm12_irqstatus(void) {
	return ((~(PORTB.IN >> RFM12_NIRQ)) & 1);
}

/*The interrupt has little purpose. It wakes up the RFM12 and can be
read by the status bit 0x800. So we can check if the RFM12 works.
A low pin sets the status bit. Setting the pin to high, holds the status bit
until the next status read is performed.
*/
OPTIMIZER void rfm12_fireint(void) {
	PORTC.OUTCLR = (1<<RFM12_NINT);
	_delay_us(100.0);
	PORTC.OUTSET = (1<<RFM12_NINT);
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

#endif

OPTIMIZER uint16_t rfm12_status(void) {
	return rfm12_command(0x0); //status read
}

OPTIMIZER uint16_t rfm12_showstatus(void) {
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

#ifndef UNIT_TEST

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

#endif


void rfm12_showstats(void) {
	//print the stats (last message over rfm12 itself (dont care about atomic read here, its just for the stats)
	if (rfm12_txpackets | rfm12_txretries | rfm12_crcerrors | rfm12_txaborts) {
		char buffer[DEBUG_CHARS+1];
		buffer[DEBUG_CHARS] = '\0';
		//casting is only done to satisfy x86 and amd64.
		snprintf_P(buffer, DEBUG_CHARS, PSTR("RFM12 tx:%lu retr:%lu, abort:%lu, crcerr: %lu\r\n"),
		 (long unsigned int)rfm12_txpackets, (long unsigned int)rfm12_txretries, (long unsigned int)rfm12_txaborts, (long unsigned int)rfm12_crcerrors);
		rs232_sendstring(buffer);
	}
}

void rfm12_update(void) {
	DEBUG_FUNC_ENTER(rfm12_update);
	uint8_t bufcop[RFM12_DATABUFFERSIZE];
	uint16_t statuscop;
	uint8_t s, i;
	if ((RFM12_TIMER_POWER & RFM12_TIMER_POWER_BIT) == 0) {
		if (rfm12_rxdataidx) {
			PMIC.CTRL &= ~PMIC_LOLVLEN_bm; //disable timer interrupt
			memcpy(bufcop, (uint8_t*)rfm12_rxdata, rfm12_rxdataidx);
			s = rfm12_rxdataidx;
			rfm12_rxdataidx = 0;
			statuscop = rfm12_timeoutstatus;
			rfm12_timeoutstatus = 0;
			PMIC.CTRL |= PMIC_LOLVLEN_bm; //enable timer interrupt
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
				if (statuscop & 0x2000) { //overflow or underrun flag
					rs232_sendstring_P(PSTR("RFM12 status: 0x"));
					rs232_puthex(statuscop>>8);
					rs232_puthex(statuscop);
					rs232_sendstring_P(PSTR("\r\n"));
				}
			}
			//put into key queue if password has been send
			if (rfm12_passstate >= 3) {
				uint8_t j = 0;
				for (i = 0; i < RFM12_KEYQUEUESIZE; i++) {
					if (g_state.rfm12keyqueue[i] == 0) {
						g_state.rfm12keyqueue[i] = bufcop[j];
						j++;
					}
					if (j >= s) {
						break;
					}
				}
			} else {
				for (i = 0; i < s; i++) {
					uint16_t expect = g_settings.rfm12passcode;
					if (rfm12_passstate == 0) expect /= 100;
					if (rfm12_passstate == 1) expect /= 10;
					expect = (expect % 10) + '0';
					if (bufcop[i] == expect) {
						if (g_settings.debugRs232 == 6) {
							rs232_sendstring_P(PSTR("RFM12 pass++\r\n"));
						}
						rfm12_passstate++;
					} else {
						rfm12_passstate = 0;
					}
				}
			}
		}
	}
	DEBUG_FUNC_LEAVE(rfm12_update);
}

//pgmspace = 1 -> read from flash. 0 -> read from RAM
uint8_t rfm12_sendMulti(const char * buffer, uint8_t size, uint8_t pgmspace) {
	uint8_t tx = rfm12_txdataidxwp;
	uint8_t txnext;
	uint8_t i;
	/*The logger uses rtc_8thcounter + 1 as timeout. By
		using +2 here, it should be safe to reach the timeout before
		calling this function and loosing data here due an imminent
		timeout.
	*/
	uint8_t timeout = (rtc_8thcounter + 2) % 8;
	if (rfm12_passstate < 3) {
		return 0;
	}
	for (i = 0; i < size; i++) {
		txnext = tx + 1;
		if (txnext >= RFM12_DATABUFFERSIZE) {
			txnext = 0;
		}
		while (txnext == rfm12_txdataidxrp) {
			//wait until buffer is free again or timeout occurred
			if (rtc_8thcounter == timeout) {
				return 0; //since we did not update rfm12_txdataidxwp, whole message gets removed
			}
		}
		if (!pgmspace) {
			rfm12_txdata[tx] = buffer[i]; //RAM
		} else {
			rfm12_txdata[tx] = pgm_read_byte(buffer + i); //FLASH
		}
		tx = txnext;
	}
	rfm12_txdataidxwp = tx;
	return 1;
}

uint8_t rfm12_send(const char * buffer, uint8_t size) {
	return rfm12_sendMulti(buffer, size, 0);
}

uint8_t rfm12_sendP(const char * buffer, uint8_t size) {
	return rfm12_sendMulti(buffer, size, 1);
}

OPTIMIZER uint8_t rfm12_restartrx(void) {
	if (rfm12_mode == 3) { //get away with our tx mode...
		rfm12_command(0x8209); //disable tx first
		rfm12_waitcycles = RFM12_WAITCYCLESRX;
		//rs232_putchar('k');
		return 4;
	}
	rfm12_skipnext = 0;
	rfm12_rxbufferidx = 0;
	rfm12_command(0x82C9); // enable rx
	rfm12_command(0xCA81); // restart syncword detection:
	rfm12_command(0xCA83); // enable FIFO
	if (rfm12_actgot == 0) {
		//the variable timeout should avoid both sides retransmitting at the same time
		rfm12_waitcycles = RFM12_ACTTIMEOUTCYCLES + (rand_r(&rfm12_randstate) % RFM12_ACTTIMEOUTCYCLES*2);
	}
	//rs232_putchar('l');
	return 0;
}

OPTIMIZER void rfm12_recalculatecrc(void) {
	uint8_t crc = 0;
	uint8_t pos = 7;
	if (rfm12_txbuffer[6]) {
		pos += rfm12_txbuffer[6] + 1;
	}
	uint8_t i;
	for (i = 5; i < pos; i++) {
		crc = _crc_ibutton_update(crc, rfm12_txbuffer[i]);
	}
	rfm12_txbuffer[pos] = crc;
}

OPTIMIZER uint8_t rfm12_assemblePackage(void) {
	/* There are four possible options:
		1. Build new data
		2. Resend old data without modification
		3. Resend old data, but with act set. Requireing recalculation of CRC
		4. Resend old data, but with act clear. Requireing recalculation of CRC
	*/
	if (rfm12_waitcycles) {
		return 2; //still in rx-> tx turn around waiting
	}
	if (rfm12_actgot) {
		//all well, build new data (1)
		rfm12_txbuffer[0] = 0xAA;
		rfm12_txbuffer[1] = 0xAA;
		rfm12_txbuffer[2] = 0xAA;
		rfm12_txbuffer[3] = 0x2D;
		rfm12_txbuffer[4] = 0xD4;
		rfm12_txbuffer[5] = rfm12_actreq;
		rfm12_txbuffer[6] = 0; //number of data bytes
		rfm12_actreq = 0;
		uint8_t bytesp = 0;
		if (rfm12_txdataidxwp != rfm12_txdataidxrp) {
			rfm12_lasttxid++;
			rfm12_txbuffer[7] = rfm12_lasttxid;
			bytesp++;
			uint8_t i;
			for (i = 8; i < RFM12_DATABUFFERSIZE - 2; i++) {
				rfm12_txbuffer[i] = rfm12_txdata[rfm12_txdataidxrp];
				rfm12_txdataidxrp = (rfm12_txdataidxrp + 1) % RFM12_DATABUFFERSIZE;
				bytesp++;
				if (rfm12_txdataidxwp == rfm12_txdataidxrp) {
					break;
				}
			}
			rfm12_actgot = 0; //we need to get an act for this package
			rfm12_retriesleft = RFM12_NUMERRETRIES;
			rfm12_txpackets++;
		}
		rfm12_txbufferlen = bytesp + 9;
		if (bytesp) {
			rfm12_txbuffer[6] = bytesp - 1; //number of bytes to send
		}
		rfm12_txbuffer[bytesp + 8] = 0; //terminate sending
		rfm12_recalculatecrc();
	} else {
		if (rfm12_retriesleft) {
			rfm12_retriesleft--;
			if (!rfm12_retriesleft) {
				//sent the data one last time.
				rfm12_actgot = 1; //Dont wait for the act a last time
				rfm12_txaborts++;
			}
		}
		rfm12_txretries++;
		if (rfm12_actreq != rfm12_txbuffer[5]) { //reset same data but with different status byte
			//we need to set or clear the bit and recalculate the sum (3, 4)
			rfm12_txbuffer[5] = rfm12_actreq;
			rfm12_actreq = 0;
			rfm12_recalculatecrc();
		} else {
			//simply resend the package (2)
		}
	}
	rfm12_txbufferidx = 0;
	rfm12_command(0x8239); // TX on
	return 3;
}

OPTIMIZER uint8_t rfm12_txData(void) {
	rfm12_waitcycles = RFM12_TIMEOUTSCYCLES; //reset timeout cycles
	if (rfm12_txbufferidx < rfm12_txbufferlen) {
		if (rfm12_skipnext) {
			rfm12_command(0xB8AA); //0xAA as dummy byte
			rfm12_skipnext = 0;
		} else {
			rfm12_command(0xB800 | rfm12_txbuffer[rfm12_txbufferidx]);
			if ((rfm12_txbuffer[rfm12_txbufferidx] == 0xFF) ||
					(rfm12_txbuffer[rfm12_txbufferidx] == 0)) {
				rfm12_skipnext = 1;
			}
			rfm12_txbufferidx++;
		}
		return 3; //continue sending
	}
	rfm12_waitcycles = RFM12_WAITCYCLESRX + 1; //1 is substracted within the same cycle
	rfm12_command(0x8209); // RX + TX off
	return 4; //go to wait mode to idle
}

OPTIMIZER uint8_t rfm12_rxData(void) {
	uint8_t nextmode = 0;
	rfm12_waitcycles = RFM12_TIMEOUTSCYCLES; //reset timeout cycles
	if (rfm12_rxbufferidx < RFM12_DATABUFFERSIZE) {
		rfm12_rxbuffer[rfm12_rxbufferidx] = rfm12_command(0xB000);
		nextmode = 1;
		if (!rfm12_skipnext) { //previous was not 0x0 or 0xFF
			if ((rfm12_rxbuffer[rfm12_rxbufferidx] == 0x0) || (rfm12_rxbuffer[rfm12_rxbufferidx] == 0xFF)) {
				rfm12_skipnext = 1;
			}
#if defined(RFM12_DEBUG)
			if (rfm12_rxdataidx < (RFM12_DATABUFFERSIZE)) {
				rfm12_rxdata[rfm12_rxdataidx++] = rfm12_rxbuffer[localidx];
			}
#endif
			rfm12_rxbufferidx++;
			if (rfm12_rxbufferidx > 2) { //we know the packet length
				const uint8_t datalen = rfm12_rxbuffer[1];
				uint8_t pkglen = 3;
				if (datalen) {
					pkglen += datalen + 1;
				}
				if (rfm12_rxbufferidx == pkglen) { //packet should be complete (data bytes+ number + status + crc)
					uint8_t i;
					uint8_t crc = 0;
					for (i = 0; i < pkglen - 1; i++) {
						crc = _crc_ibutton_update(crc, rfm12_rxbuffer[i]);
					}
					if (rfm12_rxbuffer[pkglen - 1] == crc) {
#ifndef RFM12_DEBUG
						if (datalen > 0) {
							//rs232_putchar('a');
							nextmode = 2; //need to send act
							rfm12_waitcycles = RFM12_WAITCYCLESTX;
							rfm12_command(0x8209); // RX + TX off
							rfm12_actreq = 1;
							if (rfm12_rxbuffer[2] != rfm12_lastrxid) {
								for (i = 0; i < datalen; i++) {
									if (rfm12_rxdataidx < RFM12_DATABUFFERSIZE) {
										rfm12_rxdata[rfm12_rxdataidx] = rfm12_rxbuffer[3+i];
										//rs232_putchar(rfm12_rxbuffer[3+i]);
										rfm12_rxdataidx++;
									}
								}
								rfm12_lastrxid = rfm12_rxbuffer[2];
							} else { //wrong index, sender did not get our act, we need to send act again
								//rs232_putchar('b'); . sender did not get our
							}
						} else { //no data...
							nextmode = 0; //we do not need to act packages with zero lenght
							rfm12_waitcycles = 0;
							//rs232_putchar('c');
						}
#endif
						rfm12_rxbufferidx = 0;
						rfm12_skipnext = 0;
						if (rfm12_rxbuffer[0] & 1) {
							rfm12_actgot = 1; //Other peer received our send packet right
						}
					} else { //wrong CRC
						nextmode = 0;
						rfm12_crcerrors++;
						//rs232_putchar('d');
						//rs232_puthex(crc);
					}
				}
			}
		} else {
			rfm12_skipnext = 0;
		}
	} else { //buffer overflow... restart
		nextmode = 0;
		//rs232_putchar('e');
	}
	if ((nextmode == 0) && (rfm12_mode != 1)) {
		rfm12_restartrx();
	}
	return nextmode;
}

#ifndef UNIT_TEST
ISR(RFM12_TIMER_INT)
#else
void ISR_RFM12_TIMER(void)
#endif
{
	DEBUG_FUNC_ENTER(rfm12_timer);
	PMIC.CTRL &= ~PMIC_LOLVLEN_bm;
	sei();
	if (rfm12_mode == 0) {
		//if ((rfm12_irqstatus() && (rfm12_status() & 0x8000)) { //does not work, why?
		if (rfm12_ready()) {
			rfm12_mode = rfm12_rxData();
		} else if (rfm12_waitcycles == 0) { //RX timeout reaced, or nothing to wait for
			if ((rfm12_txdataidxwp != rfm12_txdataidxrp) || (rfm12_actgot == 0)) {
				rfm12_command(0x8209); // RX + TX off
				rfm12_waitcycles = RFM12_WAITCYCLESRX + 1; //1 is substracted within the same cycle
				rfm12_mode = rfm12_assemblePackage();
			}
		}
	} else if (rfm12_mode == 1) {
		if (rfm12_ready()) {
			rfm12_mode = rfm12_rxData();
		}
	} else if (rfm12_mode == 2) {
		rfm12_mode = rfm12_assemblePackage();
	} else if (rfm12_mode == 3) {
		//rs232_putchar('g');
		if (rfm12_ready()) {
			//rs232_putchar('h');
			rfm12_mode = rfm12_txData();
		}
	}
	if (rfm12_waitcycles) {
		rfm12_waitcycles--;
		//rs232_putchar('T');
		if ((rfm12_waitcycles == 0) && ((rfm12_mode == 1) || (rfm12_mode >= 3))) {
			//rs232_putchar('t');
			rfm12_timeoutstatus = rfm12_status();
			rfm12_mode = rfm12_restartrx();
		}
	}
	cli();
	PMIC.CTRL |= PMIC_LOLVLEN_bm;
	DEBUG_FUNC_LEAVE(rfm12_timer);
}

void rfm12_init(void) {
	DEBUG_FUNC_ENTER(rfm12_init);
	rfm12_portinit();
	rfm12_reset();
	rfm12_rxbufferidx = 0;
	rfm12_txbufferidx = 0;
	rfm12_txbufferlen = 0;
	rfm12_rxdataidx = 0;
	rfm12_lastrxid = 255;
	rfm12_lasttxid = 0;
	rfm12_passstate = 0;
	rfm12_txdataidxwp = 0;
	rfm12_txdataidxrp = 0;
	rfm12_actreq = 0;
	rfm12_actgot = 1;
	rfm12_mode = 0;
	rfm12_waitcycles = 0;
	rfm12_retriesleft = 0;
	rfm12_txpackets = 0;
	rfm12_txretries = 0;
	rfm12_crcerrors = 0; //rx
	rfm12_txaborts = 0;
	if (!rfm12_randstate) {
		rfm12_randstate = g_settings.reboots ^ g_settings.usageseconds ^ g_state.time;
	}
	_MemoryBarrier(); //as the interrupt variables are not volatile, we have to write here
	uint8_t irqstate0 = rfm12_irqstatus();
	if (!(rfm12_showstatus() & 0x4000) || (rfm12_showstatus() & 0x4000)) {
		rs232_sendstring_P(PSTR("RFM12 not pesent\r\n"));
		rfm12_standby(); //set ports back to idle...
		DEBUG_FUNC_LEAVE(rfm12_init);
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
		DEBUG_FUNC_LEAVE(rfm12_init);
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
	//rfm12_command(0xC600 | 16); //data rate command: 16 results in ~21552b/s
	rfm12_command(0xC600 | ((344828UL/RFM12_BAUDRATE)-1));
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
		DEBUG_FUNC_LEAVE(rfm12_init);
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
	RFM12_TIMER.PER = (F_CPU/RFM12_TIMERCYCLES);   //we use approx 20kBaud -> 2500 byte/s -> sample 5000 times to get everything
	RFM12_TIMER.CNT = 0x0;    //reset counter
	RFM12_TIMER.INTCTRLA = TC_OVFINTLVL_LO_gc; // low prio interupt of overflow
	if (g_settings.debugRs232 == 6) {
		rs232_sendstring_P(PSTR("RFM12 init done\r\n"));
	}
	DEBUG_FUNC_LEAVE(rfm12_init);
}

