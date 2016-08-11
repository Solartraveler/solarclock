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
		C. as 2
		D. as 2
		E. as 2
		F. as 2
	*/
	uint8_t debugRs232;
	uint8_t alarmEnabled;  //0: Disabled, 1: Enabled
	uint8_t alarmHour;     //0...23 [hours]
	uint8_t alarmMinute;   //0...59 [minutes]
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
} settings_t;

typedef struct {
	uint8_t alarmEnterState; //config screen: 0: set hour, 1: set minutes, 2: enable/disable
	uint16_t timerCountdownSecs; //[seconds]
	uint8_t subsecond; //[1/8th second]
	uint32_t time; //[seconds] since 1.1.2000
	int16_t freqdelta; //remaining error [(1/100)%] of the internal RC resonator compared to 32.768kHz crystal
	uint16_t ldr; //lower 14 bits: [AD] raw value, upper 2 bits: conversion resistor selected
	uint8_t brightnessLdr; //current one if ldr would be used (before slow adjust)
	uint8_t brightness; //current one really used
	uint8_t brightDownCd; //[0.5 seconds] wait until brightness reduce is allowed again
	uint8_t brightUpCd; //[0.5 seconds] wait until brightness increase is allowed again
	uint64_t consumption; //[µAs]
	uint8_t dotsOn;      // number of dots of the display currently enabled
	uint16_t keyDebugAd; //[AD] raw value converted value of key B
	uint16_t gradcelsius10; //[1/10°C]
	uint16_t dcf77ResyncCd; //count down in [seconds] until dcf77 resync starts, also saves settings
	uint8_t dcf77Synced; //0: never synced, 1: synced.
	uint8_t irKeyCd; //count down in [1/8s]
	uint8_t irKeyLast; //0: None, 1..4: The keys A...D
	uint8_t displayNoOffCd; //do not switch display off if not zero [seconds]
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
} sysstate_t;

extern settings_t g_settings; //permanent settings

extern sysstate_t g_state; //floating state of main program

void config_load(void);
void config_save(void);


#endif

