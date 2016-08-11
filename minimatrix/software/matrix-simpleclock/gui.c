/* Matrix-Simpleclock
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

/* The GUI logic. No AVR specific functions (except loading from flash)
within this file, allows compiling and testing the whole GUI on a PC.
*/

#include <string.h>
#include <stdio.h>
#include <stdint.h>
#include <ctype.h>

#include "config.h"
#include "timeconvert.h"

/*Enable for debug builds.
  Can result in crashes or dark display with no way to get back without
  clearing the EEPRom with a programmer
*/
//#define UNSAFE_OPS

#ifndef PC_BUILD
//AVR only
#include <avr/pgmspace.h>
#include "main.h"
#include "dcf77.h"
#include "displayRtc.h"
#include "rs232.h"

#else
//PC compatibility layer
#define PROGMEM
#define PSTR(X) X
#define sprintf_P sprintf
#define reboot() printf("reboot()\r\n")
#define dcf77_disable() printf("DCF77 disabled\r\n")
#define dcf77_enable(X) printf("DCF77 enabled\r\n")
#define disp_configure_set(X, Y) printf("disp_configure_set(%i, %i);\r\n", X, Y)
#define rs232_rx_disable() printf("rs232_rx_disable()\r\n")
#define rs232_rx_init() printf("rs232_rx_init()\r\n")

void monthdayfromdayinyear(uint16_t dayinyear, uint16_t year, uint8_t * month, uint8_t * day);
uint8_t yearsince2000(uint32_t seconds, uint16_t * dofy);
void dcf77_getstatus(char* text);
unsigned char pgm_read_byte(const unsigned char * addr) {
	return *addr;
}

#endif

#include "gui.h"
#include "config.h"

#include "menu-interpreter.h"
#include "menudata-progmem.c"

#define CHARS_PER_MENU_TEXT 14

unsigned char g_StringMenus[MENU_TEXT_MAX*CHARS_PER_MENU_TEXT];

dispUpdateFunc * g_dispUpdate; //NULL is a legitimate value



unsigned char menu_byte_get(MENUADDR addr) {
	if (addr >= MENU_DATASIZE) {
		return 0;
	}
	return pgm_read_byte(menudata+addr);
}

void updateVoltageText(void) {
	sprintf_P((char*)menu_strings[MENU_TEXT_InputVoltage], PSTR("%umV"), g_state.batVoltage);
}

void updateCurrentText(void) {
	sprintf_P((char*)menu_strings[MENU_TEXT_ChargerCurrent], PSTR("%imA"), g_state.chargerCurrent);
}

void updateDcf77Text(void) {
	dcf77_getstatus((char*)menu_strings[MENU_TEXT_DcfValue]);
}

void updateClockText(void) {
	uint32_t temp = g_state.time;
	uint8_t s = temp % 60;
	uint8_t m = (temp / 60) % 60;
	uint8_t h = (temp / (60*60)) % 24;
	if (g_settings.clockShowSeconds) {
		sprintf_P((char*)menu_strings[MENU_TEXT_Clock], PSTR("%2i:%02i:%02i"), h, m, s);
		if (h < 10) {
			menu_strings[MENU_TEXT_Clock][0] = '\t';
		}
	} else {
			sprintf_P((char*)menu_strings[MENU_TEXT_Clock], PSTR("     %2i:%02i"), h, m);
		if (h < 10) {
			menu_strings[MENU_TEXT_Clock][5] = '\t';
		}
	}
}

void updateKeylockText(void) {
	uint32_t temp = g_state.time;
	uint8_t m = (temp / 60) % 60;
	uint8_t h = (temp / (60*60)) % 24;
	sprintf_P((char*)menu_strings[MENU_TEXT_Keylock], PSTR("K  %2i:%02i"), h, m);
	if (h < 10) {
		menu_strings[MENU_TEXT_Clock][4] = '\t';
	}
}

void updateDateText(void) {
	uint32_t temp = g_state.time;
	uint16_t dofy;
	uint16_t y = yearsince2000(temp, &dofy);
	uint8_t month;
	uint8_t d;
	monthdayfromdayinyear(dofy, y, &month, &d);
	month++; //starts with 0
	d++; //starts with 0
	sprintf_P((char*)menu_strings[MENU_TEXT_Date], PSTR("%02i.%02i.%2i"), (uint16_t)d, (uint16_t)month, y);
}

void updateTemperatureText(void) {
	uint16_t gradcelsius10th = g_state.gradcelsius10 % 10;
	uint8_t gradcelsius = g_state.gradcelsius10 / 10;
	sprintf_P((char*)menu_strings[MENU_TEXT_Temperature], PSTR( "%i.%u°C"), gradcelsius, gradcelsius10th);
}

void updateKeysText(void) {
	sprintf_P((char*)menu_strings[MENU_TEXT_KeyAdValue], PSTR("B:%i"), g_state.keyDebugAd);
}

void updateConsumptionText(void) {
	uint32_t mah = (g_state.consumption/((uint64_t)60*(uint64_t)60*(uint64_t)1000));
	sprintf_P((char*)menu_strings[MENU_TEXT_ConsumptionTotal], PSTR("%lumAh"), (unsigned long)mah);
}

void updateAlarmText(void) {
	sprintf_P((char*)menu_strings[MENU_TEXT_Alarm], PSTR("%2i:%02i %s"), g_settings.alarmHour, g_settings.alarmMinute, g_settings.alarmEnabled ? "On": "--");
	if (g_settings.alarmHour < 10) {
		menu_strings[MENU_TEXT_Alarm][0] = '\t';
	}
	if ((g_state.subsecond & 0x6) == 0) {
		if (g_state.alarmEnterState == 0) {
			menu_strings[MENU_TEXT_Alarm][0] = '\t';
			menu_strings[MENU_TEXT_Alarm][1] = '\t';
		} else if (g_state.alarmEnterState == 1) {
			menu_strings[MENU_TEXT_Alarm][3] = '\t';
			menu_strings[MENU_TEXT_Alarm][4] = '\t';
		} else {
			menu_strings[MENU_TEXT_Alarm][6] = '\t';
			menu_strings[MENU_TEXT_Alarm][7] = '\t';
		}
	}
}

void updateTimerText(void) {
	if (g_state.timerCountdownSecs) {
		uint8_t h = g_state.timerCountdownSecs / (60*60);
		uint8_t m = g_state.timerCountdownSecs / 60 % 60;
		uint8_t s = g_state.timerCountdownSecs % 60;
		if (h < 10) {
			sprintf_P((char*)menu_strings[MENU_TEXT_Timer], PSTR("   %i:%02i:%02i"), h, m, s);
		} else {
			sprintf_P((char*)menu_strings[MENU_TEXT_Timer], PSTR("%i:%02i:%02i"), h, m, s);
		}
	} else {
		sprintf_P((char*)menu_strings[MENU_TEXT_Timer], PSTR("%imin"), g_settings.timerMinutes);
	}
}

//direct A/D value from LDR
void updateLightText(void) {
	uint8_t workmode = g_state.ldr >> 14;
	sprintf_P((char*)menu_strings[MENU_TEXT_Light], PSTR("%i:%i"), workmode, g_state.ldr & 0x3FFF);
}

void updateChargedText(void) {
	uint16_t mah = 0;
	mah = g_state.batteryCharged / (60*60);
	sprintf_P((char*)menu_strings[MENU_TEXT_ChargerCharged], PSTR("%umAh"), mah);
}

//value for driving the display
void updateDispbrightText(void) {
	if (g_settings.brightnessAuto) {
		sprintf_P((char*)menu_strings[MENU_TEXT_Brightness], PSTR("%i<%i"), g_state.brightnessLdr, g_settings.brightness);
	} else {
		sprintf_P((char*)menu_strings[MENU_TEXT_Brightness], PSTR("%i>%i"), g_state.brightnessLdr, g_settings.brightness);
	}
}

void updatePerformanceText(void) {
	unsigned int rcperc = g_state.performanceRcRunning * 100 / 256;
	unsigned int cpuperc = g_state.performanceCpuRunning * 100 / 256;
	sprintf_P((char*)menu_strings[MENU_TEXT_Performance], PSTR("%u%%%u%%"), rcperc, cpuperc);
}


/*This function updates all text, which are only changed as response to user
 input. (Not as response to A/D changes or timings*/
static void updateText(void) {
	if (g_settings.debugRs232) {
		sprintf_P((char*)menu_strings[MENU_TEXT_Rs232Debug], PSTR("Dbg On %X"), g_settings.debugRs232);
	} else {
		sprintf_P((char*)menu_strings[MENU_TEXT_Rs232Debug], PSTR("Dbg Off"));
	}
	sprintf_P((char*)menu_strings[MENU_TEXT_Refresh], PSTR("%iHz"), g_settings.displayRefresh);
	sprintf_P((char*)menu_strings[MENU_TEXT_SoundFreq], PSTR("%iHz"), g_settings.soundFrequency);
	sprintf_P((char*)menu_strings[MENU_TEXT_SoundVolume], PSTR("Vol %i"), g_settings.soundVolume);
	sprintf_P((char*)menu_strings[MENU_TEXT_SoundAutoOffTime], PSTR("%imin"), g_settings.soundAutoOffMinutes);
	sprintf_P((char*)menu_strings[MENU_TEXT_BatCap], PSTR("%imAh"), g_settings.batteryCapacity);
	sprintf_P((char*)menu_strings[MENU_TEXT_LedCurrent], PSTR("%iµA"), g_settings.consumptionLedOneMax);
	sprintf_P((char*)menu_strings[MENU_TEXT_CalibRes], PSTR("%i0mΩ"), g_settings.currentResCal);
	sprintf_P((char*)menu_strings[MENU_TEXT_Dcf77Level], PSTR("E<=%i"), g_settings.dcf77Level);
	if (g_settings.clockShowSeconds) {
		sprintf_P((char*)menu_strings[MENU_TEXT_ShowSecs], PSTR("Sec On"));
	} else {
		sprintf_P((char*)menu_strings[MENU_TEXT_ShowSecs], PSTR("Sec Off"));
	}
	if (g_settings.brightnessNoOff) {
		sprintf_P((char*)menu_strings[MENU_TEXT_BrigthAlways], PSTR("Min Br 1"));
	} else {
		sprintf_P((char*)menu_strings[MENU_TEXT_BrigthAlways], PSTR("Min Br 0"));
	}
	if (g_settings.chargerMode == 0) {
		sprintf_P((char*)menu_strings[MENU_TEXT_ChargerMode], PSTR("Auto"));
	} else if (g_settings.chargerMode == 1) {
		sprintf_P((char*)menu_strings[MENU_TEXT_ChargerMode], PSTR("Off"));
	} else {
		sprintf_P((char*)menu_strings[MENU_TEXT_ChargerMode], PSTR("On"));
	}
}

static uint8_t dcf77On(void) {
	dcf77_enable(g_state.freqdelta);
	return 1;
}

static uint8_t dcf77Off(void) {
	dcf77_disable();
	return 1;
}

static uint8_t alarmDec(void) {
	if (g_state.alarmEnterState == 0) {
		g_settings.alarmHour--;
		if (g_settings.alarmHour >= 24) {
			g_settings.alarmHour = 23;
		}
	}
	if (g_state.alarmEnterState == 1) {
		g_settings.alarmMinute--;
		if (g_settings.alarmMinute >= 60) {
			g_settings.alarmMinute = 59;
		}
	}
	if (g_state.alarmEnterState == 2) {
		g_settings.alarmEnabled = 0;
	}
	return 0;
}

static uint8_t alarmInc(void) {
	if (g_state.alarmEnterState == 0) {
		g_settings.alarmHour++;
		if (g_settings.alarmHour >= 24) {
			g_settings.alarmHour = 0;
		}
	}
	if (g_state.alarmEnterState == 1) {
		g_settings.alarmMinute++;
		if (g_settings.alarmMinute >= 60) {
			g_settings.alarmMinute = 0;
		}
	}
	if (g_state.alarmEnterState == 2) {
		g_settings.alarmEnabled = 1;
	}
	return 0;
}

static uint8_t alarmNext(void) {
	g_state.alarmEnterState++;
	if (g_state.alarmEnterState >= 3) {
		g_state.alarmEnterState = 0;
	}
	return 0;
}

static uint8_t timerDec(void) {
	if (g_state.timerCountdownSecs == 0) {
		if (g_settings.timerMinutes >= 25) {
			g_settings.timerMinutes -= 4;
		}
		if (g_settings.timerMinutes >= 2) {
			g_settings.timerMinutes--;
		}
	}
	return 0;
}

static uint8_t timerInc(void) {
	if (g_state.timerCountdownSecs == 0) {
		if (g_settings.timerMinutes < 180) {
			if (g_settings.timerMinutes >= 20) {
				g_settings.timerMinutes += 4;
			}
			g_settings.timerMinutes++;
		}
	}
	return 0;
}

static uint8_t timerStart(void) {
	if (g_state.timerCountdownSecs) {
		g_state.timerCountdownSecs = 0; //abort running timer
	} else {
		g_state.timerCountdownSecs = g_settings.timerMinutes*60; //start timer
	}
	return 0;
}

static uint8_t brightDec(void) {
#ifdef 	UNSAFE_OPS
	if (g_settings.brightness > 0) //can result in a permanently dark screen and blind navigation
#else
	if (g_settings.brightness > 1)
#endif
	{
		if (g_settings.brightness >= 80) {
			g_settings.brightness -= 20;
		} else if (g_settings.brightness >= 15) {
			g_settings.brightness -= 5;
		} else {
			g_settings.brightness--;
		}
	}
	return 0;
}

static uint8_t brightInc(void) {
	if ((g_settings.brightness >= 60) && (g_settings.brightness < 235)) {
		g_settings.brightness += 20;
	} else if ((g_settings.brightness >= 10) && (g_settings.brightness < 250)) {
		g_settings.brightness += 5;
	} else {
		if (g_settings.brightness < 255) {
			g_settings.brightness++;
		}
	}
	return 0;
}

static uint8_t brightAutoToggle(void) {
	g_settings.brightnessAuto = 1 - g_settings.brightnessAuto;
	return 0;
}

static uint8_t brightAlwaysOn(void) {
	g_settings.brightnessNoOff = 1;
	updateText();
	return 1;
}

static uint8_t brightAlwaysOff(void) {
	g_settings.brightnessNoOff = 0;
	updateText();
	return 1;
}

static uint8_t clockShowSecOff(void) {
	g_settings.clockShowSeconds = 0;
	updateText();
	return 1;
}

static uint8_t clockShowSecOn(void) {
	g_settings.clockShowSeconds = 1;
	updateText();
	return 1;
}

static uint8_t soundAutoOffDec(void) {
	if (g_settings.soundAutoOffMinutes >= 2) {
		g_settings.soundAutoOffMinutes--;
	}
	updateText();
	return 1;
}

static uint8_t soundAutoOffInc(void) {
	if (g_settings.soundAutoOffMinutes < 30) {
		g_settings.soundAutoOffMinutes++;
	}
	updateText();
	return 1;
}

static uint8_t soundVolumeDec(void) {
	if (g_settings.soundVolume >= 10) {
		g_settings.soundVolume -= 10;
	}
	updateText();
	return 1;
}

static uint8_t soundVolumeInc(void) {
	if (g_settings.soundVolume <= 245) {
		g_settings.soundVolume += 10;
	}
	updateText();
	return 1;
}

static uint8_t soundFreqDec(void) {
	if (g_settings.soundFrequency >= 200) {
		g_settings.soundFrequency -= 100;
	}
	updateText();
	return 1;
}

static uint8_t soundFreqInc(void) {
	if (g_settings.soundFrequency <= 1900) {
		g_settings.soundFrequency += 100;
	}
	updateText();
	return 1;
}

static uint8_t refreshDec(void) {
	if (g_settings.displayRefresh >= 75) {
		g_settings.displayRefresh -= 25;
	}
	updateText();
	disp_configure_set(g_state.brightness, g_settings.displayRefresh);
	return 1;
}

static uint8_t refreshInc(void) {
#ifdef UNSAFE_OPS
	if (g_settings.displayRefresh <= 3000)
#else
	if (g_settings.displayRefresh <= 225)
#endif
	{
		g_settings.displayRefresh += 25;
	}
	updateText();
	disp_configure_set(g_state.brightness, g_settings.displayRefresh);
	return 1;
}

static uint8_t chargerDec(void) {
	if (g_settings.chargerMode) {
		g_settings.chargerMode--;
	}
	updateText();
	return 1;
}

static uint8_t chargerInc(void) {
	if (g_settings.chargerMode <= 1) {
		g_settings.chargerMode++;
	}
	updateText();
	return 1;
}

static uint8_t rs232Dec(void) {
	if (g_settings.debugRs232) {
		g_settings.debugRs232--;
	}
	if (g_settings.debugRs232 < 2) {
		rs232_rx_disable();
	}
	updateText();
	return 1;
}

static uint8_t rs232Inc(void) {
	if (g_settings.debugRs232 < 15) {
		g_settings.debugRs232++;
	}
	if (g_settings.debugRs232 == 2) {
		rs232_rx_init();
	}
	updateText();
	return 1;
}

static uint8_t batCapDec(void) {
	if (g_settings.batteryCapacity > 650) {
		g_settings.batteryCapacity -= 50;
	}
	updateText();
	return 1;
}

static uint8_t batCapInc(void) {
	if (g_settings.batteryCapacity < 1200) {
		g_settings.batteryCapacity += 50;
	}
	updateText();
	return 1;
}

static uint8_t ledCurrDec(void) {
	if (g_settings.consumptionLedOneMax > CONSUMPTIONLEDONEMAX_MIN) {
		g_settings.consumptionLedOneMax -= 10;
	}
	updateText();
	return 1;
}

static uint8_t ledCurrInc(void) {
	if (g_settings.consumptionLedOneMax < CONSUMPTIONLEDONEMAX_MAX) {
		g_settings.consumptionLedOneMax += 10;
	}
	updateText();
	return 1;
}

static uint8_t calRes46Dec(void) {
	if (g_settings.currentResCal > CURRENTRESCAL_MIN) {
		g_settings.currentResCal--;
	}
	updateText();
	return 1;
}

static uint8_t calRes46Inc(void) {
	if (g_settings.currentResCal < CURRENTRESCAL_MAX) {
		g_settings.currentResCal++;
	}
	updateText();
	return 1;
}

static uint8_t dcf77LevelDec(void) {
	if (g_settings.dcf77Level > DCF77LEVEL_MIN) {
		g_settings.dcf77Level--;
	}
	updateText();
	return 1;
}

static uint8_t dcf77LevelInc(void) {
	if (g_settings.dcf77Level < DCF77LEVEL_MAX) {
		g_settings.dcf77Level++;
	}
	updateText();
	return 1;
}

unsigned char menu_action(unsigned short action) {
	unsigned char redraw = 0;
	switch(action) {
		case MENU_ACTION_ShowVoltageOn:      g_dispUpdate = &updateVoltageText; break;
		case MENU_ACTION_ShowCurrentOn:      g_dispUpdate = &updateCurrentText; break;
		case MENU_ACTION_ShowLightOn:        g_dispUpdate = &updateLightText; break;
		case MENU_ACTION_ShowDCF77On:        g_dispUpdate = &updateDcf77Text; break;
		case MENU_ACTION_ShowClockOn:        g_dispUpdate = &updateClockText; break;
		case MENU_ACTION_ShowDateOn:         g_dispUpdate = &updateDateText; break;
		case MENU_ACTION_ShowTemperatureOn:  g_dispUpdate = &updateTemperatureText; break;
		case MENU_ACTION_ShowKeysOn:         g_dispUpdate = &updateKeysText; break;
		case MENU_ACTION_ShowConsumptionOn:  g_dispUpdate = &updateConsumptionText; break;
		case MENU_ACTION_ShowDispbrightOn:   g_dispUpdate = &updateDispbrightText; break;
		case MENU_ACTION_ShowTimerOn:        g_dispUpdate = &updateTimerText; break;
		case MENU_ACTION_ShowAlarmOn:        g_dispUpdate = &updateAlarmText; break;
		case MENU_ACTION_ShowChargedOn:      g_dispUpdate = &updateChargedText; break;
		case MENU_ACTION_ShowKeylockOn:      g_dispUpdate = &updateKeylockText; break;
		case MENU_ACTION_ShowPerformanceOn:  g_dispUpdate = &updatePerformanceText; break;
		case MENU_ACTION_DisplayUpdateOff:   g_dispUpdate = NULL; break;
		case MENU_ACTION_Dcf77Off:           redraw = dcf77Off(); break;
		case MENU_ACTION_Dcf77On:            redraw = dcf77On(); break;
		case MENU_ACTION_AlarmDec:           redraw = alarmDec(); break;
		case MENU_ACTION_AlarmInc:           redraw = alarmInc(); break;
		case MENU_ACTION_AlarmNext:          redraw = alarmNext(); break;
		case MENU_ACTION_TimerDec:           redraw = timerDec(); break;
		case MENU_ACTION_TimerInc:           redraw = timerInc(); break;
		case MENU_ACTION_TimerStart:         redraw = timerStart(); break;
		case MENU_ACTION_BrightDec:          redraw = brightDec(); break;
		case MENU_ACTION_BrightInc:          redraw = brightInc(); break;
		case MENU_ACTION_BrightAutoToggle:   redraw = brightAutoToggle(); break;
		case MENU_ACTION_BrightAlwaysOff:    redraw = brightAlwaysOff(); break;
		case MENU_ACTION_BrightAlwaysOn:     redraw = brightAlwaysOn(); break;
		case MENU_ACTION_ClockShowSecOff:    redraw = clockShowSecOff(); break;
		case MENU_ACTION_ClockShowSecOn:     redraw = clockShowSecOn(); break;
		case MENU_ACTION_SoundAutoOffDec:    redraw = soundAutoOffDec(); break;
		case MENU_ACTION_SoundAutoOffInc:    redraw = soundAutoOffInc(); break;
		case MENU_ACTION_SoundVolumeDec:     redraw = soundVolumeDec(); break;
		case MENU_ACTION_SoundVolumeInc:     redraw = soundVolumeInc(); break;
		case MENU_ACTION_SoundFreqDec:       redraw = soundFreqDec(); break;
		case MENU_ACTION_SoundFreqInc:       redraw = soundFreqInc(); break;
		case MENU_ACTION_RefreshDec:         redraw = refreshDec(); break;
		case MENU_ACTION_RefreshInc:         redraw = refreshInc(); break;
		case MENU_ACTION_Reboot:             reboot(); break;
		case MENU_ACTION_ChargerDec:         redraw = chargerDec(); break;
		case MENU_ACTION_ChargerInc:         redraw = chargerInc(); break;
		case MENU_ACTION_Rs232Dec:           redraw = rs232Dec(); break;
		case MENU_ACTION_Rs232Inc:           redraw = rs232Inc(); break;
		case MENU_ACTION_BatCapDec:          redraw = batCapDec(); break;
		case MENU_ACTION_BatCapInc:          redraw = batCapInc(); break;
		case MENU_ACTION_LedCurrDec:         redraw = ledCurrDec(); break;
		case MENU_ACTION_LedCurrInc:         redraw = ledCurrInc(); break;
		case MENU_ACTION_CalRes46Dec:        redraw = calRes46Dec(); break;
		case MENU_ACTION_CalRes46Inc:        redraw = calRes46Inc(); break;
		case MENU_ACTION_Dcf77LevelDec:      redraw = dcf77LevelDec(); break;
		case MENU_ACTION_Dcf77LevelInc:      redraw = dcf77LevelInc(); break;
	}
	if (g_dispUpdate) {
		g_dispUpdate();
		redraw = 1;
	}
	return redraw;
}

//the global settings should be loaded before calling this function
void gui_init(void) {
	uint8_t i;
	for (i = 0; i < MENU_TEXT_MAX; i++) {
		menu_strings[i] = g_StringMenus + i*CHARS_PER_MENU_TEXT;
	}
	updateText();
	g_dispUpdate = &updateDcf77Text; //This is for the first screen shown on startup
	g_dispUpdate();
	menu_redraw();
}

