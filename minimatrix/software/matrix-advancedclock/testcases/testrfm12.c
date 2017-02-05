/* Matrix-Advancedclock
  (c) 2017 by Malte Marwedel
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
Functions with the prefix tr12a_ -> test rfm12 action -> do something with the
state machine.
Functions with the prefix tr12c_ -> test rfm12 check -> only check variables,
don't modify anything.
*/


#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "testrfm12.h"
#include "../rfm12.h"

#define BUFFERSIZE 32

extern uint8_t rfm12_rxbuffer[];
extern uint8_t rfm12_rxbufferidx;
extern uint8_t rfm12_txbuffer[];
extern uint8_t rfm12_txbufferidx;
extern uint8_t rfm12_txbufferlen;
extern uint8_t rfm12_waitcycles;
extern uint8_t rfm12_skipnext;
extern uint8_t rfm12_lastrxid;
extern uint8_t rfm12_lasttxid;
extern uint8_t rfm12_mode;
extern uint8_t rfm12_actreq;
extern uint8_t rfm12_actgot;
extern uint8_t rfm12_retriesleft;
extern volatile uint8_t rfm12_rxdata[];
extern volatile uint8_t rfm12_rxdataidx;
extern volatile uint8_t rfm12_txdata[];
extern volatile uint8_t rfm12_txdataidxwp; //write from print
extern volatile uint8_t rfm12_txdataidxrp; //read from interrupt
extern uint8_t rfm12_passstate; //if 3, commands are accepted

extern uint32_t rfm12_txpackets;
extern uint32_t rfm12_txretries;
extern uint32_t rfm12_crcerrors; //rx
extern uint32_t rfm12_txaborts;

uint8_t rfm12_assemblePackage(void);
uint8_t rfm12_txData(void);
uint8_t rfm12_rxData(void);
void ISR_RFM12_TIMER(void);

uint8_t rtc_8thcounter;

uint8_t g_debug;
uint16_t g_testrfm12status = 0x4800;
uint8_t g_testrfm12databyte;

uint16_t rfm12_command(uint16_t outdata) {
	uint16_t ret = 0;
	if (outdata == 0) { //get status command
		ret = g_testrfm12status;
		g_testrfm12status &= ~0x5800;
	}
	if (outdata == 0xB000) { //get data from fifo command
		//printf("get from rfm12\n");
		ret = g_testrfm12databyte;
		g_testrfm12status &= ~0x8000;
	}
	if ((outdata & 0xFF00) == 0xB800) { //write data to fifo command
		//printf("write to rfm12\n");
		g_testrfm12status &= ~0x8000;
	}
	if ((outdata & 0xFFF8) == 0x8208) { //rx + tx = off, but oscillator still running
		//printf("write to rfm12\n");
		g_testrfm12status &= ~0x8000;
	}
	return ret;
}

uint8_t rfm12_ready(void) {
	if (g_testrfm12status & 0x8000) {
		return 1;
	}
	return 0;
}

uint8_t rfm12_irqstatus(void) {
	if (g_testrfm12status & 0x800) {
		return 1;
	}
	return 0;
}

void rfm12_fireint(void) {
	g_testrfm12status |= 0x800;
}

void rfm12_reset(void) {
	g_testrfm12status = 0x4800; //reset + power on bit set
}

void waitms(uint16_t x) {
	UNREFERENCED_PARAMETER(x);
	g_testrfm12status |= 0x1000;
};

//1:1 copy from: http://www.nongnu.org/avr-libc/user-manual/group__util__crc.html#ga37b2f691ebbd917e36e40b096f78d996
uint8_t _crc_ibutton_update(uint8_t crc, uint8_t data) {
	uint8_t i;
	crc = crc ^ data;
	for (i = 0; i < 8; i++)
	{
		if (crc & 0x01)
			crc = (crc >> 1) ^ 0x8C;
		else
			crc >>= 1;
	}
	return crc;
}

int tr12c_memcmp(const char * fname, const int line, const volatile uint8_t * should, const volatile uint8_t * is, size_t len) {
	unsigned int i;
	for (i = 0; i < len; i++) {
		if (should[i] != is[i]) {
			printf("%s:%i error, At %u should 0x%x, is 0x%x\n", fname, line, i, should[i], is[i]);
			return 1;
		}
	}
	return 0;
}

int tr12c_assemble(const char * fname, const int line, uint8_t * expected, uint8_t len, uint8_t newtxid) {
	if (rfm12_txbufferidx != 0) {
		printf("%s:%i error, buffer index not zero!\n", fname, line);
		return 1;
	}
	if (rfm12_txbufferlen != len) {
		printf("%s:%i error, buffer len wrong. Should %u, is %u\n", fname, line, len, rfm12_txbufferlen);
		return 1;
	}
	if (rfm12_lasttxid != newtxid) {
		printf("%s:%i error, Tx id should be %u, is %u\n", fname, line, newtxid, rfm12_lasttxid);
		return 1;
	}
	return tr12c_memcmp(fname, line, expected, rfm12_txbuffer, len);
}

uint8_t tr12_getcrc(uint8_t * buffer, uint8_t len) {
	uint16_t i;
	uint8_t crc = 0;
	for (i = 0; i < len; i++) {
		crc = _crc_ibutton_update(crc, buffer[i]);
	}
	return crc;
}

int tr12c_mode(const char * fname, const int line, uint8_t modeexpected) {
	if (rfm12_mode != modeexpected) {
		printf("%s:%i error, did not get to mode %i, is %i, timeout=%i\n", fname, line, modeexpected, rfm12_mode, rfm12_waitcycles);
		return 1;
	}
	return 0;
}

int tr12c_rxstate(const char * fname, const int line, uint8_t mode, uint8_t skipnext, const uint8_t * buffercontext, uint8_t rxindex) {
	if (tr12c_mode(fname, line, mode)) return 1;
	if (rfm12_skipnext != skipnext) {
		printf("%s:%i error, skipnext should %i is %i\n", fname, line, skipnext, rfm12_skipnext);
		return 1;
	}
	if (rfm12_rxbufferidx != rxindex) {
		printf("%s:%i error, buffer idx should %i is %i\n", fname, line, rxindex, rfm12_rxbufferidx);
		return 1;
	}
	return tr12c_memcmp(fname, line, buffercontext, rfm12_rxbuffer, rxindex);
}

uint8_t tr12_simtxbyte(uint8_t data) {
	g_testrfm12databyte = data;
	g_testrfm12status |= 0x8000; //data waiting
	if ((data == 0x0) || (data == 0xFF)) {
		return 1; //dummy required
	}
	return 0;
}

uint8_t tr12c_txdataidxwp(const char * fname, const int line, uint8_t should) {
	if (rfm12_txdataidxwp != should) {
		printf("%s:%i error, rfm12_txdataidxwp should %u, is %u\n", fname, line, should, rfm12_txdataidxwp);
		return 1;
	}
	return 0;
}

uint8_t tr12c_actgot(const char * fname, const int line, uint8_t expected) {
	if (rfm12_actgot != expected) {
		if (rfm12_actgot) {
			printf("%s:%i error, rfm12_actgot should be clear\n", fname, line);
		} else {
			printf("%s:%i error, rfm12_actgot should be set\n", fname, line);
		}
		return 1;
	}
	return 0;
}

uint8_t tr12c_retrycounter(const char * fname, const int line, uint8_t expected) {
	if (rfm12_retriesleft != expected) {
		printf("%s:%i error, rfm12_retiesleft should be %i, is %i\n", fname, line, expected, rfm12_retriesleft);
		return 1;
	}
	return 0;
}

uint8_t tr12_buildtxpackage(uint8_t * buffer, uint8_t bufferlen, const char * text, uint8_t actgot, uint8_t id) {
	uint8_t datalen = strlen(text);
	if (bufferlen <= 9+datalen) {
		printf("%s:%i error, give bigger buffer\n", __func__, __LINE__);
	}
	buffer[0] = 0xAA;
	buffer[1] = 0xAA;
	buffer[2] = 0xAA;
	buffer[3] = 0x2D;
	buffer[4] = 0xD4;
	buffer[5] = actgot;
	buffer[6] = datalen;
	buffer[7] = id;
	memcpy(buffer+8, text, datalen);
	buffer[8+datalen] = tr12_getcrc(buffer + 5, 3 + datalen);
	buffer[9+datalen] = 0;
	return datalen;
}

uint8_t tr12a_waittxtimeout(const char * fname, const int line) {
	uint8_t errors = 0;
	uint8_t i;
	for (i = 0; i < RFM12_WAITCYCLESTX; i++) {
		errors |= tr12c_mode(fname, line, 2);
		ISR_RFM12_TIMER();
	}
	return errors;
}

uint8_t tr12a_waitrxtimeout(const char * fname, const int line) {
	uint8_t errors = 0;
	uint8_t i;
	for (i = 0; i < RFM12_WAITCYCLESRX; i++) {
		errors |= tr12c_mode(fname, line, 4);
		ISR_RFM12_TIMER();
	}
	return errors;
}

uint8_t tr12a_waitnoactimeout(const char * fname, const int line) {
	int timeout;
	uint8_t errors = 0;
	uint8_t timeoutcycles = rfm12_waitcycles;
	if ((timeoutcycles < RFM12_ACTTIMEOUTCYCLES) || (timeoutcycles > RFM12_ACTTIMEOUTCYCLES*3)) {
		printf("%s:%i error, timeout %i out of bounds\n", fname, line, timeoutcycles);
		return 1;
	}
	for (timeout = 0; timeout < timeoutcycles; timeout++) {
		errors |= tr12c_mode(fname, line, 0); //we wait for an act package until the timeout hits us
		ISR_RFM12_TIMER();
	}
	return errors;
}

uint8_t tr12a_sentdata(const char * fname, const int line, uint8_t * expected, uint8_t datalen) {
	uint8_t i;
	uint8_t errors = 0;
	for (i = 0; i < datalen; i++) {
		errors |= tr12c_mode(fname, line, 3);
		g_testrfm12status = 0x8000;
		if (rfm12_txbufferidx != i) {
			printf("%s:%i error, rfm12_txbufferidx should %u, is %u\n", fname, line, i, rfm12_txbufferidx);
			errors = 1;
		}
		ISR_RFM12_TIMER();
		if ((expected[i] == 0xFF) || (expected[i] == 0x00)) {
			if (!rfm12_skipnext) {
				printf("%s:%i error, did not fill stuff byte\n", fname, line);
			} else {
				errors |= tr12c_mode(fname, line, 3);
				g_testrfm12status = 0x8000;
				ISR_RFM12_TIMER();
			}
		}
	}
	return errors;
}

uint8_t tr12a_recdata(const char * fname, const int line, uint8_t * expected, uint8_t datalen, uint8_t continuemode) {
	uint8_t errors = 0;
	uint8_t i;
	for (i = 0; i < datalen; i++) {
		uint8_t stuffrequired = tr12_simtxbyte(expected[i]);
		uint8_t expmode = 1;
		uint8_t expindex = i + 1;
		ISR_RFM12_TIMER();
		if(i == (datalen - 1)) {
			expmode = continuemode;
			if (expmode != 1) { //if we stay in mode 1, this usually means a rfm12 error
				expindex = 0;
			}
		}
		errors |= tr12c_rxstate(fname, line, expmode, stuffrequired, expected, expindex);
		if (stuffrequired) {
			tr12_simtxbyte(0xAA);
			ISR_RFM12_TIMER();
			errors |= tr12c_rxstate(fname, line, 1, 0, expected, i+1);
		}
	}
	return errors;
}

int testrfm12_init(void) {
	rfm12_init();
	if (TCE1.INTCTRLA != TC_OVFINTLVL_LO_gc) {
		printf("testrfm12_init error, timer not active. Should 0x%x, is 0x%x\n", TC_OVFINTLVL_LO_gc, TCE1.INTCTRLA);
		return 1;
	}
	if (rfm12_lastrxid == 0) {
		printf("testrfm12_init error, last rx id should not be 0\n");
		return 1;
	}
	return tr12c_actgot(__func__, __LINE__,  1);
}

int testrfm12_idle(void) {
	rfm12_mode = 0;
	rfm12_waitcycles = 0;
	rfm12_actgot = 1;
	rfm12_txdataidxwp = 0;
	rfm12_txdataidxrp = 0;
	g_testrfm12status = 0;
	ISR_RFM12_TIMER();
	if (rfm12_mode != 0) {
		printf("testrfm12_idle error\n");
		return 1;
	}
	return 0;
}

//empty act package
int testrfm12_assemble1(void) {
	uint8_t expected[] = {0xAA, 0xAA, 0xAA, 0x2D, 0xD4, 0x1, 0x0, 0xEE, 0x0};
	expected[7] = tr12_getcrc(expected + 5, 2);
	rfm12_actgot = 1;
	rfm12_actreq = 1;
	rfm12_waitcycles = 0;
	uint8_t lasttxid = rfm12_lasttxid = 1;
	rfm12_txdataidxwp = 0;
	rfm12_txdataidxrp = 0;
	rfm12_txbufferidx = 1;
	rfm12_assemblePackage();
	return tr12c_assemble(__func__, __LINE__,  expected, sizeof(expected), lasttxid);
}

//package with single byte - w
int testrfm12_assemble2(void) {
	uint8_t expected[] = {0xAA, 0xAA, 0xAA, 0x2D, 0xD4, 0x0, 0x1, 0x05, 'w', 0x2F, 0x0};
	rfm12_actgot = 1;
	rfm12_actreq = 0;
	rfm12_waitcycles = 0;
	uint8_t lasttxid = rfm12_lasttxid = 4;
	rfm12_txdataidxwp = 1;
	rfm12_txdataidxrp = 0;
	rfm12_txdata[0] = 'w';
	rfm12_txbufferidx = 1;
	rfm12_assemblePackage();
	return tr12c_assemble(__func__, __LINE__,  expected, sizeof(expected), lasttxid + 1);
}

//package resend test without act modification
int testrfm12_assemble3(void) {
	uint8_t expected[] = {0xAA, 0xAA, 0xAA, 0x2D, 0xD4, 0x0, 0x1, 0x05, 'w', 0x2F, 0x0};
	memcpy(rfm12_txbuffer, expected, sizeof(expected));
	rfm12_txbufferlen = sizeof(expected);
	rfm12_actgot = 0;
	rfm12_actreq = 0;
	rfm12_waitcycles = 0;
	uint8_t lasttxid = rfm12_lasttxid = 5;
	rfm12_txdataidxwp = 1;
	rfm12_txdataidxrp = 0;
	rfm12_txdata[0] = 'e';
	rfm12_txbufferidx = 1;
	rfm12_assemblePackage();
	return tr12c_assemble(__func__, __LINE__,  expected, sizeof(expected), lasttxid);
}

//package resend test with act modification
int testrfm12_assemble4(void) {
	uint8_t expected[] = {0xAA, 0xAA, 0xAA, 0x2D, 0xD4, 0x0, 0x1, 0x05, 'w', 0x2F, 0x0};
	memcpy(rfm12_txbuffer, expected, sizeof(expected));
	rfm12_txbufferlen = sizeof(expected);
	expected[5] = 0x1;
	expected[9] = tr12_getcrc(expected + 5, 4);
	rfm12_actgot = 0;
	rfm12_actreq = 1;
	uint8_t lasttxid = rfm12_lasttxid = 5;
	rfm12_txdataidxwp = 1;
	rfm12_txdataidxrp = 0;
	rfm12_txdata[0] = 'e';
	rfm12_txbufferidx = 1;
	rfm12_assemblePackage();
	return tr12c_assemble(__func__, __LINE__,  expected, sizeof(expected), lasttxid);
}

//gets one char and should send an act for it
int testrfm12_rxchar(void) {
	uint8_t errors = 0;
	uint8_t expected[] = {0x0, 0x1, 0x05, 'w', 0x2F};
	rfm12_init();
	rfm12_lastrxid = 4;
	uint8_t lasttxid = rfm12_lasttxid = 42;
	rfm12_rxdata[0] = 'e';
	errors |= tr12a_recdata(__func__, __LINE__, expected, sizeof(expected), 2);
	//rx completed here. check intermediate buffer for char
	if (rfm12_rxdata[0] != expected[3]) {
		printf("%s:%i error, did not write to rxdata\n", __func__, __LINE__);
		errors = 1;
	}
	if (rfm12_rxdataidx != 1) {
		printf("%s:%i error, data index should be 1, is %i\n", __func__, __LINE__, rfm12_rxdataidx);
		errors = 1;
	}
	//wait for permission for tx timeout
	errors |= tr12a_waittxtimeout(__func__, __LINE__);
	//prepare the answer
	ISR_RFM12_TIMER();
	uint8_t expectedanswer[] = {0xAA, 0xAA, 0xAA, 0x2D, 0xD4, 0x1, 0x0, 0xEE, 0x0};
	expectedanswer[7] = tr12_getcrc(expectedanswer + 5, 2);
	errors |= tr12c_assemble(__func__, __LINE__, expectedanswer, sizeof(expectedanswer), lasttxid);
	//send the answer
	errors |= tr12a_sentdata(__func__, __LINE__, expectedanswer, sizeof(expectedanswer));
	//wait for permission for rx timeout
	errors |= tr12a_waitrxtimeout(__func__, __LINE__);
	//go back to receive
	errors |= tr12c_mode(__func__, __LINE__, 0);
	//stay in receive
	ISR_RFM12_TIMER();
	errors |= tr12c_mode(__func__, __LINE__, 0);
	return errors;
}

//sends a message with the rfm12
int testrfm12_txtext(void) {
	const char * hello = "Hello world\n";
	const char * welcome = "Refugees welcome\n";
	uint8_t errors = 0;
	rfm12_init();
	rfm12_lastrxid = 4;
	uint8_t lasttxid = rfm12_lasttxid = 42;
	rfm12_passstate = 3;
	rfm12_send(hello, strlen(hello));
	errors |= tr12c_txdataidxwp(__func__, __LINE__, strlen(hello));
	errors |= tr12c_mode(__func__, __LINE__, 0);
	errors |= tr12c_memcmp(__func__, __LINE__, (uint8_t*)hello, rfm12_txdata, strlen(hello));
	ISR_RFM12_TIMER();
	errors |= tr12c_mode(__func__, __LINE__, 2);
	errors |= tr12a_waittxtimeout(__func__, __LINE__);
	//now there should be an assembled TX package
	uint8_t expectedpackage[BUFFERSIZE]; // = {0xAA, 0xAA, 0xAA, 0x2D, 0xD4, 0x00};
	uint8_t datalen = tr12_buildtxpackage(expectedpackage, BUFFERSIZE, hello, 0, lasttxid + 1);
	errors |= tr12c_assemble(__func__, __LINE__, expectedpackage, datalen + 10, lasttxid + 1);
	errors |= tr12c_mode(__func__, __LINE__, 3);
	//now send it out
	errors |= tr12a_sentdata(__func__, __LINE__, expectedpackage, datalen + 10);
	//wait for permission for rx timeout
	errors |= tr12a_waitrxtimeout(__func__, __LINE__);
	errors |= tr12c_mode(__func__, __LINE__, 0); //wait until go to rx mode
	errors |= tr12c_actgot(__func__, __LINE__, 0);
	errors |= tr12c_txdataidxwp(__func__, __LINE__, rfm12_txdataidxrp);
	if (rfm12_txbufferlen != datalen + 10) {
		printf("%s error, rfm12_txbufferlen should %i, is %i\n", __func__, datalen + 10, rfm12_txbufferlen);
		errors = 1;
	}
	//put something in the queue which should not be sent until act is received
	rfm12_send(welcome, strlen(welcome));
	errors |= tr12c_txdataidxwp(__func__, __LINE__, strlen(hello) + strlen(welcome));
	//now wait for act package
	uint8_t actpackage[] = {0x1, 0x0, 0xFF};
	actpackage[2] = tr12_getcrc(actpackage, 2);
	errors |= tr12a_recdata(__func__, __LINE__, actpackage, sizeof(actpackage), 0);
	errors |= tr12c_actgot(__func__, __LINE__, 1);
	//now the next data from the buffer should be prepared for sending
	ISR_RFM12_TIMER();
	errors |= tr12c_mode(__func__, __LINE__, 2);
	errors |= tr12a_waittxtimeout(__func__, __LINE__);
	errors |= tr12c_mode(__func__, __LINE__, 3);
	return errors;
}

//sends a message with the rfm12
int testrfm12_txtextnoact(void) {
	const char * hello = "Hello World\n";
	const char * welcome = "Refugees welcome\n";
	uint8_t errors = 0;
	rfm12_init();
	uint8_t lasttxid = rfm12_lasttxid;
	rfm12_passstate = 3;
	rfm12_send(hello, strlen(hello));
	errors |= tr12c_txdataidxwp(__func__, __LINE__, strlen(hello));
	errors |= tr12c_mode(__func__, __LINE__, 0);
	errors |= tr12c_memcmp(__func__, __LINE__, (uint8_t*)hello, rfm12_txdata, strlen(hello));
	ISR_RFM12_TIMER();
	errors |= tr12c_mode(__func__, __LINE__, 2);
	errors |= tr12a_waittxtimeout(__func__, __LINE__);
	//now there should be an assembled TX package
	uint8_t expectedpackage[BUFFERSIZE];
	uint8_t datalen = tr12_buildtxpackage(expectedpackage, BUFFERSIZE, hello, 0, lasttxid + 1);
	errors |= tr12c_assemble(__func__, __LINE__, expectedpackage, datalen + 10, lasttxid + 1);
	errors |= tr12c_mode(__func__, __LINE__, 3);
	errors |= tr12c_retrycounter(__func__, __LINE__, RFM12_NUMERRETRIES);
	//now send it out
	errors |= tr12a_sentdata(__func__, __LINE__, expectedpackage, datalen + 10);
	//wait for permission for rx timeout
	errors |= tr12a_waitrxtimeout(__func__, __LINE__);
	errors |= tr12c_mode(__func__, __LINE__, 0); //wait until go to rx mode
	errors |= tr12c_actgot(__func__, __LINE__, 0);
	errors |= tr12c_txdataidxwp(__func__, __LINE__, rfm12_txdataidxrp);
	if (rfm12_txbufferlen != datalen + 10) {
		printf("%s:%i error, rfm12_txbufferlen should %i, is %i\n", __func__, __LINE__,  datalen + 10, rfm12_txbufferlen);
		errors = 1;
	}
	//put something in the queue which should not be sent until act is received
	rfm12_send(welcome, strlen(welcome));
	errors |= tr12c_txdataidxwp(__func__, __LINE__, strlen(hello) + strlen(welcome));
	//now wait for act package
	errors |= tr12a_waitnoactimeout(__func__, __LINE__);
	//now a resend should be initiated
	ISR_RFM12_TIMER(); //transition to new mode possible after timeout done
	errors |= tr12c_mode(__func__, __LINE__, 2);
	errors |= tr12a_waittxtimeout(__func__, __LINE__);
	errors |= tr12c_assemble(__func__, __LINE__, expectedpackage, datalen + 10, lasttxid + 1);
	errors |= tr12c_mode(__func__, __LINE__, 3);
	errors |= tr12c_retrycounter(__func__, __LINE__, RFM12_NUMERRETRIES -1);
	errors |= tr12a_sentdata(__func__, __LINE__, expectedpackage, datalen + 10);
	errors |= tr12a_waitrxtimeout(__func__, __LINE__);
	errors |= tr12c_mode(__func__, __LINE__, 0); //wait until go to rx mode
	errors |= tr12c_actgot(__func__, __LINE__, 0);
	errors |= tr12c_txdataidxwp(__func__, __LINE__, strlen(hello) + strlen(welcome));
	errors |= tr12a_waitnoactimeout(__func__, __LINE__);
	//and a last time
	rfm12_retriesleft = 1;
	ISR_RFM12_TIMER(); //transition to new mode possible after timeout done
	errors |= tr12c_mode(__func__, __LINE__, 2);
	errors |= tr12a_waittxtimeout(__func__, __LINE__);
	errors |= tr12c_assemble(__func__, __LINE__, expectedpackage, datalen + 10, lasttxid + 1);
	errors |= tr12c_actgot(__func__, __LINE__, 1); //just behave as if we got the act in the timeout case
	errors |= tr12c_mode(__func__, __LINE__, 3);
	errors |= tr12c_retrycounter(__func__, __LINE__, 0);
	errors |= tr12a_sentdata(__func__, __LINE__, expectedpackage, datalen + 10);
	errors |= tr12a_waitrxtimeout(__func__, __LINE__);
	errors |= tr12c_mode(__func__, __LINE__, 0); //wait until go to rx mode
	errors |= tr12c_actgot(__func__, __LINE__, 1); //just behave as if we got the act in the timeout case
	//now there should be the next message (welcome) in our buffer
	errors |= tr12c_txdataidxwp(__func__, __LINE__, strlen(hello) + strlen(welcome));
	ISR_RFM12_TIMER();
	datalen = tr12_buildtxpackage(expectedpackage, BUFFERSIZE, welcome, 0, lasttxid + 2);
	errors |= tr12c_mode(__func__, __LINE__, 2);
	errors |= tr12a_waittxtimeout(__func__, __LINE__);
	errors |= tr12c_assemble(__func__, __LINE__, expectedpackage, datalen + 10, lasttxid + 2);
	errors |= tr12c_txdataidxwp(__func__, __LINE__, rfm12_txdataidxrp);
	if (rfm12_txaborts != 1) {
		printf("%s:%i error, did not increment stat rfm12_txaborts counter, is %i\r\n", __func__, __LINE__, rfm12_txaborts);
		errors = 1;
	}
	return errors;
}


//sends a message with the rfm12, but at some point the rfm12 does not answer any more
int testrfm12_txerror(void) {
	const char * hello = "Hello world\n";
	uint8_t errors = 0;
	rfm12_init();
	rfm12_lastrxid = 4;
	uint8_t lasttxid = rfm12_lasttxid = 42;
	rfm12_passstate = 3;
	rfm12_send(hello, strlen(hello));
	errors |= tr12c_txdataidxwp(__func__, __LINE__, strlen(hello));
	errors |= tr12c_mode(__func__, __LINE__, 0);
	errors |= tr12c_memcmp(__func__, __LINE__, (uint8_t*)hello, rfm12_txdata, strlen(hello));
	ISR_RFM12_TIMER();
	errors |= tr12c_mode(__func__, __LINE__, 2);
	errors |= tr12a_waittxtimeout(__func__, __LINE__);
	//now there should be an assembled TX package
	uint8_t expectedpackage[BUFFERSIZE];
	uint8_t datalen = tr12_buildtxpackage(expectedpackage, BUFFERSIZE, hello, 0, lasttxid + 1);
	errors |= tr12c_assemble(__func__, __LINE__, expectedpackage, datalen + 10, lasttxid + 1);
	errors |= tr12c_mode(__func__, __LINE__, 3);
	//now send it out
	errors |= tr12a_sentdata(__func__, __LINE__, expectedpackage, 7);
	uint8_t i;
	//printf("timeout=%i, max=%i, mode=%i\n", rfm12_waitcycles, RFM12_TIMEOUTSCYCLES, rfm12_mode);
	/*We need to substract to from the timeout, because one is always substracted within
	  the same cycle. And the next mode is already entered within the cycle where the
	  timeout reaches zero.
	*/
	for (i = 0; i < (RFM12_TIMEOUTSCYCLES-2); i++) {
		ISR_RFM12_TIMER();
		//printf("timeout=%i, max=%i, mode=%i\n", rfm12_waitcycles, RFM12_TIMEOUTSCYCLES, rfm12_mode);
		errors |= tr12c_mode(__func__, __LINE__, 3);
	}
	ISR_RFM12_TIMER();
	errors |= tr12a_waitrxtimeout(__func__, __LINE__);
	errors |= tr12c_mode(__func__, __LINE__, 0);
	return errors;
}

//gets a message with the rfm12, but at some point the rfm12 stops receiveing
int testrfm12_rxerror(void) {
	uint8_t errors = 0;
	uint8_t expected[] = {0x0, 0x1, 0x05, 'w', 0x2F};
	rfm12_init();
	rfm12_lastrxid = 4;
	rfm12_lasttxid = 42;
	rfm12_rxdata[0] = 'e';
	//we abort the sending two chars before the message has finished
	errors |= tr12a_recdata(__func__, __LINE__, expected, sizeof(expected)-2, 1);
	uint8_t i;
	for (i = 0; i < (RFM12_TIMEOUTSCYCLES-2); i++) {
		ISR_RFM12_TIMER();
		//printf("timeout=%i, max=%i, mode=%i\n", rfm12_waitcycles, RFM12_TIMEOUTSCYCLES, rfm12_mode);
		errors |= tr12c_mode(__func__, __LINE__, 1);
	}
	ISR_RFM12_TIMER();
	errors |= tr12c_mode(__func__, __LINE__, 0);
	return errors;
}

int testrfm12(void) {
	int errors = 0;
	errors += testrfm12_init();
	errors += testrfm12_idle();
	errors += testrfm12_rxchar();
	errors += testrfm12_assemble1();
	errors += testrfm12_assemble2();
	errors += testrfm12_assemble3();
	errors += testrfm12_assemble4();
	errors += testrfm12_txtext();
	errors += testrfm12_txtextnoact();
	errors += testrfm12_txerror();
	errors += testrfm12_rxerror();
	return errors;
}
