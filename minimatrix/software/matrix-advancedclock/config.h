#ifndef CONFIG_H
#define CONFIG_H

#include <stdint.h>

#define CURRENTRESCAL_NORMAL 220
#define CURRENTRESCAL_MIN 180
#define CURRENTRESCAL_MAX 240

#define CONSUMPTIONLEDONEMAX_NORMAL 400
#define CONSUMPTIONLEDONEMAX_MIN 100
#define CONSUMPTIONLEDONEMAX_MAX 1000

#define DCF77LEVEL_NORMAL 60
#define DCF77LEVEL_MAX 150
#define DCF77LEVEL_MIN 2

#define ALARMS 2
#define RC5KEYS 4

#define RFM12_KEYQUEUESIZE 4

/*
Every data which should be stored in the EEPROM must be defined in settings_t.
An additional CRC of the structure is stored there too.
Every data which are state variabels (used for more than one iteration and
therefore are not local variables of data given as function call arguments)
and are used by more than one .c file should be stored in sysstate_t.
So by default no variables needs to be defined as external somewhere (there are
some exceptions).
*/

typedef struct {
	/* Level of debug:
		0: no rs232 out
		1: basic messages
		2: as 1 + control input
		3. as 2 + brightness adjustment print
		4. as 2 + IR key print
		5. as 2 + DCF77 print (full minute sample)
		6. as 2 + RFM12 print
		7. as 2 + charger print
		8. as 2 + adc calibration values
		9. as 2 + reasons for not stopping rc oscillator
		A. as 2 + estimated consumption
		B. as 2 + DCF77 print (timer adjust + unconverted time received)
		C. as 2 + Logger messages
		D. as 2 + RC5 messages
		E. as 2 + timing exceeds
		F. as 2
	*/
	uint8_t debugRs232;
	uint8_t alarmEnabled[ALARMS];  //0: Disabled, 1: Enabled
	uint8_t alarmHour[ALARMS];     //0...23 [hours]
	uint8_t alarmMinute[ALARMS];   //0...59 [minutes]
	uint8_t alarmWeekdays[ALARMS];  //each bit one day of the week. lsb = monday
	uint16_t timerMinutes; //[minutes]
	uint8_t brightnessAuto; //0: Manual brightness control, 1: Automatic brightness control
	uint8_t brightness;     //brighntess for manual brightness
	uint8_t brightnessNoOff; //if 1, never go to brightness 0.
	uint8_t clockShowSeconds; //[seconds]
	uint8_t soundAutoOffMinutes; //[minutes]
	uint8_t soundVolume;     //0: min, 255: max
	uint16_t soundFrequency; //[Hz]
	uint16_t displayRefresh; //[Hz]
	uint8_t chargerMode; //0: auto, 1: off, 2: on (on gets converted to auto on startup)
	uint16_t consumptionLedOneMax; //consuption in [µA] of one dot lighting 1/5 (5 lines) of the time
	uint16_t batteryCapacity; //[mAh]
	uint16_t currentResCal; //[10*mOhm] Calibrated value of the resistor (R46) for measuring the input current in 1/100 th of Ohm
	uint16_t dcf77Level; //number of errors one sampled minute (38bit) might have
	uint8_t rc5mode;     //0: off, 1: on for 5min if user entered something or beeper is enabled. 2: always on
	uint16_t rc5codes[RC5KEYS]; //infrared code for key left, 0 = none, [0] = left, [1] = down, [2] = up, [3] = right
	uint8_t rfm12mode;   //0: off, 1: on for 1min if user entered something or beeper is enabled. 2: 5min if user entered something or beeper is enabled, 3: always on
	uint32_t reboots;    //number of power ups
	uint32_t usageseconds; //number of seconds this device has run [seconds]
	uint8_t powersaveHourStart; //standby starts when the time is equal to this value and the weekday fits [hours]
	uint8_t powersaveMinuteStart; //standby starts when the time is equal to this value and the weekday fits [minutes]
	uint8_t powersaveHourStop; //standby stops when the time is equal to this value [hours]
	uint8_t powersaveMinuteStop;//standby stops when the time is equal to this value [minutes]
	uint8_t powersaveWeekdays;  //each bit one day of the week. lsb = monday
	//new settings start here
	uint16_t rfm12passcode; //initial number which must be entered in order to accept commands. 0...999
	uint8_t summertimeadjust; //0 = no summer time adjust, 1 = summer time adjust
	uint8_t loggerPeriod; //[hour] until resync of dcf after last sync stopped
	uint8_t dcf77Period; //[hour] how often a log is written
	uint8_t flickerWorkaround; //if 1, workaround for display flicker is enabled (at the cost of more power consumption)
	uint8_t reserved[29]; //decrease if new settings get in, so no crc mismatch on next start
} settings_t;

//upgrade path from old settings to new stettings:
typedef struct {
	uint8_t debugRs232;
	uint8_t alarmEnabled[ALARMS];  //0: Disabled, 1: Enabled
	uint8_t alarmHour[ALARMS];     //0...23 [hours]
	uint8_t alarmMinute[ALARMS];   //0...59 [minutes]
	uint8_t alarmWeekdays[ALARMS];  //each bit one day of the week. lsb = monday
	uint16_t timerMinutes; //[minutes]
	uint8_t brightnessAuto; //0: Manual brightness control, 1: Automatic brightness control
	uint8_t brightness;     //brighntess for manual brightness
	uint8_t brightnessNoOff; //if 1, never go to brightness 0.
	uint8_t clockShowSeconds; //[seconds]
	uint8_t soundAutoOffMinutes; //[minutes]
	uint8_t soundVolume;     //0: min, 255: max
	uint16_t soundFrequency; //[Hz]
	uint16_t displayRefresh; //[Hz]
	uint8_t chargerMode; //0: auto, 1: off, 2: on (on gets converted to auto on startup)
	uint16_t consumptionLedOneMax; //consuption in [µA] of one dot lighting 1/5 (5 lines) of the time
	uint16_t batteryCapacity; //[mAh]
	uint16_t currentResCal; //[10*mOhm] Calibrated value of the resistor (R46) for measuring the input current in 1/100 th of Ohm
	uint16_t dcf77Level; //number of errors one sampled minute (38bit) might have
	uint8_t rc5mode;     //0: off, 1: on for 5min if user entered something or beeper is enabled. 2: always on
	uint16_t rc5codes[RC5KEYS]; //infrared code for key left, 0 = none, [0] = left, [1] = down, [2] = up, [3] = right
	uint8_t rfm12mode;   //0: off, 1: on for 1min if user entered something or beeper is enabled. 2: 5min if user entered something or beeper is enabled, 3: always on
	uint32_t reboots;    //number of power ups
	uint32_t usageseconds; //number of seconds this device has run [seconds]
	uint8_t powersaveHourStart; //standby starts when the time is equal to this value and the weekday fits [hours]
	uint8_t powersaveMinuteStart; //standby starts when the time is equal to this value and the weekday fits [minutes]
	uint8_t powersaveHourStop; //standby stops when the time is equal to this value [hours]
	uint8_t powersaveMinuteStop;//standby stops when the time is equal to this value [minutes]
	uint8_t powersaveWeekdays;  //each bit one day of the week. lsb = monday
} settingsLegacy_t;


typedef struct {
	uint8_t ksize;
	uint16_t maxentries;
	uint16_t nextentry;
	uint32_t nextid;
	uint8_t reportingmode; //0 = none, 1 = hex, 2 = human readable
	uint16_t reportingindex; //we cant report everything in one cycle
} loggerstate_t;

typedef struct {
	uint8_t alarmEnterState; //config screen: 0: set hour, 1: set minutes, 2: enable/disable, 3...9: weekday
	uint16_t timerCountdownSecs; //[seconds]
	uint8_t subsecond; //[1/8th second]
	uint32_t time; //[seconds] since 1.1.2000
	uint8_t timehcache; // [hours] value equivalent to ((time /(60*60) % 24), reduces cpu load by avoiding slow divisions
	uint8_t timemcache; // [minutes] value equivalent to ((time / 60 % 60), reduces cpu load by avoiding slow divisions
	uint8_t timescache; // [seconds] value equivalent to (time % 60), reduces cpu load by avoiding slow divisions
	uint8_t summertime; //1 = the current time is the summer time.
	int16_t freqdelta; //remaining error [(1/100)%] of the internal RC resonator compared to 32.768kHz crystal
	uint16_t ldr; //lower 14 bits: [AD] raw value, upper 2 bits: conversion resistor selected
	uint8_t brightnessLdr; //current one if ldr would be used (before slow adjust)
	uint8_t brightness; //current one really used
	uint8_t brightDownCd; //[0.125 seconds] wait until brightness reduce is allowed again
	uint8_t brightUpCd; //[0.125 seconds] wait until brightness increase is allowed again
	uint64_t consumption; //[µAs]
	uint8_t dotsOn;      // number of dots of the display currently enabled
	uint16_t keyDebugAd; //[AD] raw value converted value of key B
	uint16_t gradcelsius10; //[1/10°C]
	uint32_t dcf77ResyncCd; //count down in [seconds] until dcf77 resync starts, also saves settings
	uint16_t dcf77ResyncTrytimeCd; //count down in [8*seconds] until resync is aborted
	uint8_t dcf77Synced; //0: never synced, 1: synced.
	uint8_t irKeyCd; //count down in [1/8s]
	uint8_t irKeyLast; //0: None, 1..4: The keys A...D
	uint8_t displayNoOffCd; //do not switch display off if not zero [seconds] Set to 60s on keypress
	uint8_t soundEnabledCd; //count down [seconds] until sound goes off automatically
	uint16_t batVoltage; //[mV] of the battery
	uint8_t batLowWarningCd; //count down [seconds] while no battery low warning is displayed
	int32_t chargerResistoroffset; //error correction factor to the base 4096 (best would be 4096). ADC0*Offset/4096 = ADC7 if no current is charged
	int16_t chargerCurrent; //[mA] of the charger
	int32_t chargerCharged60; //[mAs] adds the value of chargerCurrent every second, clear after 60 additions
	uint64_t consumptionBefore60; //[uAs] consumption value as it was at previous chargerCharged60iter = 60
	uint8_t chargerCharged60iter; //number of chargerCharged60 additions
	uint8_t chargerState; //0: off, 1: on
	uint32_t batteryCharged; //[mAs] adds part (depending on charging efficiency) of the value chargerCharged60 every 60 seconds, remove consumption every 60 seconds. Set to zero if voltage is critically low
	uint16_t chargerCd; //count down [seconds], charging forbidden
	uint16_t chargerIdle; //count up [seconds] without charging current (night)
	uint16_t performanceOff; //number of ticks, the cpu is idle but the oscillator is running. one tick = 64 clock cycles
	uint16_t performanceUp; //TC1 value written by the clock interrupt waking the system
	uint16_t performanceRcRunning; //[1/2.56 %]
	uint16_t performanceCpuRunning; //[1/2.56 %]
	uint8_t powerdownEnabled;
	uint8_t rc5entermode; // 0 = none (normal operation), 1 = left, 2 = up, 3 = down, 4 = right
	uint8_t rc5modeis; //0 = currently off, 1 = receiver currently on
	uint16_t rc5Cd; //[seconds] count down until device should be powered off
	uint8_t rfm12modeis; //0 = currently off, 1 = receiver currently on
	uint16_t rfm12Cd; //[seconds] count down until device should be powered off
	uint8_t powersaveenterstate; //state for the three edit fields. 0...1 for hour/minute edit 0...6 for weekday edit field
/* powersaveEnabled
	0 = disabled
	bit0 = enabled by timer
	bit1 = enabled by user
  bit2 = enabled by critical low voltage
  If enabled (at least one bit set), only enable display after an keypress for a short time.
*/
	uint8_t powersaveEnabled;
	uint8_t pintesterrors; //1 = pintest detected errors, 0 = no errors or not run
	uint8_t rfm12keyqueue[RFM12_KEYQUEUESIZE]; //keys from the rfm12 module
	loggerstate_t logger;
} sysstate_t;

extern settings_t g_settings; //permanent settings

extern sysstate_t g_state; //floating state of main program

void config_load(void);
void config_save(void);


#endif

