#ifndef RFM12_H
#define RFM12_H

//call to get a key press received by the rfm12 device
char rfm12_update(void);

//call to disable everything and set RFM12 to low power mode (µA consumption)
void rfm12_standby(void);

//enable RFM12 in receiver mode
void rfm12_init(void);

//send a string by the RFM12 module (needs to be initialized before)
void rfm12_send(char * buffer, uint8_t size);

/* Internal functions, use for debugging only
uint16_t rfm12_status(void);

void rfm12_fireint(void);

void rfm12_reset(void);
uint16_t rfm12_command(uint16_t data);

void rfm12_showstatus(void);
*/


#endif

