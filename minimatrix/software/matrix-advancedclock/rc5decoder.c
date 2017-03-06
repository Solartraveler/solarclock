/************************************************************************/
/*                                                                      */
/*                      RC5 Remote Receiver                             */
/*                                                                      */
/*              Author: Peter Dannegger                                 */
/*                      danni@specs.de                                  */
/*                                                                      */
/*              Modified by Malte Marwedel 2014-10-01:                  */
/*               Version for XMEGA timer, tested with internal 2MHz RC. */
/*               Stopping the timer and using the pin isr for wakeup    */
/*               allows stopping the resonator while no signal gets     */
/*               received. This saves power. Note that only pin two of  */
/*               each port has full ISR port while not clocked.         */
/*                                                                      */
/************************************************************************/

#include <avr/io.h>
#include <avr/interrupt.h>
#include <stdlib.h>

//only needed for definition of F_CPU
#include "main.h"
#include "rc5decoder.h"
#include "debug.h"


#define RC5TIME 	1.778e-3		// 1.778msec
#define TIMERPERIOD 512
/* Multiplication values for original PULSE_MIN, PULSE_1_2, PULSE_MAX:
   0.4, 0.8, 1.2.
   A period of 1024 @2MHz works only in 80% of all key presses
*/
#define PULSE_MIN	(uchar)(F_CPU / TIMERPERIOD * RC5TIME * 0.3 + 0.5)
#define PULSE_1_2	(uchar)(F_CPU / TIMERPERIOD * RC5TIME * 0.7 + 0.5)
#define PULSE_MAX	(uchar)(F_CPU / TIMERPERIOD * RC5TIME * 1.3 + 0.5)

typedef unsigned char uchar;
typedef unsigned int uint;

uchar	rc5_bit;				// bit value
uchar	rc5_time;				// count bit time
uint	rc5_tmp;				// shift bits in

#define xRC5 2
#define xRC5_IN PORTC.IN

volatile uint16_t rc5_data;


static void rc5decode(void) {
  uint tmp = rc5_tmp;				// for faster access
  if( ++rc5_time > PULSE_MAX ){			// count pulse time
    if( !(tmp & 0x4000) && tmp & 0x2000 )	{// only if 14 bits received
       rc5_data = tmp;           //store  received value
    }
    tmp = 0;
    PR_PRPE |= PR_TC0_bm; //stop clock of timer E0
    PORTC.INT0MASK |= 0x4; //start pin interrupt
  }
  if( (rc5_bit ^ xRC5_IN) & 1<<xRC5 ){		// change detect
    rc5_bit = ~rc5_bit;				// 0x00 -> 0xFF -> 0x00

    if( rc5_time < PULSE_MIN )			// too short
      tmp = 0;

    if( !tmp || rc5_time > PULSE_1_2 ){		// start or long pulse time
      if( !(tmp & 0x4000) )			// not too many bits
        tmp <<= 1;				// shift
      if( !(rc5_bit & 1<<xRC5) )		// inverted bit
        tmp |= 1;				// insert new bit
      rc5_time = 0;				// count next pulse time
    }
  }
  rc5_tmp = tmp;
}

ISR(TCE0_OVF_vect) {
	DEBUG_INT_ENTER(rc5_Timer);
	rc5decode();
	DEBUG_INT_LEAVE(rc5_Timer);
}

ISR(PORTC_INT0_vect) {
	DEBUG_INT_ENTER(rc5_Pin);
	PORTC.INT0MASK &= ~0x4; //stop this interrupt
	rc5decode();
	PR_PRPE &= ~PR_TC0_bm; //start clock timer E0
	DEBUG_INT_LEAVE(rc5_Pin);
}

void rc5Init(void) {
	DEBUG_FUNC_ENTER(rc5_Init);
	//configure ports
	PORTC.DIRSET = 2;
	PORTC.OUTSET = 2; //PC1 reciver on off switch
	PORTC.PIN1CTRL = PORT_OPC_TOTEM_gc;
	PORTC.DIRCLR = 4; //input of reciver signal
	PORTC.OUTCLR = 4;
	PORTC.PIN2CTRL = PORT_OPC_TOTEM_gc | PORT_ISC_FALLING_gc;
	//configure the timer to overflow 3906/s (TIMERPERIOD = 512 @ 2MHz )
	PR_PRPE &= ~PR_TC0_bm; //clock timer E0
	TCE0.CTRLA = TC_CLKSEL_DIV1_gc; //divide by one
	TCE0.CTRLB = 0x00; // select Modus: Normal
	TCE0.PER = TIMERPERIOD;   //initial value
	TCE0.CNT = 0x0;    //reset counter
	TCE0.INTCTRLA = 0x1; // low prio interupt of overflow
	/*control interrupt (only works for pin2 -> i did not know that while
	 designing the PCB -> had pure luck using this. Other pins could loose
	 the ISR if the ISR condition disappears before the clock is running again)*/
	PORTC.INTCTRL = 0x1; //low prio interrupt on int0
	PORTC.INT0MASK = 0x4; //use pin2
	PR_PRPE |= PR_TC0_bm; //stop clock timer E0
	DEBUG_FUNC_LEAVE(rc5_Init);
}

uint16_t rc5getdata(void) {
	uint16_t val;
	cli();
	val = rc5_data;
	rc5_data = 0;
	sei();
	return val;
}

void rc5Stop(void) {
	DEBUG_FUNC_ENTER(rc5_Stop)
	PR_PRPE |= PR_TC0_bm; //stop clock of timer E0
	PORTC.DIRCLR = 2;
	PORTC.OUTCLR = 2; //PC1 on off switch
	PORTC.INTCTRL = 0;
	PORTC.INT0MASK = 0;
	PORTC.PIN1CTRL = PORT_OPC_PULLDOWN_gc;
	PORTC.PIN2CTRL = PORT_OPC_PULLDOWN_gc;
	DEBUG_FUNC_LEAVE(rc5_Stop);
}
