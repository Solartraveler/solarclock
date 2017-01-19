#ifndef RFM12_H
#define RFM12_H

#include "config.h"

//call to get a key press received by the rfm12 device
void rfm12_update(void);

//call to disable everything and set RFM12 to low power mode (µA consumption)
void rfm12_standby(void);

//enable RFM12 in receiver mode
void rfm12_init(void);

//send a string by the RFM12 module (needs to be initialized before)
void rfm12_send(char * buffer, uint8_t size);

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



/* Internal functions, use for debugging only
uint16_t rfm12_status(void);

void rfm12_fireint(void);

void rfm12_reset(void);
uint16_t rfm12_command(uint16_t data);

void rfm12_showstatus(void);
*/


#endif

