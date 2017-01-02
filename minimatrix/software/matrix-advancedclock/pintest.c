
#include <stdio.h>
#include <avr/io.h>
#include <util/delay_basic.h>
#include <avr/wdt.h>
#include <avr/pgmspace.h>

#include "pintest.h"
#include "rs232.h"
#include "main.h"


/*How to interpret the results:
Example print on serial port:
1(0-1): 0x2 (1)
8(1-0): 0x2 (0)
34(4-2): 0x5 (0)
35(4-3): 0x2 (0)
72(9-0): 0x2 (0)
73(9-1): 0x2 (0)
74(9-2): 0x2 (0)

interpretation (first number in brackets, see array ports, second in brackets
is the pin, left number outside the brackets is portnumber*8+pinnumber):
1(0-1): Pin 1. -> PORTA(0), PIN1
74(9-2): Pin 2 -> PORTK(9), PIN2

Bitmask as results, meaning:
0x01 -> Pin set as pull down, but not read as zero -> some external periphery held it high
0x02 -> Pin set as pull up, but not read as one -> some external periphery held it down
0x04 -> All pins are pull up, expect one, which is pull down -> now pin should be read as zero.
If 0x4 but not 0x1 is set, this usually means pins are soldered together by accident.
Last number in brackets:
(0) -> This error behaviour is ok, simply by external periphery not allowing the control
of the pin by pullups.
(1) -> This is likely an error and should be fixed.

In the example above this means all pins are ok, except the first one.
PORTA Pin1 is the LDR, and it pulls the pin down stronger than the internal
pull-up, so a zero is read when a one is expectd. Repeating the test in darkness
(LDR high resistance) will result in all ok.
*/


void pintest(uint8_t * resultmap) {
	PORT_t * ports[PINTEST_NUMPORTS] = {&PORTA, &PORTB, &PORTC, &PORTD, &PORTE, &PORTF, &PORTH, &PORTJ, &PORTK, &PORTQ};
	uint8_t originalstate[PINTEST_NUMPINS];
	uint8_t originaldirection[PINTEST_NUMPORTS];
	uint8_t i;
	uint8_t uartorigstate = USART.CTRLB;
	uint8_t rtcintorigstate = RTC_INTCTRL;
	USART.CTRLB &= ~((USART_RXEN_bm | USART_TXEN_bm)); //disable port overwrite
	_delay_loop_2(F_CPU/4/(BAUDRATE/10)*2); //wait two chars to transmit
	RTC_INTCTRL &= ~RTC_COMPINTLVL_HI_gc; //make sure the refresh interrupt wont change pins
	//configure direction to set as input
	for (i = 0; i < PINTEST_NUMPORTS; i++) {
		originaldirection[i] = ports[i]->DIR;
		ports[i]->DIR = 0x0;
	}
	//configure all pins as pulldown
	for (i = 0; i < PINTEST_NUMPINS; i++) {
		PORT_t * port = ports[i / 8];
		register8_t * pinctrl = (&(port->PIN0CTRL)) + (i & 7);
		originalstate[i] = *pinctrl;
		*pinctrl = PORT_OPC_PULLDOWN_gc;
		resultmap[i] = 0;
	}
	_delay_loop_2(10000);
	//check if all pins are read as 0
	for (i = 0; i < PINTEST_NUMPINS; i++) {
		PORT_t * port = ports[i / 8];
		if ((port->IN) & ( 1 << (i & 7))) {
			resultmap[i] |= 1;
		}
	}
	//configure all pins as pullup
	for (i = 0; i < PINTEST_NUMPINS; i++) {
		PORT_t * port = ports[i / 8];
		register8_t * pinctrl = (&(port->PIN0CTRL)) + (i & 7);
		*pinctrl = PORT_OPC_PULLUP_gc;
	}
	_delay_loop_2(10000);
	//check if all pins are read as 1
	for (i = 0; i < PINTEST_NUMPINS; i++) {
		PORT_t * port = ports[i / 8];
		if (!((port->IN) & ( 1 << (i & 7)))) {
			resultmap[i] |= 2;
		}
	}
	//configure one pin as pull down and check if down again
	for (i = 0; i < PINTEST_NUMPINS; i++) {
		PORT_t * port = ports[i / 8];
		register8_t * pinctrl = (&(port->PIN0CTRL)) + (i & 7);
		*pinctrl = PORT_OPC_PULLDOWN_gc;
		_delay_loop_2(10000); //some cylces to discharge caps if there are any
		if ((port->IN) & ( 1 << (i & 7))) {
			resultmap[i] |= 4;
		}
		*pinctrl = PORT_OPC_PULLUP_gc;
		wdt_reset();
	}
	//set pins back to original mode
	for (i = 0; i < PINTEST_NUMPINS; i++) {
		PORT_t * port = ports[i / 8];
		register8_t * pinctrl = (&(port->PIN0CTRL)) + (i & 7);
		*pinctrl = originalstate[i];
	}
	for (i = 0; i < PINTEST_NUMPORTS; i++) {
		ports[i]->DIR = originaldirection[i];
	}
	RTC_INTCTRL = rtcintorigstate;
	USART.CTRLB = uartorigstate;
}

uint8_t pintest_runtest(void) {
	uint8_t resultmap[PINTEST_NUMPINS];
	char buffer[32];
	pintest(resultmap);
	uint8_t i;
	uint8_t result = 0;
	uint8_t lresult = 0;
	for (i = 0; i < PINTEST_NUMPINS; i++) {
		uint8_t v = resultmap[i];
		if (v) {
			lresult = 1;
			if (((i == 1)  && (v & 4)) || //temperature -> does not get low alone due pullup (sometimes)
				  ((i == 8)  && (v & 2)) || //does not get high, due load from temperature sensor
				  ((i == 9)  && (v == 2)) || //RFM12 NIRQ can be either high or low (here low)
				  ((i == 9)  && (v == 5)) || //RFM12 NIRQ can be either high or low (here high)
				  ((i == 17) && (v & 2)) || //TSOP power, pullup can not power tsopt
				  ((i == 18) && (v & 2)) || //TSOP signal, pullup in TSOP powers device, seems to pull down sometimes
				  ((i == 30) && (v & 2)) || //pullup can not power DCF77 module
				  ((i == 31) && (v & 2)) || //DCF77 module seems to pull down output even if not enabled
				  ((i == 34) && (v == 5)) || //RXD wont get low if a transmitter is connected
				  ((i == 35) && (v == 5)) || //TXD might not get low due a pullup
				  ((i == 35) && (v == 2)) || //TXD might not get high due load from converter input
				  ((i == 36) && (v & 4)) || //LDR, migth be pulled by the other pins (or brighness influences result)
				  ((i == 72) && (v & 2)) || //Q0 -> crystal
				  ((i == 73) && (v & 2)) || //Q1 -> crystal
				  ((i == 74) && (v & 2))) //does not get high, due load from temperature sensor
			{
				lresult = 0;
			}
			if (lresult) {
				result = 1;
			}
			sprintf_P(buffer, PSTR("%i(%i-%i): 0x%x (%i)\r\n"), i, i / 8, i % 8, resultmap[i], lresult);
			rs232_sendstring(buffer);
		}
	}
	return result;
}