#ifndef LOGGER_H
#define LOGGER_H

#include <string.h>
#include <stdint.h>

#define LOG_MESSAGE_DCFSYNC 1
#define LOG_MESSAGE_REBOOT 2
#define LOG_MESSAGE_BATTERY 3


#define LOG_DATA_MAX 12

//according to the datasheet 1.7V, leave 100mV as reserve
#define EEPROM_MIN_VOLTAGE 1800

typedef struct __attribute__((packed)) {
	uint32_t newtime;
	uint16_t errorrate;
} lsync_t;

typedef struct __attribute__((packed)) {
	uint32_t rebootnum;
	uint8_t rebootcause;
	uint8_t debugtrace;
} lreboot_t;

typedef struct __attribute__((packed)) {
	uint16_t voltage;
	uint16_t batteryCharged;
	uint16_t solarcurrent;
	uint16_t temperature;
	uint32_t consumed;
} lbattery_t;

typedef struct __attribute__((packed)) {
	uint32_t id;
	uint32_t timestamp;
	uint8_t messagetype;
	union {
		uint8_t raw[LOG_DATA_MAX];
		lsync_t lsync;
		lreboot_t lreboot;
		lbattery_t lbattery;
	} data;
	uint16_t crc;
} logmessage_t ;



void logger_init(void);

void logger_writemessage(uint8_t messagetype, uint8_t * datafield, uint8_t datasize);

//hr == 1 -> human readable. 0 -> hex values only
void logger_print(uint8_t hr);
void logger_print_iter(void);

inline void loggerlog_synced(uint32_t newtime, uint16_t errorrate) {
	lsync_t lsync;
	lsync.newtime = newtime;
	lsync.errorrate = errorrate;
	logger_writemessage(LOG_MESSAGE_DCFSYNC, (uint8_t*)&lsync, sizeof(lsync));
}

inline void loggerlog_bootup(uint32_t rebootnum, uint8_t rebootcause, uint8_t debugtrace) {
	lreboot_t lreboot;
	lreboot.rebootnum = rebootnum;
	lreboot.rebootcause = rebootcause;
	lreboot.debugtrace = debugtrace;
	logger_writemessage(LOG_MESSAGE_REBOOT, (uint8_t*)&lreboot, sizeof(lreboot));
}

void loggerlog_batreport(void);


#endif

