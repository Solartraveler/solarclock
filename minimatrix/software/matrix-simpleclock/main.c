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

#include <util/delay_basic.h>
#include <avr/pgmspace.h>
#include <avr/interrupt.h>
#include <avr/sleep.h>
#include <avr/wdt.h>
#include <avr/io.h>
#include <avr/eeprom.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>

#include "main.h"
//needs F_CPU from main.h:
#include <util/delay.h>
#include "menu-interpreter.h"

#include "displayRtc.h"
#include "rs232.h"
#include "debug.h"
#include "sound.h"
#include "dcf77.h"
#include "clocks.h"
#include "rfm12.h"
#include "adc.h"
#include "gui.h"
#include "config.h"
#include "charger.h"

settings_t g_settings; //permanent settings
sysstate_t g_state; //floating state of main program

void waitms(uint16_t ms) {
	while (ms) {
		_delay_loop_2(F_CPU/4000); //4 cycle per loop 4MHz -> 1ms: 1000 loops
		ms--;
	}
}

void power_setup(void) {
	PR_PRGEN = 0x1F;
	PR_PRPA = 0x7;
	PR_PRPB = 0x7;
	PR_PRPC = 0x7F;
	PR_PRPD = 0x7F;
	PR_PRPE = 0x7F;
	PR_PRPF = 0x7F;
	/*flash power reduciton hw bug:
	 can not be used together with power save or extended standby */
	//NVM_CTRLB |= NVM_FPRM_bm | NVM_EPRM_bm;
	NVM_CTRLB |= NVM_EPRM_bm;
	CCP = 0xD8;
	MCU_MCUCR = 1; //disable jtag, saves power
}

void pull_port(PORT_t * port, uint8_t bitmask) {
	uint8_t i;
	for (i = 0; i < 8; i++) {
		register8_t * pinctrl = (&(port->PIN0CTRL)) + i;
		if (bitmask & 1) {
			*pinctrl = PORT_OPC_PULLUP_gc;
		} else {
			*pinctrl = PORT_OPC_PULLDOWN_gc;
		}
		bitmask >>= 1;
	}
}

static void pull_all_pins(void) {
	pull_port(&PORTA, 0x4);
	pull_port(&PORTB, 0x0);
	pull_port(&PORTC, 0x1);
	pull_port(&PORTD, 0xFF);
	pull_port(&PORTE, 0x4F);
	pull_port(&PORTF, 0xFF);
	pull_port(&PORTH, 0xFF);
	pull_port(&PORTJ, 0xFF);
	pull_port(&PORTK, 0xFF);
	pull_port(&PORTQ, 0x03);
	pull_port(&PORTR, 0x03);
}

void flash_status_led(uint8_t status) {
	PORTK.OUTCLR = (1<<4); //enable one matrix line
	PORTF.OUT = ~status; //0 = on, 1 = 0ff
	_delay_loop_2(F_CPU/200);
	PORTF.OUT = 0xFF;
	PORTK.OUTSET = (1<<4); //disable one matrix line
}

void reboot(void) {
	config_save();
	disp_configure_set(0, g_settings.displayRefresh);
	while (eeprom_is_ready() == 0); //wait for config to finish
	cli();
	CCP = 0xD8; //change protection register
	RST.CTRL = 1; //software reset
	while(1); //if we fail before, the watchdog will do it
}

/*Estimates the current µA every second. This is a pure mathematical
  estimation, without any real current measurements.
  Basically, the main consumers are the MCU and the LEDs, so everything else
  will be handled as simple constant consumption offset.
  Note that the value will only be updated once per second, while the brightness
  is updated twice, this error can be accepted.
  16.98mA total consumption at max brightness and 63 dots on (RS232 debug on)
  ~2.75mA total consumtion at 1 brightness and 32 dots on (RS232 debug on)
  ~2.75mA with out LEDs inserted or brighntess 0 (RS232 debug on)
	~2.2mA with leds of brightness 0, RS232 debug off
  LED consumption one dot max brightness: 225µA

*/
static void update_consumption(void) {
	uint64_t consumptionOld = g_state.consumption;
	/*Consumption by the LEDs depending on the brightness and number of LEDs
		first multiplication fits in 16 bits: 140 max dots * 255 max brightness
	*/
	uint32_t uas = (uint16_t)g_state.dotsOn * (uint16_t)g_state.brightness;
	uas *= (uint32_t)g_settings.consumptionLedOneMax;
	uas /= 255;
	/*
	  constant offset values
	  Vcc measurement: 16µA
	  Poti:            5µA
	  LM358-1.2:       31µA
	  MCP 6051:        29µA
	  Sum:             81µA
	  DCF77 module:    70µA
	  ATXMEGA standby: <1µA
	  Oscilloscope sampling for 1..2 seconds: brighntess = 1,
	  no RS232 and no DCF77: 847µA -> assume 840µA without LEDs

	  According to data sheet:
	  Active supply 2.4V @ 2MHz: ~1100µA
	  Idle supply 2.4V @ 2MHz:    ~340µA
	  2MHz RC supply:             ~120µA
	  Each timer:                  ~18µA
	  RS232:                      ~7.5µA -> so little, ignore

	usually there is 8% cpu load -> 88µA
	and there is 12% rc load ->     14µA
	and 4% idle load ->             13µA
	                           sum: 115µA
	so the constant offset for consumption is approx: 840 - 115 -> 725µA

	*/
	if (dcf77_is_enabled()) {
		uas += 88; //timer ~18µA + receiver ~70µA
	}
	uas += (uint16_t)g_state.performanceRcRunning * 120 / 256; //120 / 256;
	uas += (uint16_t)g_state.performanceCpuRunning * 1100 / 256; //1100 / 256;
	uas += (uint16_t)(g_state.performanceRcRunning - g_state.performanceCpuRunning) * 340 / 256; //340 / 256;
	uas += 725; //constant offset
	g_state.consumption += uas; //64 bit addition
	if (g_settings.debugRs232 == 0xA) {
		//the sprintf implementation does not support %llu, so values will be wrong
		//after an extended period
		uint32_t delta = g_state.consumption - consumptionOld;
		uint32_t cons = g_state.consumption;
		DbgPrintf_P(PSTR("Consume: %luuAs (+%luuA)\r\n"), cons, delta);
	}
}

static void update_ldr(void) {
	uint8_t workmode;
	uint16_t v2 = 0;
	uint16_t v1 = 0;
	uint16_t v0 = 0;
	uint16_t v;
	uint8_t i;
	//start with 1MOhm
	adca_startup();
	PORTQ.PIN2CTRL = PORT_OPC_TOTEM_gc; //1MOhm
	PORTE.PIN4CTRL = PORT_OPC_TOTEM_gc; //1KOhm
	PORTE.PIN7CTRL = PORT_OPC_TOTEM_gc; //~50KOhm
	PORTA.PIN1CTRL = PORT_OPC_TOTEM_gc | PORT_ISC_INPUT_DISABLE_gc; //the ADC pin
	PORTQ.DIRSET = 4;
	PORTQ.OUTSET = 4;
	_delay_ms(2.0);
	uint16_t results[8];
	adca_getQuad(1, 1, 1, 1, ADC_REFSEL_VCC_gc, &(results[0]));
	adca_getQuad(1, 1, 1, 1, ADC_REFSEL_VCC_gc, &(results[4]));
	for (i = 0; i < 8; i++) {
		v2 += results[i];
	}
	PORTQ.DIRCLR = 4;
	PORTQ.OUTCLR = 4;
	PORTE.DIRSET = 0x80;
	PORTE.OUTSET = 0x80;
	for (i = 0; i < 8; i++) {
		v1 += adca_get(1);
	}
	PORTE.DIRCLR = 0x80;
	PORTE.OUTCLR = 0x80;
	PORTE.DIRSET = 0x10;
	PORTE.OUTSET = 0x10;
	for (i = 0; i < 8; i++) {
		v0 += adca_get(1);
	}
	adca_stop();
	PORTQ.PIN2CTRL = PORT_OPC_PULLDOWN_gc;
	PORTE.PIN4CTRL = PORT_OPC_PULLDOWN_gc;
	PORTE.PIN7CTRL = PORT_OPC_PULLDOWN_gc;
	PORTE.DIRCLR = 0x90;
	PORTE.OUTCLR = 0x90;
	PORTQ.DIRCLR = 4;
	PORTQ.OUTCLR = 4;
	v0 /= 8;
	v1 /= 8;
	v2 /= 8;
	if (v0 < 4000) {
		workmode = 0;
		v = v0;
	} else if (v1 < 4000) {
		workmode = 1;
		v = v1;
	} else {
		workmode = 2;
		v = v2;
	}
	g_state.ldr = (workmode << 14) | v;
/*
	DbgPrintf_P(PSTR("0x%x\r\n"), g_ldr);
*/
}

static void update_temperature(void) {
	uint16_t v = 0;
	uint8_t i;
	adcb_startup();
	PORTQ.PIN2CTRL = PORT_OPC_TOTEM_gc;
	PORTB.PIN0CTRL = PORT_OPC_TOTEM_gc | PORT_ISC_INPUT_DISABLE_gc; //the ADC pin
	PORTQ.OUTSET = 4;
	PORTQ.DIRSET = 4;
	_delay_ms(0.05); //reaches 80% of the voltag after 20µs
	for (i = 0; i < 8; i++) {
		v += adcb_get(0);
	}
	adcb_stop();

	PORTQ.DIRCLR = 4;
	PORTQ.OUTCLR = 4;
	PORTQ.PIN2CTRL = PORT_OPC_PULLDOWN_gc;
	/* 1. step, calculate resistance of sensor.
		Rx = R1/((1.6*4095*8)/v -1);
		Rx = 4700/(52416/v -1);
		Rx = (4700*v)/(52416 -v)
*/
	//uint32_t ohm = ((uint32_t)v*4700)/(3069-v);
	uint32_t ohm = ((uint32_t)v*4700)/(52416-v);
	/* 3. grade polynomia with 0, 25, 50 and 80°C:
		2.12x10^-9*ohm^3 -0.0000224786*ohm^2 + 0.128189*ohm - 157.224
		3. grade not representable in 32bit.

		2. grade polynomia: with 0, 50 and 100°C
 -0.0000070424599426781825*x^2 +0.09231465863293291*x-130.5806904123189

		Calculate with 1/10 grad and then round proper

	int16_t gradcelsius = (-(ohm*ohm*10/141996) + (ohm*100/108) - 1306);

	Error calculation:
	AD value sum 8xsampled: 15734, R1 +-1% 4700Ohm:
	RxMin = 1994Ohm @ R1=4653  -> temperature: 25,49°C
	RxBest = 2015Ohm @ R1=4700 -> temperature: 26,8°C
	RxMax = 2035Ohm @ R1=4747  -> temperature: 28,11°C
	Rounding error by ohm*100/108: +0.56°C
	Rounding error by polynomia: -0.002°C
-69: Calibration offset
	*/
	g_state.gradcelsius10 = (-(ohm*ohm*10/141996) + (ohm*200/217) - 1306 - 182);
}

/*
To not get confused with all the various light and bright values:
g_state.ldr -> sampled A/D value from photoresistor
g_state.brightnessLdr -> calculated from g_state.ldr, would be used if auto adjust
g_settings.brightness -> would be used under manual control
g_state.brigthness -> current brightness for display to use
*/
static void update_display_brightness(void) {
	if ((g_settings.brightnessAuto) || (g_dispUpdate == updateLightText) ||
	    (g_dispUpdate == updateDispbrightText)) {
		update_ldr();
	}
	uint8_t newbrightness = 0;
	if ((g_settings.brightnessAuto) || (g_dispUpdate == updateDispbrightText)) {
		if ((g_settings.brightnessNoOff)  || (g_state.displayNoOffCd)) {
			newbrightness = 1;
		}
		if (g_state.ldr < 0x8CE4) { //mode 2 + 3300
			newbrightness = 4;
		}
		if (g_state.ldr < 0x8C62) { //mode 2 + 3250
			newbrightness = 10;
		}
		if (g_state.ldr < 0x8BB8) { //mode 2 + 3000
			newbrightness = 20;
		}
		if (g_state.ldr < 0x87D0) { //mode 2 + 2000
			newbrightness = 40;
		}
		if (g_state.ldr < 0x8190) { //mode 2 + 400
			newbrightness = 90;
		}
		if (g_state.ldr < 0x47D0) { //mode 1 + 2000
			newbrightness = 128;
		}
		if (g_state.ldr < 0x4460) { //mode 1 + 1200
			newbrightness = 196;
		}
		if (g_state.ldr < 0x4000) { //mode 0 + 0
			newbrightness = 255;
		}
		g_state.brightnessLdr = newbrightness;
	}
	if (g_settings.brightnessAuto) {
		//slow adjust
		if (newbrightness < g_state.brightness) {
			g_state.brightUpCd = 3;
			if (g_state.brightDownCd) {
				newbrightness = g_state.brightness;
				g_state.brightDownCd--;
			} else if ((newbrightness < (g_state.brightness - 9)) &&
				    (g_state.brightness > 20)) {
				newbrightness = g_state.brightness - 10;
			} else {
				newbrightness = g_state.brightness - 1;
			}
		} else if (newbrightness > g_state.brightness) {
			g_state.brightDownCd = 3;
			if (g_state.brightUpCd) {
				newbrightness = g_state.brightness;
				g_state.brightUpCd--;
			} else if (newbrightness > (g_state.brightness + 9)) {
				newbrightness = g_state.brightness + 10;
			} else {
				newbrightness = g_state.brightness + 1;
			}
		} else {
			g_state.brightUpCd = 2;
			g_state.brightDownCd = 2;
		}
	} else {
		newbrightness = g_settings.brightness;
	}
	if (newbrightness != g_state.brightness) {
		disp_configure_set(newbrightness, g_settings.displayRefresh);
		g_state.brightness = newbrightness;
	}
	if (g_settings.debugRs232 == 3) {
		DbgPrintf_P(PSTR("BrVals: %u %i %i %i %i %i\r\n"),
		g_state.ldr, g_state.brightnessLdr, g_settings.brightness,
  	g_state.brightness, newbrightness, g_settings.brightnessAuto);
	}
}

static void watchdog_enable(void) {
	CCP = 0xD8; //change protection
	//WDT_CTRL = (9 << 2) | 3; //enable with 4seconds timeout
	WDT_CTRL = (0xA << 2) | 3; //enable with 8seconds timeout
}

#define SLOW_RISING
static uint8_t irKeysRead(void) {
	uint16_t val[4];
	uint8_t key = 0;
	uint8_t numkeys = 0;
	uint16_t avg = 0;
#ifdef SLOW_RISING
	PORTA.PIN2CTRL = PORT_OPC_PULLDOWN_gc;
#else
	PORTA.PIN2CTRL = PORT_OPC_TOTEM_gc;
	PORTA.DIRSET = 0x04;
	PORTA.OUTCLR = 0x04;
#endif
	_delay_us(50.0);
	adca_startup();
	adca_getQuad(3, 4, 5, 6, ADC_REFSEL_VCC_gc, val);
#ifdef SLOW_RISING
	PORTA.PIN2CTRL = PORT_OPC_PULLUP_gc;
#else
	PORTA.OUTSET = 0x04;
#endif
	adca_stop();
	for (uint8_t i = 0; i < 4; i++) {
		avg += val[i];
	}
	g_state.keyDebugAd = val[1];
	if (g_settings.debugRs232 == 4) {
		DbgPrintf_P(PSTR("IR %u %u %u %u\r\n"), val[0], val[1], val[2], val[3]);
	}
	avg = (avg >> 2) * 9/10; //90% limit of average value
	for (uint8_t i = 0; i < 4; i++) {
		if (val[i] < avg) {
			if (g_settings.debugRs232) {
				DbgPrintf_P(PSTR("Pressed %c\r\n"), 'A'+i);
			}
			key = i + 1;
			numkeys++;
		}
	}
	if (numkeys == 1) {
		return key;
	}
	return 0;
}

static void update_alarmAndTimer(void) {
	if (g_state.timerCountdownSecs) {
		g_state.timerCountdownSecs--;
		if (!g_state.timerCountdownSecs) {
			g_state.soundEnabledCd = g_settings.soundAutoOffMinutes*60;
		}
	}
	if ((g_settings.alarmEnabled) && (g_state.dcf77Synced)) {
		uint32_t temp = g_state.time;
		uint8_t s = temp % 60;
		if (s == 0) {
			uint8_t m = (temp / 60) % 60;
			uint8_t h = (temp / (60*60)) % 24;
			if ((h == g_settings.alarmHour) && (m == g_settings.alarmMinute)) {
				g_state.soundEnabledCd = g_settings.soundAutoOffMinutes*60;
			}
		}
	}
	if (g_state.soundEnabledCd) {
		g_state.soundEnabledCd--;
		//half second beep, half second off, etc
		sound_beep(g_settings.soundVolume, g_settings.soundFrequency, 500);
	}
}

//run 8 times per second
static void run8xS(void) {
	//check for RS232 input
	if (g_settings.debugRs232 >= 2) {
		char c = rs232_getchar();
		if (c == 'w') {
			menu_keypress(3);
		}
		if (c == 'a') {
			menu_keypress(1);
		}
		if (c == 's') {
			menu_keypress(2);
		}
		if (c == 'd') {
			menu_keypress(4);
		}
		g_state.displayNoOffCd = 60;
	}
	//update DCF77
	if (dcf77_update()) {
		dcf77_disable();
		g_state.dcf77ResyncCd = 60*60*4; //4 hours to next sync
		menu_keypress(100); //auto switch to clock, when in dcf77 view
	}
	//read in IR key sensors
	uint8_t irKey = 0;
	if (dcf77_is_idle()) { //reduce noise (do not measure while bit comes in)
		/*if we never synced, and tried for more than 10minutes and if no key
		  press is required to stop sound, disable keys completely until we got
		  sync.
		*/
		if ((g_state.dcf77Synced) || (g_state.time < (10*60)) ||
		    (dcf77_is_enabled() == 0) || (g_state.soundEnabledCd)) {
			irKey = irKeysRead();
		}
	}
	if (irKey) {
		g_state.displayNoOffCd = 60; //60s countdown
		if (irKey != g_state.irKeyLast) {
			if (g_state.soundEnabledCd) {
				g_state.soundEnabledCd = 0; //disable beep
			} else {
				menu_keypress(irKey);
			}
			g_state.irKeyCd = 6;
		} else {
			if (g_state.irKeyCd == 0) {
				//continuous keypress for repeated menu press of same key
				menu_keypress(irKey);
				g_state.irKeyCd = 4;
			}
		}
	}
	g_state.irKeyLast = irKey;
	if (g_state.irKeyCd) {
		g_state.irKeyCd--;
	}
}

//run 4 times per second
static void run4xS(void) {
	if (g_dispUpdate) {
		g_dispUpdate();
		menu_redraw();
	}
}

//run 2 times per second
static void run2xS(void) {
	//update display brightness
	update_display_brightness();
}

#define PERFORMANCE_MAXTICKS (F_CPU/256ULL)

static void update_performance(void) {
	//get and reset counter
	cli();
	uint16_t rcOn = TCC1.CNT;
	TCC1.CNT = 0;
	sei();
	uint16_t cpuOn = rcOn - g_state.performanceOff;
	g_state.performanceOff = 0;
	uint32_t percRc = ((uint32_t)rcOn * 256UL) / PERFORMANCE_MAXTICKS;
	uint32_t percCpu = ((uint32_t)cpuOn * 256UL) / PERFORMANCE_MAXTICKS;
	g_state.performanceRcRunning = percRc;
	g_state.performanceCpuRunning = percCpu;
	//debug keep alive
	if (g_settings.debugRs232 >= 1) {
		DbgPrintf_P(PSTR("Ping R:%u%% C:%u%%\r\n"), (uint16_t)percRc * 100 /256, (uint16_t)percCpu * 100 /256);
		//DbgPrintf_P(PSTR("Ping R:%u C:%u\r\n"), rcOn, cpuOn);
	}
}


//run 1 times per second
static void run1xS(void) {
	//increase time tick
	g_state.time++;
	//reset watchdog
	wdt_reset();
	//run calibration
	int16_t res = calibrate_update();
	if (res != 0x7FFF) {
		g_state.freqdelta = res;
		dcf77_enable(g_state.freqdelta);
	}
	//count down for no display off due to key press
	if (g_state.displayNoOffCd) {
		g_state.displayNoOffCd--;
	}
	//calc time to dcf77 resync
	if (g_state.dcf77ResyncCd) {
		g_state.dcf77ResyncCd--;
		if (g_state.dcf77ResyncCd == 0) {
			config_save();
			calibrate_start(); //first calibrate before updateing dcf77
		}
	}
	//update temperature
	update_temperature(); //may not be run before update_ldr()
	//update voltage and charger
	charger_update();
	//update alarm and timer
	update_alarmAndTimer();
	//calculate consumption
	update_consumption();
	//cpu load and idle times
	update_performance();
}

static void reset_print(void) {
	uint8_t resetsource = RST.STATUS;
	rs232_sendstring_P(PSTR("Reset:"));
	if (resetsource & 0x1) {
		rs232_sendstring_P(PSTR(" PowerOn"));
	}
	if (resetsource & 0x2) {
		rs232_sendstring_P(PSTR(" External"));
	}
	if (resetsource & 0x4) {
		rs232_sendstring_P(PSTR(" Brownout"));
	}

	if (resetsource & 0x8) {
		rs232_sendstring_P(PSTR(" Watchdog"));
	}

	if (resetsource & 0x10) {
		rs232_sendstring_P(PSTR(" Debug"));
	}

	if (resetsource & 0x20) {
		rs232_sendstring_P(PSTR(" Software"));
	}
	RST.STATUS = 0x3F; //clear all bits
	rs232_sendstring_P(PSTR("\r\n"));
}

static void performance_counter_init(void) {
	PR_PRPC &= ~PR_TC1_bm;
	TCC1.CTRLA = TC_CLKSEL_DIV256_gc; //allows 32MHz clock to fit into > 1s sampling
	TCC1.CTRLB = 0x00; // select Modus: Normal
	TCC1.CTRLC = 0;
	TCC1.CTRLD = 0; //no capture
	TCC1.CTRLE = 0; //normal 16bit counter
	TCC1.INTCTRLA = 0; // no interrupt
	TCC1.INTCTRLB = 0; // no interrupt
	TCC1.PER = 0xFFFF;
	TCC1.CNT = 0x0;    //reset counter
}

int main(void) {
	pull_all_pins();
	watchdog_enable();
	power_setup();
	disp_rtc_setup();
	g_settings.debugRs232 = 1; //gets overwritten by config_load() anyway
	rs232_sendstring_P(PSTR("Simple-Clock V1.02\r\n"));
	config_load();
	g_state.batteryCharged = g_settings.batteryCapacity;
	g_state.batteryCharged *= (60*60);//mAh -> mAs
	sei();
	reset_print();
	gui_init();
	charger_calib(); //takes a long time
	calibrate_start();
	if (g_settings.debugRs232 >= 2) {
		rs232_rx_init();
	}
	disp_configure_set(g_settings.brightness, g_settings.displayRefresh);
	g_state.brightness = g_settings.brightness;
	rfm12_standby();
	performance_counter_init();
	g_state.subsecond = rtc_8thcounter;
	while(1) {
		uint8_t rtc_8thcounter_l = rtc_8thcounter; //rtc_8thcounter updated by interrupt
		if (rtc_8thcounter_l != g_state.subsecond) { //1/8th second passed
			run8xS();
			if ((g_state.subsecond & 1) == 0) {
				run4xS();
			}
			if ((g_state.subsecond & 3) == 0) {
				run2xS();
			}
			if ((g_state.subsecond & 7) == 0) {
				run1xS();
			}
			/*g_state.subsecond should be now equal to rtc_8thcounter, if not we
			  will try to catch up and do not skip updates. Only after 32sec behind,
			  we skip.
			  Needing to catch up more than one second might result in missing the
			  alarm!
			*/
			g_state.subsecond++;
		}
		if (((PR_PRPD & PR_PRPE & PR_PRPF) == 0x7F) &&
		   ((PR_PRPA & PR_PRPB) == 0x7) &&
		   ((PR_PRPC & 0x7D) == 0x7D) && //TC1 may be active for performance measurement
		   (PR_PRGEN == 0x1B)) {
			//if no periphery is running except the rtc, stop the cpu
			SLEEP.CTRL = (3<<1) | 1; //power save
		} else {
			//00 7F 6F 7F 07 07 1B
			if (g_settings.debugRs232 == 9) {
				rs232_puthex(PR_PRPC);
				rs232_puthex(PR_PRPD);
				rs232_puthex(PR_PRPE);
				rs232_puthex(PR_PRPF);
				rs232_puthex(PR_PRPA);
				rs232_puthex(PR_PRPB);
				rs232_puthex(PR_PRGEN);
				rs232_putchar('\r');
				rs232_putchar('\n');
			}
			SLEEP.CTRL = 1; //idle
		}
		rtc_waitsafeoff();
		uint16_t cntBeforeOff = TCC1.CNT;
		g_state.performanceUp = 0;
		sleep_cpu();
		uint16_t cntAfterOff = TCC1.CNT;
		SLEEP.CTRL = 0;
		if (g_state.performanceUp) {
			g_state.performanceOff += g_state.performanceUp - cntBeforeOff;
		} else {
			//counters never overflow here, so its safe
			g_state.performanceOff += cntAfterOff - cntBeforeOff;
		}
	}
}
