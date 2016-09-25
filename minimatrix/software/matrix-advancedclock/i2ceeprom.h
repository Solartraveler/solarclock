#ifndef I2CEEPROM_H
#define I2CEEPROM_H

#include <inttypes.h>
uint8_t i2ceep_init();
uint8_t i2ceep_writebyte(uint16_t address, uint8_t value);
uint8_t i2ceep_readbyte(uint16_t address);
uint8_t i2ceep_writeblock(uint16_t address, uint8_t * databuffer, uint16_t datasize);
uint8_t i2ceep_readblock(uint16_t address, uint8_t * databuffer, uint16_t datasize);

void i2ceep_disable(void);


#endif
