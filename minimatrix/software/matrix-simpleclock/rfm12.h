#ifndef RFM12_H
#define RFM12_H

uint16_t rfm12_status(void);

void rfm12_fireint(void);

void rfm12_standby(void);

void rfm12_init(void);
void rfm12_reset(void);
uint16_t rfm12_command(uint16_t data);

void rfm12_showstatus(void);

#endif

