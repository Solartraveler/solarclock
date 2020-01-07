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

#include <string.h>
#include <avr/eeprom.h>
#include <util/crc16.h>
#include <avr/pgmspace.h>

#include "config.h"
#include "rs232.h"
#include "debug.h"
#include "rfm12.h"

#define EEPROM_CONFIG_POS (void*)0
#define EEPROM_CRC_POS ((uint16_t*)(EEPROM_CONFIG_POS + sizeof(settings_t)))
#define EEPROM_CONFIG_BACKUP_POS (void*)(EEPROM_CRC_POS + 4)
#define EEPROM_CRC_BACKUP_POS ((uint16_t*)(EEPROM_CONFIG_BACKUP_POS + sizeof(settings_t)))

#define EEPROM_CONFIG_LEGACY_POS (void*)0
#define EEPROM_CRC_LEGACY_POS ((uint16_t*)(EEPROM_CONFIG_LEGACY_POS + sizeof(settingsLegacy_t)))

static uint16_t calcCrc(uint8_t * data, uint8_t size) {
	uint8_t i;
	uint16_t crc = 0;
	for (i = 0; i < size; i++) {
		crc = _crc16_update(crc, data[i]);
	}
	return crc;
}

static uint8_t config_read_eep(void * addressconfigeep, uint16_t * addresscrceep) {
	eeprom_read_block(&g_settings, addressconfigeep, sizeof(settings_t));
	uint16_t crcShould = eeprom_read_word(addresscrceep);
	uint16_t crcIs = calcCrc((uint8_t*)(&g_settings), sizeof(settings_t));
	if (crcIs == crcShould) {
		return 1;
	}
	g_settings.debugRs232 = 1; //basic debug prints, no RX
	return 0;
}

void config_load(void) {
	g_settings.debugRs232 = 1; //basic debug prints, no RX
	uint8_t success = config_read_eep(EEPROM_CONFIG_POS, EEPROM_CRC_POS);
	if (!success) {
		rs232_sendstring_P(PSTR("Reading backup settings...\r\n"));
		success = config_read_eep(EEPROM_CONFIG_BACKUP_POS, EEPROM_CRC_BACKUP_POS);
	}
	if (!success) {
		rs232_sendstring_P(PSTR("New settings\r\n"));
		memset((uint8_t*)(&g_settings), 0, sizeof(settings_t));
		for (uint8_t i = 0; i < ALARMS; i++) {
			g_settings.alarmHour[i] = 8;
			g_settings.alarmWeekdays[i] = 0x7F; //all days on
		}
		g_settings.soundVolume = 120;
		g_settings.soundFrequency = 1000;
		g_settings.soundAutoOffMinutes = 5;
		g_settings.displayRefresh = 100;
		g_settings.brightnessAuto = 1;
		g_settings.brightness = 60;
		g_settings.brightnessNoOff = 1;
		g_settings.clockShowSeconds = 1;
		g_settings.timerMinutes = 5;
		g_settings.debugRs232 = 0; //no debug prints
		//g_settings.debugRs232 = 2; //debug prints
		g_settings.consumptionLedOneMax = CONSUMPTIONLEDONEMAX_NORMAL;
		g_settings.batteryCapacity = 1000;
		g_settings.currentResCal = CURRENTRESCAL_NORMAL;
		g_settings.dcf77Level = DCF77LEVEL_NORMAL;
		g_settings.rfm12passcode = 123;
		g_settings.summertimeadjust = 1;
		g_settings.dcf77Period = 6;
		g_settings.loggerPeriod = 4;
		g_settings.flickerWorkaround = 0;
		g_settings.timeCalib = 0;
		g_settings.powersaveBatteryBelow = 75;
	} else {
		rs232_sendstring_P(PSTR("Valid settings in EEPROM\r\n"));
		//validate value ranges
		if ((g_settings.displayRefresh < 50)) {
			g_settings.displayRefresh = 50;
		}
		if ((g_settings.displayRefresh > 250)) {
			g_settings.displayRefresh = 250;
		}
		if (g_settings.soundFrequency > 2000) {
			g_settings.soundFrequency = 2000;
		}
		if (g_settings.soundFrequency < 100) {
			g_settings.soundFrequency = 100;
		}
		if (g_settings.debugRs232 > 15) {
			g_settings.debugRs232 = 2;
		}
		if (g_settings.chargerMode > 1) {
			g_settings.chargerMode = 0; //switches charger from ON to Auto for saftety reason
		}
		if ((g_settings.consumptionLedOneMax < CONSUMPTIONLEDONEMAX_MIN) ||
		   (g_settings.consumptionLedOneMax > CONSUMPTIONLEDONEMAX_MAX)) {
			g_settings.consumptionLedOneMax = CONSUMPTIONLEDONEMAX_NORMAL; //adjust depending on color and potentiometer settings
		}
		if (g_settings.batteryCapacity > BATTERYCAPACITY_MAX) {
			g_settings.batteryCapacity = 1000;
		}
		if (g_settings.batteryCapacity < BATTERYCAPACITY_MIN) {
			g_settings.batteryCapacity = BATTERYCAPACITY_MIN;
		}
		if ((g_settings.currentResCal < CURRENTRESCAL_MIN) ||
		    (g_settings.currentResCal > CURRENTRESCAL_MAX)) {
			g_settings.currentResCal = CURRENTRESCAL_NORMAL;
		}
		if ((g_settings.dcf77Level < DCF77LEVEL_MIN) ||
		    (g_settings.dcf77Level > DCF77LEVEL_MAX)) {
			g_settings.dcf77Level = DCF77LEVEL_NORMAL;
		}
		for (uint8_t i = 0; i < ALARMS; i++) {
			g_settings.alarmEnabled[i] &= 1;
		}
		g_settings.brightnessAuto &= 1;
		g_settings.brightnessNoOff &= 1;
		g_settings.clockShowSeconds &= 1;
		if (g_settings.alarmWeekdays[0] != 0x7F) {
			g_settings.alarmWeekdays[0] = 0x7F; //simple alarm, weekday cant be edited
		}
		if (g_settings.rc5mode > 3) {
			g_settings.rc5mode = 0;
		}
		if (g_settings.rfm12mode > 3) {
			g_settings.rfm12mode = 0;
		}
		if (g_settings.rfm12passcode > 999) {
			g_settings.rfm12passcode = 999;
		}
		if (g_settings.summertimeadjust > 1) {
			g_settings.summertimeadjust = 1;
		}
		if (g_settings.dcf77Period > 24) {
			g_settings.dcf77Period = 24;
		}
		if (g_settings.dcf77Period < 1) {
			g_settings.dcf77Period = 1;
		}
		if (g_settings.loggerPeriod > 24) {
			g_settings.loggerPeriod = 24;
		}
		if (g_settings.loggerPeriod < 1) {
			g_settings.loggerPeriod = 1;
		}
		if (g_settings.flickerWorkaround > 1) {
			g_settings.flickerWorkaround = 1;
		}
		if ((g_settings.timeCalib > TIMECALIB_MAX) || (g_settings.timeCalib < TIMECALIB_MIN)) {
			g_settings.timeCalib = 0;
		}
		if (g_settings.powersaveBatteryBelow > 100) {
			g_settings.powersaveBatteryBelow = 100;
		}
	}
}

void config_save(void) {
	eeprom_update_block(&g_settings, EEPROM_CONFIG_POS, sizeof(settings_t));
	uint16_t crc = calcCrc((uint8_t*)(&g_settings), sizeof(settings_t));
	eeprom_update_word(EEPROM_CRC_POS, crc);
	eeprom_update_block(&g_settings, EEPROM_CONFIG_BACKUP_POS, sizeof(settings_t));
	eeprom_update_word(EEPROM_CRC_BACKUP_POS, crc);
}

/*
The part allows sending in multiple blocks with delays between and prevents
throwing away messages because the RFM12 FIFO is full.
Set to 1 to start the process.
The return value gives the next number to use. If 0 is returned, the process is
done.
*/
uint8_t config_print(uint8_t part) {
	//wait for RFM12 to have a free buffer
	if (rfm12_replicateready()) {
		uint16_t freerfm = rfm12_txbufferfree();
		if (freerfm < RFM12_DATABUFFERSIZE) //might not work in the case of long loops to print at once, but reliable enough
		{
			return part;
		}
	}
	//print part
	if (part == 1) {
		DbgPrintf_P(PSTR("DebugRs232: %u\r\n"), g_settings.debugRs232);
		for (uint8_t i = 0; i < ALARMS; i++) {
			DbgPrintf_P(PSTR("Alarm[%u]: %u:%u Days:0x%X Enabled:%u\r\n"), i, g_settings.alarmHour[i], g_settings.alarmMinute[i], g_settings.alarmWeekdays[i], g_settings.alarmEnabled[i]);
		}
	} else if (part == 2) {
		DbgPrintf_P(PSTR("Timer[minutes]: %u\r\n"), g_settings.timerMinutes);
		DbgPrintf_P(PSTR("BrightnessAuto: %u\r\n"), g_settings.brightnessAuto);
	} else if (part == 3) {
		DbgPrintf_P(PSTR("Brightness: %u\r\n"), g_settings.brightness);
		DbgPrintf_P(PSTR("BrightnessNoOff: %u\r\n"), g_settings.brightnessNoOff);
	} else if (part == 4) {
		DbgPrintf_P(PSTR("ClockShowSeconds: %u\r\n"), g_settings.clockShowSeconds);
		DbgPrintf_P(PSTR("SoundAutoOff[minutes]: %u\r\n"), g_settings.soundAutoOffMinutes);
	} else if (part == 5) {
		DbgPrintf_P(PSTR("SoundVolume: %u\r\n"), g_settings.soundVolume);
		DbgPrintf_P(PSTR("SoundFrequency[Hz]: %u\r\n"), g_settings.soundFrequency);
	} else if (part == 6) {
		DbgPrintf_P(PSTR("DisplayRefresh[Hz]: %u\r\n"), g_settings.displayRefresh);
		DbgPrintf_P(PSTR("ChargerMode: %u\r\n"), g_settings.chargerMode);
	} else if (part == 7) {
		DbgPrintf_P(PSTR("ConsumtionLedOneMax[µA]: %u\r\n"), g_settings.consumptionLedOneMax);
		DbgPrintf_P(PSTR("BatteryCapacity[mAh]: %u\r\n"), g_settings.batteryCapacity);
	} else if (part == 8) {
		DbgPrintf_P(PSTR("CurrentResCal[10mΩ]: %u\r\n"), g_settings.currentResCal);
		DbgPrintf_P(PSTR("Dcf77Level: %u\r\n"), g_settings.dcf77Level);
	} else if (part == 9) {
		DbgPrintf_P(PSTR("Dcf77Period[hours]: %u\r\n"), g_settings.dcf77Period);
		DbgPrintf_P(PSTR("Rc5Mode: %u\r\n"), g_settings.rc5mode);
	} else if (part == 10) {
		for (uint8_t i = 0; i < RC5KEYS; i++) {
			DbgPrintf_P(PSTR("Rc5Codes[%u]: %u\r\n"), i, g_settings.rc5codes[i]);
		}
	} else if (part == 11) {
		DbgPrintf_P(PSTR("Rfm12Mode: %u\r\n"), g_settings.rfm12mode);
		DbgPrintf_P(PSTR("Rfm12Passcode: %u\r\n"), g_settings.rfm12passcode);
	} else if (part == 12) {
		DbgPrintf_P(PSTR("Reboots: %lu\r\n"), (unsigned long)g_settings.reboots);
		DbgPrintf_P(PSTR("Usage[seconds]: %lu\r\n"), (unsigned long)g_settings.usageseconds);
	}  else if (part == 13) {
		DbgPrintf_P(PSTR("PowersaveStart: %u:%u\r\n"), g_settings.powersaveHourStart, g_settings.powersaveMinuteStart);
		DbgPrintf_P(PSTR("PowersaveStop: %u:%u\r\n"), g_settings.powersaveHourStop, g_settings.powersaveMinuteStop);
	} else if (part == 14) {
		DbgPrintf_P(PSTR("PowersaveWeekdays: 0x%X\r\n"), g_settings.powersaveWeekdays);
		DbgPrintf_P(PSTR("PowersaveBatteryBelow[%%]: %u\r\n"), g_settings.powersaveBatteryBelow);
	}  else if (part == 15) {
		DbgPrintf_P(PSTR("Summertimeadjust: %u\r\n"), g_settings.summertimeadjust);
		DbgPrintf_P(PSTR("LoggerPeriod[hours]: %u\r\n"), g_settings.loggerPeriod);
	} else if (part == 16) {
		DbgPrintf_P(PSTR("FlickerWorkaround: %u\r\n"), g_settings.flickerWorkaround);
		DbgPrintf_P(PSTR("TimeCalib[ms]: %i\r\n"), g_settings.timeCalib);
	} else {
		return 0;
	}
	return (part + 1);
}


