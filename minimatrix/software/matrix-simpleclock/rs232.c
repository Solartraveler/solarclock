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

#include <avr/io.h>
#include <avr/interrupt.h>

#include "main.h"
#include <util/delay.h>

#include "rs232.h"
#include "config.h"

volatile uint8_t g_rs232starttransmit = 0;
volatile char g_rs232nextchar = 0;
volatile uint8_t g_rs232userx = 0;


ISR(RS232_ISR_VECT) {
	char nextchar = g_rs232nextchar;
	if (nextchar) {
		g_rs232nextchar = 0;
		USART.DATA = nextchar;
	} else {
		if ((g_rs232starttransmit == 0) && (g_rs232userx == 0)) {
			RS232_PORT.RS232_TX_PIN = PORT_ISC0_bm | PORT_ISC1_bm | PORT_ISC2_bm | PORT_OPC_PULLUP_gc;
			USART.CTRLB = 0;
			PWRSAVEREG |= PWRSAVEBIT;
		}
	}
}

void rs232_tx_init(void) {
	_delay_loop_2(F_CPU/BAUDRATE*30/4); //wait 30 bits to finish current transmit (1 in shift register, 1 in data register, one in softare wait variable)
	PWRSAVEREG &= ~PWRSAVEBIT;
	USART.CTRLA = USART_TXCINTLVL_LO_gc;
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
	RS232_PORT.DIRSET = 1<<3;
	_delay_loop_2(F_CPU/BAUDRATE*10/4); //wait 10 bits, since enabling produces some spikes
}

void rs232_rx_init(void) {
	rs232_sendstring_P(PSTR("RX enabled\r\n"));
	g_rs232userx = 1;
	rs232_tx_init();
	RS232_PORT.RS232_RX_PIN = PORT_OPC_TOTEM_gc;
	USART.CTRLB |= USART_RXEN_bm;
}

void rs232_rx_disable(void) {
	rs232_sendstring_P(PSTR("RX disabled\r\n"));
	g_rs232userx = 0;
	USART.CTRLB &= ~USART_RXEN_bm;
	RS232_PORT.RS232_RX_PIN = PORT_ISC0_bm | PORT_ISC1_bm | PORT_ISC2_bm; //disables input sense
}

void rs232_disable(void) {
	rs232_rx_disable();
	RS232_PORT.RS232_TX_PIN = PORT_ISC0_bm | PORT_ISC1_bm | PORT_ISC2_bm | PORT_OPC_PULLDOWN_gc;
	USART.CTRLB = 0;
	PWRSAVEREG |= PWRSAVEBIT;
}

void rs232_putchar(char ch) {
	g_rs232starttransmit = 1;
	if (PWRSAVEREG & PWRSAVEBIT) {
		rs232_tx_init();
	}
	while (g_rs232nextchar);
	cli();
	if (USART.STATUS & USART_DREIF_bm) {
		USART.DATA = ch;
	} else {
		g_rs232nextchar = ch;
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

static void rs232puthex0(uint8_t value) {
	if (value >= 10) {
		value += 'A' - 10;
	} else {
		value += '0';
	}
	rs232_putchar(value);
}

void rs232_puthex(uint8_t value) {
	rs232puthex0(value >> 4);
	rs232puthex0(value & 0xF);
}

void rs232_sendstring(char * string) {
	while (*string) {
		rs232_putchar(*string);
		string++;
	}
}

void rs232_sendstring_P(const char * string) {
	char c;
	if (g_settings.debugRs232) {
		while ((c = pgm_read_byte(string))) {
			rs232_putchar(c);
			string++;
		}
	}
}