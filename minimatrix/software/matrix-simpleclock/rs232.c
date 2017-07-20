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
#include <avr/interrupt.h>
#include <string.h>

#include "main.h"
#include <util/delay.h>

#include "rs232.h"
#include "config.h"
#include "rfm12.h"

#define RS232_TXBUFFERSIZE 64

volatile uint8_t g_rs232starttransmit = 0;
volatile char g_rs232buffer[RS232_TXBUFFERSIZE];
volatile char g_rs232bufferwp; //points to the next memory field to be written
volatile char g_rs232bufferrp;//points to the memory field to be read next
volatile uint8_t g_rs232userx = 0;

/*Transmit complete interrupt (not data register empty).
  So the HW buffer is not used and we can immediately power the HW down.
*/
ISR(RS232_ISR_VECT) {
	if ((g_rs232bufferwp != g_rs232bufferrp) && (USART.STATUS & USART_DREIF_bm)) {
		uint8_t bufferrp = g_rs232bufferrp;
		USART.DATA = g_rs232buffer[bufferrp];
		bufferrp++;
		if (bufferrp >= RS232_TXBUFFERSIZE) {
			bufferrp = 0;
		}
		g_rs232bufferrp = bufferrp;
	} else {
		if ((g_rs232starttransmit == 0) && (g_rs232userx == 0)) {
			RS232_PORT.DIRCLR = 1<<RS232_TX_PIN_NR;
			RS232_PORT.RS232_TX_PIN = PORT_ISC0_bm | PORT_ISC1_bm | PORT_ISC2_bm | PORT_OPC_PULLUP_gc; //pull up + input disable
			USART.CTRLB = 0; //disable TX
			PWRSAVEREG |= PWRSAVEBIT;
		}
	}
}

void rs232_tx_init(void) {
	if (!(PWRSAVEREG & PWRSAVEBIT)) {
		return; //if not powered down, no need to re-initalize
	}
	PWRSAVEREG &= ~PWRSAVEBIT;
	USART.CTRLA = USART_TXCINTLVL_MED_gc;
	USART.CTRLC = USART_CHSIZE0_bm | USART_CHSIZE1_bm | USART_SBMODE_bm; //8bit 2stop
#if (BAUDRATE <= 9600)
	USART.BAUDCTRLA = (uint8_t)(F_CPU/(16*(uint32_t)BAUDRATE) - 1);
	USART.BAUDCTRLB = (F_CPU/(16*BAUDRATE) - 1) >> 8;
	USART.CTRLB = USART_TXEN_bm;
#elif BAUDRATE == 38400
	USART.BAUDCTRLA = 12; //perfect would be 12,02
	USART.BAUDCTRLB = 0xF0; //represents -1 as BSCALE
	USART.CTRLB = USART_TXEN_bm | USART_CLK2X_bm;
#else
	//use twice the speed
	USART.BAUDCTRLA = (uint8_t)(F_CPU/(8*(uint32_t)BAUDRATE) - 1);
	USART.BAUDCTRLB = (F_CPU/(8*BAUDRATE) - 1) >> 8;
	USART.CTRLB = USART_TXEN_bm | USART_CLK2X_bm;
#endif

	RS232_PORT.RS232_TX_PIN = PORT_OPC_TOTEM_gc;
	RS232_PORT.DIRSET = 1<<RS232_TX_PIN_NR;
	//_delay_loop_2(F_CPU/BAUDRATE*10/4); //wait 10 bits, since enabling produces some spikes
}

void rs232_rx_init(void) {
	rs232_sendstring_P(PSTR("RX enabled\r\n"));
	g_rs232userx = 1;
	rs232_tx_init();
	RS232_PORT.RS232_RX_PIN = PORT_OPC_PULLUP_gc; //otherwise a disconnected input gets garbage
	USART.CTRLB |= USART_RXEN_bm;
}

void rs232_rx_disable(void) {
	rs232_sendstring_P(PSTR("RX disabled\r\n"));
	g_rs232userx = 0;
	USART.CTRLB &= ~USART_RXEN_bm;
	RS232_PORT.RS232_RX_PIN = PORT_ISC0_bm | PORT_ISC1_bm | PORT_ISC2_bm | PORT_OPC_PULLUP_gc; //disables input sense + pullup
}

void rs232_disable(void) {
	rs232_rx_disable();
	RS232_PORT.DIRCLR = 1<<RS232_TX_PIN_NR;
	RS232_PORT.RS232_TX_PIN = PORT_ISC0_bm | PORT_ISC1_bm | PORT_ISC2_bm | PORT_OPC_PULLUP_gc;
	USART.CTRLB = 0;
	PWRSAVEREG |= PWRSAVEBIT;
}

void rs232_putchar(char ch) {
	uint8_t wp = g_rs232bufferwp;
	uint8_t wpnext = wp + 1;
	g_rs232starttransmit = 1; //prevents the transmitter from powering down itself
	if (PWRSAVEREG & PWRSAVEBIT) {
		rs232_tx_init();
	}
	if (wpnext >= RS232_TXBUFFERSIZE) {
		wpnext = 0;
	}
	while (wpnext == g_rs232bufferrp); //wait for room in buffer
	cli();
	if ((USART.STATUS & USART_DREIF_bm) && (wp == g_rs232bufferrp)) {
		USART.DATA = ch;
	} else {
		g_rs232buffer[wp] = ch;
		g_rs232bufferwp = wpnext;
	}
	g_rs232starttransmit = 0;
	sei();
}

char rs232_getchar(void) {
	if (USART.STATUS & USART_RXCIF_bm) {
		return USART.DATA;
	} else {
		return 0;
	}
}

static uint8_t rs232_makehex(uint8_t value) {
	if (value >= 10) {
		value += 'A' - 10;
	} else {
		value += '0';
	}
	return value;
}

void rs232_sendstring(char * string) {
	const char * index = string;
	uint8_t len = 0;
	if (g_settings.debugRs232) {
		while (*index) {
			rs232_putchar(*index);
			index++;
		}
		len = index - string;
	}
	if (rfm12_replicateready()) {
		if (!len) {
			len = strlen(string);
		}
		if (rfm12_txbufferfree() >= len) {
			rfm12_send(string, len);
		}
	}
}

void rs232_puthex(uint8_t value) {
	char buffer[3];
	buffer[0] = rs232_makehex(value >> 4);
	buffer[1] = rs232_makehex(value & 0xF);
	buffer[2] = '\0';
	rs232_sendstring(buffer);
}

void rs232_sendstring_P(const char * string) {
	char c;
	const char * index = string;
	uint8_t len = 0;
	if (g_settings.debugRs232) {
		while ((c = pgm_read_byte(index))) {
			rs232_putchar(c);
			index++;
		}
		len = index - string;
	}
	if (rfm12_replicateready()) {
		if (!len) {
			len = strlen_P(string);
		}
		if (rfm12_txbufferfree() >= len) {
			rfm12_sendP(string, len);
		}
	}
}

void rs232_stall(void) {
	/*Since this is single thread, no need to worry about rs232_putchar
	  only the interrupt is of interest. */
	USART.CTRLA = 0; //disable all interrupts of rs232
	/*According to appnote AVR1207 the flag waits until the complete HW FIFO is empty
	  Up to three chars can be in the HW buffer, so 0.5ms per char -> wait 1.6ms max
	  Or there can be no char ever transmitted in which case USART_TXCIF_bm never
	  gets set and the timeout prevents a deadlock.
	*/
	uint8_t timeout = 160;
	if ((PWRSAVEREG & PWRSAVEBIT) == 0) {
		while (((USART.STATUS & USART_TXCIF_bm) == 0) && (timeout)) {
			_delay_us(10.0);
			timeout--;
		}
	}
}

void rs232_continue(void) {
	if ((PWRSAVEREG & PWRSAVEBIT) == 0) {
		//should automatically call the int handler which looks into the software FIFO
		USART.CTRLA = USART_TXCINTLVL_MED_gc;
	}
}
