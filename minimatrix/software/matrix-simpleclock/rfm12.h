#ifndef RFM12_H
#define RFM12_H

uint16_t rfm12_status(void);

void rfm12_fireint(void);

void rfm12_standby(void);

void rfm12_init(void);
void rfm12_reset(void);
uint16_t rfm12_command(uint16_t data);

void rfm12_showstatus(void);

//dummy functions defined and used in the advanced version:
inline uint8_t rfm12_replicateready(void) {
	return 0;
}

inline uint8_t rfm12_txbufferfree(void) {
	return 0;
}

#define rfm12_send(X, Y)
#define rfm12_sendP(X, Y)

#endif

