/* Matrix-Advancedclock
  (c) 2014-2017 by Malte Marwedel
  www.marwedels.de/malte

  This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*/

#include <util/crc16.h>
#include <string.h>
#include <stdio.h>


#include "logger.h"
#include "config.h"
#include "i2ceeprom.h"
#include "rs232.h"
#include "debug.h"
#include "main.h"
#include "displayRtc.h"
#include "timeconvert.h"
#include "rfm12.h"


#define LOGGER_BASE_OFFSET 16

#define LOGGER_MESSAGE_HUMANREAD_MAX 150
#define LOGGER_MESSAGE_HEX_MAX 50

/*
Data format of the log:
The first 16 bytes are reserved as signature and permanent data. Currently
only 4 bytes are used. 12 can be used for other purpose.
Change this by editing LOGGER_BASE_OFFSET.
The first three bytes must have the text "Log".
The fourth byte must indicate the size of the eeprom in 1kiB^n
Ex 1 = 1kiB, 3 = 4kiB etc.
If the signature is not found, the size is determined and the signature written
to the eeprom. Determining the size destroys some of the old content of the
eeprom.
Every data entry in the eeprom as a fixed size of sizeof(logmessage_t).
Writing is done by round robin from low to high address. If an entry does not
fit in the reaming bytes at the end, the whole log entry is written at the
beginning. So there might be some bytes permanently unused at the eeprom end.
The log works without storing an index. Instead every entry has a upcounting
number and on startup the entry with the highest numer is searched. As the 32bit
number can be higher than the maximum writes to the eeprom, overflowing of
this counter should be no problem.
*/


static uint16_t calcCrc(uint8_t * data, uint8_t size) {
	uint8_t i;
	uint16_t crc = 0;
	for (i = 0; i < size; i++) {
		crc = _crc16_update(crc, data[i]);
	}
	return crc;
}

uint8_t logger_entryread(uint16_t entry, logmessage_t * data) {
	if (entry >= g_state.logger.maxentries) {
		return 1;
	}
	uint16_t address = entry*sizeof(logmessage_t) + LOGGER_BASE_OFFSET;
	if (i2ceep_readblock(address, (uint8_t *)data, sizeof(logmessage_t))) {
		return 1; // 1= failure
	}
	uint16_t crc = calcCrc((uint8_t*)data, sizeof(logmessage_t) - sizeof(uint16_t));
	if (crc == data->crc) {
		return 0;
	}
	return 1;
}

static void logger_seekmax(void) {
	uint16_t rangemax = g_state.logger.maxentries - 1;
	uint16_t rangemin = 0;
	uint16_t tocheck = 0;
	uint32_t minnewid = 0;
	logmessage_t message;
	if (g_settings.debugRs232 == 0xC) {
			DbgPrintf_P(PSTR("Log can store %u entries\r\n"), g_state.logger.maxentries);
	}
	while (rangemax > rangemin) {
		if (logger_entryread(tocheck, &message)) { //not a valid log entry
			if (tocheck == 0) {
				break; //zero entrys in log
			}
			//not a valid entry
			rangemax = tocheck;
		} else { //valid log entry found
			if (message.id >= minnewid) { //seek up
				minnewid = message.id + 1;
				rangemin = tocheck;
			} else { //smaller than preivous entry
				rangemax = tocheck;
			}
		}
		tocheck = rangemin + ((rangemax - rangemin) / 2);
		if (g_settings.debugRs232 == 0xC) {
			DbgPrintf_P(PSTR("rangemin:%u rangemax:%u tocheck:%u minnewid:%u\r\n"), rangemin, rangemax, tocheck, minnewid);
		}
	}
	//update state struct with result
	g_state.logger.nextid = minnewid;
	g_state.logger.nextentry = tocheck + 1;
	if ((tocheck + 1) >= g_state.logger.maxentries) {
		g_state.logger.nextentry = 0;
	}
	DbgPrintf_P(PSTR("Start log at %u id: %lu\r\n"), g_state.logger.nextentry, (unsigned long)minnewid);
}

static void logger_initializemem(void) {
	uint8_t preparesig[4] = {'L', 'o', 'g', 0};
	uint8_t preparesigcheck[4];
	i2ceep_writeblock(0, preparesig, 4);
	i2ceep_readblock(0, preparesigcheck, 4);
	if (memcmp(preparesig, preparesigcheck, 4)) {
		DbgPrintf_P(PSTR("R/W I2C EEPROM failed\r\n"));
		return;
	}
	uint16_t taddr = 2048; //if at >= 2k address ends in first address -> overflow found
	uint8_t sizeindex = 1; //1k<<1 = minimum size we want to use
	while (taddr) {
		if (g_settings.debugRs232 == 0xC) {
			DbgPrintf_P(PSTR("taddr: %u index: %u\r\n"), taddr, sizeindex);
		}
		i2ceep_writebyte(taddr + 3, sizeindex);
		uint8_t first = i2ceep_readbyte(3);
		if (g_settings.debugRs232 == 0xC) {
			DbgPrintf_P(PSTR("got %u\r\n"), first);
		}
		if (first != 0) {
			//we had an address overflow -> size found
			break;
		}
		taddr *= 2;
		sizeindex++;
	}
	g_state.logger.ksize = 1<<sizeindex;
	if (g_state.logger.ksize >= 64) {
		i2ceep_writebyte(3, sizeindex); //all other eeproms had their value already written by the overflow, but for 64k a test up to 128k would be needed
	}
	g_state.logger.maxentries = ((1024 * g_state.logger.ksize) - LOGGER_BASE_OFFSET) / sizeof(logmessage_t);
	DbgPrintf_P(PSTR("New log in EEPROM with %ikB\r\n"), g_state.logger.ksize);
}

void logger_init(void) {
	i2ceep_init();
	uint8_t signature[4];
	//check if we get data
	if (i2ceep_readblock(0, signature, 4) == 0) {
		//signature[0] = 0x0; //enable to reset eeprom and reinitialize size
		if ((memcmp(signature, "Log", 3) == 0) && (signature[3] > 0)) {
			//we have a working log structure :)
			g_state.logger.ksize = 1 << signature[3];
			g_state.logger.maxentries = ((1024 * g_state.logger.ksize) - LOGGER_BASE_OFFSET) / sizeof(logmessage_t);
			if (g_settings.debugRs232 == 0xC) {
				rs232_sendstring_P(PSTR("Seek...\r\n"));
			}
			logger_seekmax();
		} else {
			//empty log structure, initialize
			if (g_settings.debugRs232 == 0xC) {
				rs232_sendstring_P(PSTR("Init...\r\n"));
			}
			logger_initializemem();
		}
	} else {
		rs232_sendstring_P(PSTR("No I2C EEPROM to read\r\n"));
	}
	i2ceep_disable();
}

void logger_writemessage(uint8_t messagetype, uint8_t * datafield, uint8_t datasize) {
	if (g_state.logger.ksize == 0) {
		return; //no eeprom, no log
	}
	if ((g_state.batVoltage < EEPROM_MIN_VOLTAGE) && (g_state.batVoltage != 0)) {
		return; //prevent non working writes, on startup, batVoltage has not been updated yet (=0)
	}
	i2ceep_init();
	logmessage_t lmt;
	//build message
	lmt.id = g_state.logger.nextid;
	lmt.timestamp = g_state.time;
	lmt.messagetype = messagetype;
	memcpy(lmt.data.raw, datafield, datasize);
	if (datasize < LOG_DATA_MAX) {
		memset(lmt.data.raw + datasize, 0, LOG_DATA_MAX - datasize);
	}
	lmt.crc = calcCrc((uint8_t *)&lmt, sizeof(logmessage_t) - sizeof(uint16_t));
	//target in eeprom
	uint16_t eepromaddr = g_state.logger.nextentry*sizeof(logmessage_t) + LOGGER_BASE_OFFSET;
	//debug
	if (g_settings.debugRs232 == 0xC) {
		DbgPrintf_P(PSTR("Log%lu type:%u at %u\r\n"), g_state.logger.nextid, messagetype, eepromaddr);
	}
	//write to eeprom
	if (i2ceep_writeblock(eepromaddr, (uint8_t *)&lmt, sizeof(logmessage_t)) == 0) {
		//success
		if (g_settings.debugRs232 == 0xC) {
			rs232_sendstring_P(PSTR(" success\r\n"));
		}
		g_state.logger.nextid++;
		g_state.logger.nextentry++;
		if (g_state.logger.nextentry >= g_state.logger.maxentries) {
			g_state.logger.nextentry = 0;
		}
	} else {
		if (g_settings.debugRs232 == 0xC) {
			rs232_sendstring_P(PSTR(" failed\r\n"));
		}
	}
	i2ceep_disable();
}

static void loggerlog_batreportsub(uint16_t voltage, uint16_t batteryCharged,
  uint16_t solarcurrent, uint16_t temperature, uint32_t consumed) {
	lbattery_t lbattery;
	lbattery.voltage = voltage;
	lbattery.batteryCharged = batteryCharged;
	lbattery.solarcurrent = solarcurrent;
	lbattery.temperature = temperature;
	lbattery.consumed = consumed;
	logger_writemessage(LOG_MESSAGE_BATTERY, (uint8_t*)&lbattery, sizeof(lbattery));
}

void loggerlog_batreport(void) {
	uint32_t chargedmah = g_state.batteryCharged/(60UL*60UL);
	uint64_t consumedmah = g_state.consumptionBefore60/(60ULL*60ULL*1000ULL);
	loggerlog_batreportsub(g_state.batVoltage, chargedmah, g_state.chargerCurrent,
	                    g_state.gradcelsius10, consumedmah);
}

static void logger_print_entry(uint16_t entryidx) {
	logmessage_t entry;
	uint8_t * datau8 = (uint8_t*)&entry;
	uint8_t i;
	if (logger_entryread(entryidx, &entry)) {
		return; //invalid or empty entry
	}
	for (i = 0; i < sizeof(logmessage_t); i++) {
		rs232_puthex(datau8[i]);
	}
	if (g_state.logger.reportingmode == 2) {
		//human readable appending
		uint8_t year2digit, month, day;
		uint32_t timeofday = dateFromTimestamp(entry.timestamp, &day, &month, &year2digit, NULL);
		uint8_t s = timeofday  % 60;
		uint8_t m = (timeofday / 60) % 60;
		uint8_t h = timeofday / (60*60);
		uint16_t year = year2digit + 2000;
		month++;
		day++;
		DbgPrintf_P(PSTR(" %7lu %04u-%02u-%02u %2u:%02u:%02u "), (unsigned long)entry.id, year, month, day, h, m, s);
		if (entry.messagetype == LOG_MESSAGE_DCFSYNC) {
			uint16_t syncerror = entry.data.lsync.errorrate;
			uint32_t newtime = entry.data.lsync.newtime;
			int32_t delta = newtime - entry.timestamp;
			DbgPrintf_P(PSTR("SYNC, delta: %lis, error: %u"), (signed long)delta, syncerror);
		}
		if (entry.messagetype == LOG_MESSAGE_REBOOT) {
			uint32_t rebootnum = entry.data.lreboot.rebootnum;
			uint8_t cause = entry.data.lreboot.rebootcause;
			uint8_t debug = entry.data.lreboot.debugtrace;
			uint8_t debugint = entry.data.lreboot.debuginttrace;
			DbgPrintf_P(PSTR("REBO, %lu, cause:"), (unsigned long)rebootnum);
			if (cause & 0x1) {
				rs232_sendstring_P(PSTR(" PowerOn"));
			}
			if (cause & 0x2) {
				rs232_sendstring_P(PSTR(" External"));
			}
			if (cause & 0x4) {
				rs232_sendstring_P(PSTR(" Brownout"));
			}
			if (cause & 0x8) {
				DbgPrintf_P(PSTR(" Watchdog trace: %u inttrace: %u"), debug, debugint);
			}
			if (cause & 0x10) {
				rs232_sendstring_P(PSTR(" Debug"));
			}
			if (cause & 0x20) {
				rs232_sendstring_P(PSTR(" Software"));
			}
		}
		if (entry.messagetype == LOG_MESSAGE_BATTERY) {
			uint16_t voltage = entry.data.lbattery.voltage;
			uint16_t batCharged = entry.data.lbattery.batteryCharged;
			uint16_t temperature10th = entry.data.lbattery.temperature;
			int16_t current = entry.data.lbattery.solarcurrent;
			uint32_t consumed = entry.data.lbattery.consumed;
			uint16_t templ = temperature10th / 10;
			uint16_t tempr = temperature10th % 10;
			DbgPrintf_P(PSTR("BATT, %umV, bat: %umAh, %imA, %u.%u°C, consumed: %lumAh"),
			           voltage, batCharged, current, templ, tempr, (unsigned long)consumed);
		}
	}
	rs232_sendstring_P(PSTR("\r\n"));
}

void logger_print_iter(void) {
	if (!g_state.logger.reportingmode) {
		return;
	}
	i2ceep_init();
	uint8_t timestart = rtc_8thcounter;
	do {
		if (rfm12_replicateready()) {
			uint16_t freerfm = rfm12_txbufferfree();
			if ((freerfm < LOGGER_MESSAGE_HEX_MAX) ||
			   ((freerfm < LOGGER_MESSAGE_HUMANREAD_MAX) && (g_state.logger.reportingmode == 2))) {
				if (g_settings.debugRs232 == 0xC) {
					rs232_sendstring_P(PSTR("Logwait\r\n"));
				}
				return; //only print if buffer is free. Otherwise we could end up waiting too long.
			}
		}
		logger_print_entry(g_state.logger.reportingindex);
		g_state.logger.reportingindex++;
		if (g_state.logger.reportingindex >= g_state.logger.maxentries) {
			g_state.logger.reportingindex = 0;
		}
		if (timestart != rtc_8thcounter) {
			break; //otherwise printing everything at once would take too long
		}
	} while (g_state.logger.reportingindex != g_state.logger.nextentry);
	if (g_state.logger.reportingindex == g_state.logger.nextentry) {
		g_state.logger.reportingmode = 0; //stop printing
	}
	i2ceep_disable();
}

void logger_print(uint8_t hr) {
	if (hr) {
		g_state.logger.reportingmode = 2;
	} else {
		g_state.logger.reportingmode = 1;
	}
	g_state.logger.reportingindex = g_state.logger.nextentry;
	logger_print_iter();
}
