#ifndef RS232_H
#define RS232_H

#define USART USARTE0


#define PWRSAVEREG PR_PRPE
#define PWRSAVEBIT PR_USART0_bm
#define RS232_PORT PORTE
#define RS232_RX_PIN PIN2CTRL
#define RS232_TX_PIN PIN3CTRL
#define RS232_TX_PIN_NR 3
#define RS232_ISR_VECT USARTE0_TXC_vect


#define BAUDRATE 19200L

#include <avr/pgmspace.h>

void rs232_tx_init(void);

//rx init includes tx init
void rs232_rx_init(void);

//disables rx, allowing transmitter to get disabled if not used
void rs232_rx_disable(void);

//disables rx and tx
void rs232_disable();

//does not replicate the data to the rfm12
void rs232_putchar(char ch);

char rs232_getchar(void);

//if enabled, replicates the data to the rfm12
void rs232_puthex(uint8_t value);
//if enabled, replicates the data to the rfm12
void rs232_sendstring(char * string);
//if enabled, replicates the data to the rfm12
void rs232_sendstring_P(const char * string);

/*stalls the sending of data. Useful for changeing the CPU clock.
 Note that printing data during a stall might end up in a deadlock
*/
void rs232_stall(void);

//continure the rs232. Call after stall.
void rs232_continue(void);

#endif
