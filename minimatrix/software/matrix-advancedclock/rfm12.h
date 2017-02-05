#ifndef RFM12_H
#define RFM12_H

#include "config.h"

//21552baud is too fast when other interrupts come between
//use 10776baud:
#define RFM12_BAUDRATE 10000

/*number of times the timer runs every second. Should be significant more than
the datarate in bytes per second, which is 21552b/s -> 2694byte/s.
But its useless to have it twiche the rate or faster.
*/
#define RFM12_TIMERCYCLES (RFM12_BAUDRATE/4)

#define RFM12_TIMERCYCLESPERBYTE (RFM12_TIMERCYCLES*8/RFM12_BAUDRATE)

/*between rx and tx switch, there should be a waiting time of 0.5ms.
  We use 1ms until tx, and 0.5ms until rx again. While we start sending,
  the other station must have already switched from TX to RX!
  1/0.5m = 2000. Rounding up
*/
#define RFM12_WAITCYCLESTX ((RFM12_TIMERCYCLES-1) / 1000 + 1)
#define RFM12_WAITCYCLESRX ((RFM12_TIMERCYCLES-1) / 2000 + 1)

/*We use 2x...3x the byte rate here. If the RFM12 device does not have data
or permits to get new data, something with the communication went wrong
and we get back to idle state.
*/
#define RFM12_TIMEOUTSCYCLES (RFM12_TIMERCYCLESPERBYTE*3)

/*Whithin this cycles an act should be started to be received, and if not,
  we should resend the last package.
	The /100 should be qual to a ~10ms timeout.
*/
#define RFM12_ACTTIMEOUTCYCLES ((RFM12_TIMERCYCLES-1) / 100 + 1)

//number of times a package is resent when there was no act
#define RFM12_NUMERRETRIES 20

//call to get a key press received by the rfm12 device
void rfm12_update(void);

//call to disable everything and set RFM12 to low power mode (µA consumption)
void rfm12_standby(void);

//enable RFM12 in receiver mode
void rfm12_init(void);

//send a string by the RFM12 module (needs to be initialized before)
//returns 1 if successfully written to buffer, 0 if sending aborted
uint8_t rfm12_send(const char * buffer, uint8_t size);

//as rfm12_send, but buffer must point to FLASH
uint8_t rfm12_sendP(const char * buffer, uint8_t size);

void rfm12_showstats(void);

inline char rfm12_getchar(void) {
	char key = 0;
	if (g_state.rfm12keyqueue[0]) {
		key = g_state.rfm12keyqueue[0];
		uint8_t i;
		for (i = 0; i < RFM12_KEYQUEUESIZE-1; i++) {
			g_state.rfm12keyqueue[i] = g_state.rfm12keyqueue[i+1];
		}
		g_state.rfm12keyqueue[3] = 0;
	}
	return key;
}

inline uint8_t rfm12_replicateready(void) {
	if ((g_settings.debugRs232 != 6) && (g_state.rfm12modeis)) {
		//we are sending only if we are not debugging the rfm12 module itself,
		//otherwise this might result in never ending data
		return 1;
	}
	return 0;
}


/* Internal functions, use for debugging only
uint16_t rfm12_status(void);

void rfm12_fireint(void);

void rfm12_reset(void);
uint16_t rfm12_command(uint16_t data);

void rfm12_showstatus(void);
*/


#endif

