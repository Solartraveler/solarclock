/* Matrix-Advancedclock
  (c) 2014-2016 by Malte Marwedel
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

#define EEPROM_CONFIG_POS (void*)0
#define EEPROM_CRC_POS ((uint16_t*)(EEPROM_CONFIG_POS + sizeof(settings_t)))
#define EEPROM_CONFIG_BACKUP_POS (void*)(EEPROM_CRC_POS + 4)
#define EEPROM_CRC_BACKUP_POS ((uint16_t*)(EEPROM_CONFIG_BACKUP_POS + sizeof(settings_t)))

static uint16_t calcCrc(uint8_t * data, uint8_t size) {
	uint8_t i;
	uint16_t crc = 0;
	for (i = 0; i < size; i++) {
		crc = _crc16_update(crc, data[i]);
	}
	return crc;
}

uint8_t config_read_eep(void * addressconfigeep, uint16_t * addresscrceep) {
	eeprom_read_block(&g_settings, addressconfigeep, sizeof(settings_t));
	uint16_t crcShould = eeprom_read_word(addresscrceep);
	uint16_t crcIs = calcCrc((uint8_t*)(&g_settings), sizeof(settings_t));
	if (crcIs == crcShould) {
		return 1;
	}
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
		g_settings.soundVolume = 240;
		g_settings.soundFrequency = 1000;
		g_settings.soundAutoOffMinutes = 5;
		g_settings.displayRefresh = 100;
		g_settings.brightnessAuto = 1;
		g_settings.brightness = 60;
		g_settings.brightnessNoOff = 1;
		g_settings.clockShowSeconds = 1;
		g_settings.timerMinutes = 5;
		g_settings.debugRs232 = 0; //no debug prints
		g_settings.consumptionLedOneMax = CONSUMPTIONLEDONEMAX_NORMAL;
		g_settings.batteryCapacity = 1000;
		g_settings.currentResCal = CURRENTRESCAL_NORMAL;
		g_settings.dcf77Level = DCF77LEVEL_NORMAL;
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
			g_settings.debugRs232 = 1;
		}
		if (g_settings.chargerMode > 1) {
			g_settings.chargerMode = 0; //switches charger from ON to Auto for saftety reason
		}
		if ((g_settings.consumptionLedOneMax < CONSUMPTIONLEDONEMAX_MIN) ||
		   (g_settings.consumptionLedOneMax > CONSUMPTIONLEDONEMAX_MAX)) {
			g_settings.consumptionLedOneMax = CONSUMPTIONLEDONEMAX_NORMAL; //adjust depending on color and potentiometer settings
		}
		if (g_settings.batteryCapacity > 1200) {
			g_settings.batteryCapacity = 1000;
		}
		if (g_settings.batteryCapacity < 600) {
			g_settings.batteryCapacity = 600;
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
	}
}

void config_save(void) {
	eeprom_update_block(&g_settings, EEPROM_CONFIG_POS, sizeof(settings_t));
	uint16_t crc = calcCrc((uint8_t*)(&g_settings), sizeof(settings_t));
	eeprom_update_word(EEPROM_CRC_POS, crc);
	eeprom_update_block(&g_settings, EEPROM_CONFIG_BACKUP_POS, sizeof(settings_t));
	eeprom_update_word(EEPROM_CRC_BACKUP_POS, crc);
}
