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
#include "sound.h"
#include "dcf77.h"
#include "clocks.h"
#include "rfm12.h"
#include "adc.h"
#include "gui.h"
#include "config.h"
#include "charger.h"
#include "timeconvert.h"
#include "rc5decoder.h"
#include "logger.h"
#include "debug.h"
#include "touch.h"


settings_t g_settings; //permanent settings
sysstate_t g_state; //floating state of main program
uint8_t g_debug __attribute__ ((section (".noinit")));
uint8_t g_debugint __attribute__ ((section (".noinit")));

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
	uas += 674; //constant offset (after R39 removal, otherwise + 51µA)
	if (g_state.rc5modeis) {
		uas += 350; //typical current according to datasheet at 3.3V.
	}
	if (g_state.rfm12modeis) {
		uas += 11000; //typical current according to datasheet at 2.7V (rx mode 433MHz)
	}
	g_state.consumption += uas; //64 bit addition
	if (g_settings.debugRs232 == 0xA) {
		char buffer[DEBUG_CHARS+1];
		buffer[DEBUG_CHARS] = '\0';
		//the sprintf implementation does not support %llu, so values will be wrong
		//after an extended period
		uint32_t delta = g_state.consumption - consumptionOld;
		uint32_t cons = g_state.consumption;
		snprintf_P(buffer, DEBUG_CHARS, PSTR("Consume: %luuAs (+%luuA)\r\n"), cons, delta);
		rs232_sendstring(buffer);
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
	char buffer[DEBUG_CHARS+1];
	buffer[DEBUG_CHARS] = '\0';
	snprintf(buffer, DEBUG_CHARS, "0x%x\r\n", g_ldr);
	rs232_sendstring(buffer);
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
static void update_display_brightnessMeasure(void) {
	if ((g_settings.brightnessAuto) || (g_dispUpdate == updateLightText) ||
	    (g_dispUpdate == updateDispbrightText)) {
		update_ldr();
	}
	uint8_t newbrightness = 0;
	if ((g_settings.brightnessAuto) || (g_dispUpdate == updateDispbrightText)) {
		if ((g_settings.brightnessNoOff) || (g_state.displayNoOffCd)) {
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
}

static void update_display_brightnessAdjust(void) {
	uint8_t newbrightness;
	//1. determine target brigtness
	if ((g_state.powersaveEnabled) && (g_state.displayNoOffCd == 0)) {
		newbrightness = 0; //power save
	} else if (g_settings.brightnessAuto) {
		//newbrightness = value if the LDR would be used
		newbrightness = g_state.brightnessLdr;
	} else {
		//user selectedbrightness
		newbrightness = g_settings.brightness;
	}
	//newbrightness = value according to LDR changes or user select
	//2. count down adjustment blocking
	if (g_state.brightDownCd) g_state.brightDownCd--;
	if (g_state.brightUpCd) g_state.brightUpCd--;
	//3. fast adjust within few seconds target brightness should be reached
	int16_t brightdelta = newbrightness - g_state.brightness;
	newbrightness = g_state.brightness; //default case, keep as current
	if (brightdelta < 0) {
		g_state.brightUpCd = 8; //when ramping down, prevent ramping up soon
		if ((g_state.brightDownCd == 0) || (brightdelta < -20)) {
			int16_t adjust = brightdelta/4;
			if (adjust == 0) {
				adjust = -1;
			}
			newbrightness += adjust;
		}
	} else if (brightdelta > 0) {
		g_state.brightDownCd = 8; //when ramping up, prevent ramping down soon
		if ((g_state.brightUpCd == 0) || (brightdelta > 20)) {
			int16_t adjust = brightdelta/4;
			if (adjust == 0) {
				adjust = 1;
			}
			newbrightness += adjust;
		}
	} else {
		g_state.brightUpCd = 6;
		g_state.brightDownCd = 6;
	}
	//4. update brightness if value differs
	if (newbrightness != g_state.brightness) {
		disp_configure_set(newbrightness, g_settings.displayRefresh);
		g_state.brightness = newbrightness;
	}
	//debug output:
	if (g_settings.debugRs232 == 3) {
		char buffer[DEBUG_CHARS+1];
		buffer[DEBUG_CHARS] = '\0';
		snprintf_P(buffer, DEBUG_CHARS, PSTR("BrVals: %u %i %i %i %i %i\r\n"),
		g_state.ldr, g_state.brightnessLdr, g_settings.brightness,
  	brightdelta, newbrightness, g_settings.brightnessAuto);
		rs232_sendstring(buffer);
		if ((g_state.powersaveEnabled) && (g_state.displayNoOffCd)) {
			snprintf_P(buffer, DEBUG_CHARS, PSTR("Powersave in %is\r\n"), g_state.displayNoOffCd);
			rs232_sendstring(buffer);
		}
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
	char buffer[DEBUG_CHARS+1];
	buffer[DEBUG_CHARS] = '\0';
#ifdef SLOW_RISING
	PORTA.PIN2CTRL = PORT_OPC_PULLDOWN_gc;
#else
	PORTA.PIN2CTRL = PORT_OPC_TOTEM_gc;
	PORTA.DIRSET = 0x04;
	PORTA.OUTCLR = 0x04; //the output is low active, enable IR LEDs
#endif
	_delay_us(50.0);
	adca_startup();
	adca_getQuad(3, 4, 5, 6, ADC_REFSEL_VCC_gc, val);
#ifdef SLOW_RISING
	PORTA.PIN2CTRL = PORT_OPC_PULLUP_gc;
#else
	PORTA.OUTSET = 0x04; //disable IR LEDs
#endif
	adca_stop();
	for (uint8_t i = 0; i < 4; i++) {
		avg += val[i];
	}
	g_state.keyDebugAd = val[1];
	if (g_settings.debugRs232 == 4) {
		snprintf_P(buffer, DEBUG_CHARS, PSTR("IR %u %u %u %u\r\n"), val[0], val[1], val[2], val[3]);
		rs232_sendstring(buffer);
	}
	avg = (avg >> 2) * 9/10; //90% limit of average value
	for (uint8_t i = 0; i < 4; i++) {
		if (val[i] < avg) {
			if (g_settings.debugRs232) {
				snprintf_P(buffer, DEBUG_CHARS, PSTR("Pressed %c\r\n"), 'A'+i);
				rs232_sendstring(buffer);
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

static void update_alarmAndSound(void) {
	if (g_state.timerCountdownSecs) {
		g_state.timerCountdownSecs--;
		if (!g_state.timerCountdownSecs) {
			g_state.soundEnabledCd = g_settings.soundAutoOffMinutes*60;
		}
	}
	if (g_state.soundEnabledCd) {
		g_state.soundEnabledCd--;
		//half second beep, half second off, etc
		sound_beep(g_settings.soundVolume, g_settings.soundFrequency, 500);
	}
}

static uint8_t rc5keyget(void) {
	uint16_t rc5data = rc5getdata();
	if (rc5data) {
		if (g_settings.debugRs232 == 0xD) {
			char buffer[DEBUG_CHARS+1];
			buffer[DEBUG_CHARS] = '\0';
			snprintf_P(buffer, DEBUG_CHARS, PSTR("RC-5: 0x%x\r\n"), rc5data);
			rs232_sendstring(buffer);
		}
		rc5data &= ~0x800; //mask out toggle bit
		if ((g_state.rc5entermode) && (g_state.rc5entermode <= RC5KEYS)) {
			g_settings.rc5codes[g_state.rc5entermode-1] = rc5data;
		} else {
			uint8_t i;
			for (i = 0; i < RC5KEYS; i++) {
				if (g_settings.rc5codes[i] == rc5data) {
					return i+1;
				}
			}
		}
	}
	return 0;
}

static void update_rc5control(void) {
	uint8_t modeshould = 0;
	if (g_state.rc5Cd) {
		g_state.rc5Cd--;
	}
	if (((g_state.rc5Cd) && (g_settings.rc5mode > 0)) ||
	   (g_settings.rc5mode == 3)) {
		modeshould = 1;
	}
	if (modeshould != g_state.rc5modeis) {
		if (modeshould) {
			rc5Init();
			if (g_settings.debugRs232 == 0xD) {
				rs232_sendstring_P(PSTR("RC-5 receiver enabled\r\n"));
			}
		} else {
			rc5Stop();
			if (g_settings.debugRs232 == 0xD) {
				rs232_sendstring_P(PSTR("RC-5 receiver disabled\r\n"));
			}
		}
		g_state.rc5modeis = modeshould;
	}
}

static void rc5keypress(void) {
	if (g_settings.rc5mode == 1) {
		g_state.rc5Cd = 60; //60seconds on
	} else if (g_settings.rc5mode == 2) {
		g_state.rc5Cd = 60*5; //5min on
	}
}

static void update_rfm12control(void) {
	DEBUG_FUNC_ENTER(update_rfm12control);
	uint8_t modeshould = 0;
	if (g_state.rfm12Cd) {
		g_state.rfm12Cd--;
	}
	if (((g_state.rfm12Cd) && (g_settings.rfm12mode > 0)) ||
	   (g_settings.rfm12mode == 3)) {
		modeshould = 1;
	}
	if (modeshould != g_state.rfm12modeis) {
		if (modeshould) {
			rs232_sendstring_P(PSTR("RFM12 enabling...\r\n"));
			rfm12_init();
		} else {
			rs232_sendstring_P(PSTR("RFM12 enter standby...\r\n"));
			rfm12_standby();
		}
		g_state.rfm12modeis = modeshould;
	}
	DEBUG_FUNC_LEAVE(update_rfm12control);
}

static void rfm12keypress(void) {
	if (g_settings.rfm12mode == 1) {
		g_state.rfm12Cd = 60; //60seconds on
	} else if (g_settings.rfm12mode == 2) {
		g_state.rfm12Cd = 60*5; //5min on
	}
}

static void increaseRfmTimeout(void) {
	/*When sending the log data, this might take some time, so do not
	  switch off the rfm12 while sending.
	  64k/23Byte -> 2850messages to send -> 3 per second -> up to 15min for all
	  messages. The observed transmission rate is 4 per second. So enough reserve.
	  This still might be too little in the case of a very high retransmission
	  rate due to bad connection. But in this case... make connection better or
	  set rfm12 to permanently on, or press some buttons to increase timeout.
	*/
	if ((g_state.rfm12modeis) && (g_state.rfm12Cd < 60*15)) {
		g_state.rfm12Cd = 60*15;
	}
}

static void updateDebugInput(void) {
	if ((g_settings.debugRs232 >= 2) || (g_state.rfm12modeis)) {
		char c = rs232_getchar();
		if (c == 0) {
			c = rfm12_getchar();
		}
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
		if (c == 'l') {
			increaseRfmTimeout();
			logger_print(0);
		}
		if (c == 'L') {
			increaseRfmTimeout();
			logger_print(1);
		}
		if (c == 'm') {
			rfm12_showstats();
		}
		if (c == 'c') {
			menu_keypress(103); //decrease battery state by 1mAh
		}
		if (c == 'C') {
			menu_keypress(104); //increase battery state by 1mAh
		}
/* Test only:
		if (c == 'h') {
			const char * buffer = PSTR("Hello world\n\r");
			rfm12_sendP(buffer, strlen_P(buffer));
		}
*/
		if (c > 0) {
			g_state.displayNoOffCd = 60;
		}
	}
}

//run 8 times per second
static void run8xS(uint8_t delayed) {
	DEBUG_FUNC_ENTER(run8xS);
	//check for RS232 input
	updateDebugInput();
	//update DCF77
	if (dcf77_is_enabled()) {
		if (dcf77_update()) {
			dcf77_disable();
			g_state.dcf77ResyncCd = 60*60*g_settings.dcf77Period; //some hours to next sync
			menu_keypress(100); //auto switch to clock, when in dcf77 view
		}
		if (g_state.dcf77ResyncTrytimeCd) {
			g_state.dcf77ResyncTrytimeCd--;
		} else {
			dcf77_disable();
			g_state.dcf77ResyncCd = 60*60*1; //1 hours to try to next sync
		}
	}
	//read in IR key sensors
	uint8_t irKey = 0;
	if (dcf77_is_idle()) { //reduce noise (do not measure while bit comes in)
		/*if we never synced, and tried for more than 10minutes and if no key
		  press is required to stop sound, disable keys completely until we got
		  sync.
		  If we synced, but tried already more than one hour, we disable keys between 2:00 and 3:59 to reduce noise again.
		*/
		if (((g_state.dcf77Synced) &&
		     ((g_state.dcf77ResyncTrytimeCd > (8*60*60)) || (g_state.timehcache > 3 ) || (g_state.timehcache < 2))) ||
		    (g_state.time < (10*60)) ||
		    (dcf77_is_enabled() == 0) || (g_state.soundEnabledCd)) {
			if (g_state.irKeyUse) {
				irKey = irKeysRead();
			}
		}
	}
	if (!g_state.irKeyUse) {
		irKey = touchKeysRead();
	}
	//read in RC-5 key
	uint8_t rc5key = rc5keyget();
	if (rc5key) {
		irKey = rc5key; //overwrites ir key behaviour
	}
	if (irKey) {
		rc5keypress();
		rfm12keypress();
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
	//smooth brightness adjustment
	update_display_brightnessAdjust();
	//update logger reports
	if (!delayed) {
		logger_print_iter(); //we skip the logger if we are behind schedule
	}
	DEBUG_FUNC_LEAVE(run8xS);
}

//run 4 times per second
static void run4xS(void) {
	DEBUG_FUNC_ENTER(run4xS);
	if (g_dispUpdate) {
		if (g_dispUpdate()) {
			menu_redraw();
		}
	}
	//update rfm12 rec
	rfm12_update();
	DEBUG_FUNC_LEAVE(run4xS);
}

//run 2 times per second
static void run2xS(void) {
	DEBUG_FUNC_ENTER(run2xS);
	//update display brightness
	update_display_brightnessMeasure();
	DEBUG_FUNC_LEAVE(run2xS);
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
		char buffer[DEBUG_CHARS+1];
		buffer[DEBUG_CHARS] = '\0';
		snprintf_P(buffer, DEBUG_CHARS, PSTR("Ping R:%u%% C:%u%%\r\n"), (uint16_t)percRc * 100 /256, (uint16_t)percCpu * 100 /256);
		//snprintf_P(buffer, DEBUG_CHARS, PSTR("Ping R:%u C:%u\r\n"), rcOn, cpuOn);
		rs232_sendstring(buffer);
	}
}

//run every full hour
static void run1xH(void) {
	//dst switches are run first, so every update cycle gets the same hour all over!
	if (g_settings.summertimeadjust) {
		uint8_t summertime = isSummertime(g_state.time, g_state.summertime);
		if (summertime != g_state.summertime) {
			if (summertime) {
				g_state.time += 60*60;
				g_state.timehcache++;
			} else {
				g_state.time -= 60*60;
				g_state.timehcache--;
			}
			g_state.summertime = summertime;
		}
	}
	uint8_t h = g_state.timehcache;
	if (h == 22) {
		config_save(); //only save once a day to not wear eeprom out
	}
	if ((h % g_settings.loggerPeriod) == 0) {
		loggerlog_batreport();
	}
}

static void updateAlarm(void) {
	if (g_state.dcf77Synced) {
		//check alarms
		uint8_t m = g_state.timemcache;
		uint8_t h = g_state.timehcache;
		for (uint8_t i = 0; i < ALARMS; i++) {
			if ((g_settings.alarmEnabled[i]) && (m == g_settings.alarmMinute[i]) && (h == g_settings.alarmHour[i])) {
				uint8_t weekday = calcweekdayfromtimestamp(g_state.time);
				uint8_t weekdaymask = 1<<(weekday);
				if (weekdaymask & g_settings.alarmWeekdays[i]) {
					g_state.soundEnabledCd = g_settings.soundAutoOffMinutes*60;
				}
			}
		}
	}
}

static void updatePowerSaving(void) {
	if (g_state.dcf77Synced) {
		uint8_t m = g_state.timemcache;
		uint8_t h = g_state.timehcache;
		//check powersave time
		if ((m == g_settings.powersaveMinuteStart) && (h == g_settings.powersaveHourStart)) {
			uint8_t weekday = calcweekdayfromtimestamp(g_state.time);
			if (g_settings.powersaveWeekdays & (1<<(weekday))) {
				g_state.powersaveEnabled |= 0x1; //dont overwrite manual mode bit
				rs232_sendstring_P(PSTR("Powersave enabled\r\n"));
			}
		}
		if ((m == g_settings.powersaveMinuteStop) && (h == g_settings.powersaveHourStop)) {
			/*We wont check the weekday here because, otherwise we wont get out of
			 powersave if stop time is the next day and the next day should not
			 enable powersave.
			*/
			g_state.powersaveEnabled &= ~0x1; //dont overwrite manual mode bit
			rs232_sendstring_P(PSTR("Powersave disabled\r\n"));
		}
	}
	//check if voltage is critically low
	if (g_state.batVoltage < 2150) {
		g_state.powersaveEnabled |= 0x4;
	} else if (g_state.batVoltage >= 2200) {
		g_state.powersaveEnabled &= ~0x4;
	}
}

//run every full minute
static void run1xM(void) {
	updateAlarm();
	updatePowerSaving();
}

//run 1 times per second
static void run1xS(void) {
	DEBUG_FUNC_ENTER(run1xS);
	//increase time tick
	g_state.time++;
	g_state.timescache++;
	if (g_state.timescache >= 60) {
		g_state.timescache = 0;
		g_state.timemcache++;
		if (g_state.timemcache >= 60) {
			g_state.timemcache = 0;
			g_state.timehcache++;
			if (g_state.timehcache >= 24) {
				g_state.timehcache = 0;
			}
			run1xH(); //migh modify hour at daylight saving time switches!
		}
		run1xM();
	}
	//reset watchdog
	wdt_reset();
	//run calibration
	int16_t res = calibrate_update();
	if (res != 0x7FFF) {
		g_state.freqdelta = res;
		dcf77_enable(g_state.freqdelta);
		g_state.dcf77ResyncTrytimeCd = 2U*60U*60U*8U; //try for 2 hours to sync
	}
	//count down for no display off due to key press
	if (g_state.displayNoOffCd) {
		g_state.displayNoOffCd--;
	}
	//calc time to dcf77 resync
	if (g_state.dcf77ResyncCd) {
		g_state.dcf77ResyncCd--;
		if (g_state.dcf77ResyncCd == 0) {
			calibrate_start(); //first calibrate before updateing dcf77
		}
	}
	//update temperature
	update_temperature(); //may not be run before update_ldr()
	//update voltage and charger
	charger_update();
	//update alarm and timer
	update_alarmAndSound();
	//calculate consumption
	update_consumption();
	//cpu load and idle times
	update_performance();
	//rfm12 and rc5 auto on off control
	update_rc5control();
	update_rfm12control();
	//update elapsed time meter
	g_settings.usageseconds++;
	DEBUG_FUNC_LEAVE(run1xS);
}

static uint8_t reset_print(void) {
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
		char buffer[DEBUG_CHARS+1];
		buffer[DEBUG_CHARS] = '\0';
		snprintf_P(buffer, DEBUG_CHARS, PSTR(" Watchdog, trace: %u"), g_debug);
		rs232_sendstring(buffer);
	} else {
		g_debug = 0;
	}
	if (resetsource & 0x10) {
		rs232_sendstring_P(PSTR(" Debug"));
	}
	if (resetsource & 0x20) {
		rs232_sendstring_P(PSTR(" Software"));
	}
	RST.STATUS = 0x3F; //clear all bits
	rs232_sendstring_P(PSTR("\r\n"));
	return resetsource;
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

static void resetcounter_increase(void) {
	g_settings.reboots++;
}

//returns 1: ir keys to use, 0: touch keys to use
static uint8_t input_detect(void) {
	PORTA.PIN2CTRL = PORT_OPC_TOTEM_gc;
	PORTA.DIRSET = 0x04; //PIN2
	PORTA.OUTSET = 0x04; //IR is low active, touch is high active
	//At IR, setting one input as low output will result in safe discharge of stray
	PORTA.PIN3CTRL = PORT_OPC_PULLDOWN_gc;
	//All other need to be inputs for measurement
	PORTA.PIN4CTRL = PORT_OPC_TOTEM_gc;
	PORTA.PIN5CTRL = PORT_OPC_TOTEM_gc;
	PORTA.PIN6CTRL = PORT_OPC_TOTEM_gc;
	waitms(10);
	//If we read a high level now, we have a touch field
	uint8_t inputvalues = PORTA.IN & 0x70;
	PORTA.PIN3CTRL = PORT_OPC_TOTEM_gc;
	if (inputvalues) {
		rs232_sendstring_P(PSTR("Touch key input\r\n"));
		return 0;
	} else {
		rs232_sendstring_P(PSTR("IR key input\r\n"));
		return 1;
	}
}

int main(void) {
	pull_all_pins();
	watchdog_enable();
	power_setup();
	disp_rtc_setup();
	g_settings.debugRs232 = 1; //gets overwritten by config_load() anyway
	rs232_sendstring_P(PSTR("Advanced-Clock V0.5\r\n"));
	config_load();
	g_state.batteryCharged = g_settings.batteryCapacity;
	g_state.batteryCharged *= (60*60);//mAh -> mAs
	sei();
	uint8_t resetsource = reset_print();
	resetcounter_increase();
	gui_init();
	charger_calib(); //takes a long time
	logger_init(); //can take some time
	loggerlog_bootup(g_settings.reboots, resetsource, g_debug, g_debugint);
	g_debug = 0;
	calibrate_start();
	g_state.irKeyUse = input_detect();
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
			uint8_t delayed = 0;
			uint8_t subsecnext = g_state.subsecond + 1;
			if (subsecnext !=  rtc_8thcounter_l) {
				delayed = 1; //something took too much time
				if (g_settings.debugRs232 == 0xE) {
					rs232_sendstring_P(PSTR("TimeE\r\n"));
				}
			}
			run8xS(delayed);
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
		} else {
			if (((g_settings.flickerWorkaround == 0) || (g_state.brightness == 0)) &&
			   ((PR_PRPD & PR_PRPE & PR_PRPF) == 0x7F) &&
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
}
