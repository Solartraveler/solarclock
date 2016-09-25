
#include <avr/io.h>
#include <string.h>
#include <stdio.h>

#include "i2ceeprom.h"
#include "main.h"
#include "twi_master_driver.h"
#include "rs232.h"

#define TWI_PWRSAVEREG PR_PRPE
#define TWI_PWRSAVEBIT PR_TWI_bm
#define TWI TWIE
#define TWI_IO PORTE
#define TWI_DATA_PIN PIN1CTRL
#define TWI_CLK_PIN PIN0CTRL

//eeprom allows maximum 100kHz at 1.7V
#define TWI_BAUDRATE 80000
#define TWI_BAUDSETTING TWI_BAUD(F_CPU, TWI_BAUDRATE)
//fixed 1010 + A2 = VCC, A1, A0 = GND -> 1010100 -> 0x54
#define TWI_SLAVE_ADDRESS 0x54

TWI_Master_t g_twiMaster;

uint8_t i2ceep_init(void) {
	TWI_PWRSAVEREG &= ~PR_TWI_bm;
	TWI_IO.TWI_DATA_PIN = PORT_OPC_PULLUP_gc;
	TWI_IO.TWI_CLK_PIN = PORT_OPC_PULLUP_gc;
	TWI_MasterInit(&g_twiMaster, &TWI, TWI_MASTER_INTLVL_LO_gc, TWI_BAUDSETTING);
	PMIC.CTRL |= PMIC_LOLVLEN_bm;
	return 0;
}

uint8_t i2ceep_writebyte(uint16_t address, uint8_t value) {
	uint8_t buffer[3];
	buffer[0] = address>>8;
	buffer[1] = (uint8_t)address;
	buffer[2] = value;
	while (!TWI_MasterReady(&g_twiMaster));
	TWI_MasterWrite(&g_twiMaster, TWI_SLAVE_ADDRESS, buffer, 3);
	waitms(5); //byte or page write busy time
	return 0;
}

uint8_t i2ceep_readbyte(uint16_t address) {
	uint8_t buffer[2];
	buffer[0] = address>>8;
	buffer[1] = (uint8_t)address;
	while (!TWI_MasterReady(&g_twiMaster));
	do {
		TWI_MasterWriteRead(&g_twiMaster, TWI_SLAVE_ADDRESS, buffer, 2, 1);
		while (!TWI_MasterReady(&g_twiMaster));
	} while (g_twiMaster.bytesRead < 1);
	return g_twiMaster.readData[0];
}

uint8_t i2ceep_readblock(uint16_t address, uint8_t * databuffer, uint16_t datasize) {
	uint16_t bytesleft = datasize;
	uint8_t thisround;
	uint8_t wbuffer[2];
	while (bytesleft) {
		wbuffer[0] = address>>8;
		wbuffer[1] = (uint8_t)(address);
		if (bytesleft < TWIM_READ_BUFFER_SIZE) {
			thisround = bytesleft;
		} else {
			thisround = TWIM_READ_BUFFER_SIZE;
		}
		memset((void*)(g_twiMaster.readData), 0, TWIM_READ_BUFFER_SIZE);
		while (!TWI_MasterReady(&g_twiMaster));
		if (!TWI_MasterWriteRead(&g_twiMaster, TWI_SLAVE_ADDRESS, wbuffer, 2, thisround)) {
			rs232_sendstring_P(PSTR("Failed to TWI_MasterWriteRead...\r\n"));
			return 1; //failed to start sending
		}
		while (!TWI_MasterReady(&g_twiMaster));
		if (g_twiMaster.bytesRead != thisround) {
			rs232_sendstring_P(PSTR("Too few data...\r\n"));
			return 1;
		}
		memcpy(databuffer, (uint8_t*)(g_twiMaster.readData), thisround);
		databuffer += thisround;
		address += thisround;
		bytesleft -= thisround;
	}
	return 0;
}

/*
There are different page sizes on different EEPROMs,
all eeproms > 2KB seem to have at least 16byte page size, so using this here.
*/
#define I2CEEP_PAGESIZE 16


uint8_t i2ceep_writeblock(uint16_t address, uint8_t * databuffer, uint16_t datasize) {
	uint16_t bytesleft = datasize;
	uint8_t thisround;
	uint8_t wbuffer[TWIM_WRITE_BUFFER_SIZE];
	while (bytesleft) {
		wbuffer[0] = address>>8;
		wbuffer[1] = (uint8_t)(address);
		if (bytesleft < (TWIM_WRITE_BUFFER_SIZE-2)) {
			thisround = bytesleft;
		} else {
			thisround = (TWIM_WRITE_BUFFER_SIZE-2);
		}
		uint8_t pageleft = I2CEEP_PAGESIZE - (address & (I2CEEP_PAGESIZE - 1));
		if (thisround > pageleft) {
			thisround = pageleft;
		}
		memcpy(wbuffer + 2, databuffer, thisround);
		while (!TWI_MasterReady(&g_twiMaster));
		if (!TWI_MasterWrite(&g_twiMaster, TWI_SLAVE_ADDRESS, wbuffer, thisround + 2)) {
			rs232_sendstring_P(PSTR("Failed to TWI_MasterWrite...\r\n"));
			return 1; //failed to start sending
		}
		while (!TWI_MasterReady(&g_twiMaster));
		waitms(5); //byte or page write busy time
		databuffer += thisround;
		address += thisround;
		bytesleft -= thisround;
	}
	return 0;
}

void i2ceep_disable(void) {
	while (!TWI_MasterReady(&g_twiMaster));
	TWI_PWRSAVEREG |= PR_TWI_bm;
}

ISR(TWIE_TWIM_vect) {
	TWI_MasterInterruptHandler(&g_twiMaster);
}

