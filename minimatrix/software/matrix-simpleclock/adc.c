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

#include <avr/io.h>
#include <avr/pgmspace.h>
#include <avr/interrupt.h>
#include <stdio.h>

#include "config.h"
#include "rs232.h"
#include "main.h"
#include "debug.h"

#define ADCACAL0_offset 0x20
#define ADCBCAL0_offset 0x24

/*according to
http://blog.frankvh.com/2010/01/03/atmel-xmega-adc-problems-solutions/
62kHz gets the best results -> 2M/32
Quality improvement over 250kHz: more than 20 times less noise
*/
#define ADC_PRESCALER_USE ADC_PRESCALER_DIV32_gc

uint8_t adc_getCalibration(uint8_t address) {
	cli();
	NVM_CMD = NVM_CMD_READ_CALIB_ROW_gc;
	uint16_t result = pgm_read_byte(address);
	NVM_CMD = NVM_CMD_NO_OPERATION_gc;
	sei();
	cli();
	NVM_CMD = NVM_CMD_READ_CALIB_ROW_gc;
	result |= pgm_read_byte(address+1) >> 8;
	NVM_CMD = NVM_CMD_NO_OPERATION_gc;
	sei();
	return result;
}

static void adca_calibrate(void) {
	uint16_t cal = adc_getCalibration(PROD_SIGNATURES_START+ADCACAL0_offset);
	ADCA_CAL = cal;
	if (g_settings.debugRs232 == 8) {
		DbgPrintf_P(PSTR("ADCA calib=%u(0x%x)\r\n"), cal, cal);
	}
}

static void adcb_calibrate(void) {
	uint16_t cal = adc_getCalibration(PROD_SIGNATURES_START+ADCBCAL0_offset);
	ADCB_CAL = cal;
	if (g_settings.debugRs232 == 8) {
		DbgPrintf_P(PSTR("ADCB calib=%u(0x%x)\r\n"), cal, cal);
	}
}

uint16_t adca_get2(uint8_t pin, uint8_t reference) {
	ADCA_CTRLA = ADC_ENABLE_bm;
	ADCA_CTRLB = ADC_RESOLUTION_12BIT_gc;
	ADCA_REFCTRL = reference | ADC_BANDGAP_bm; //+ bandgap
	ADCA_PRESCALER = ADC_PRESCALER_USE;
	ADCA_CH0_CTRL = ADC_CH_INPUTMODE_SINGLEENDED_gc;
	ADCA_CH0_MUXCTRL = (pin << 3);
	ADCA_CH0_CTRL |= ADC_CH_START_bm;
	while(!ADCA_CH0_INTFLAGS);
	ADCA_CH0_INTFLAGS = 1; //clear the bit
	return ADCA_CH0_RES;
}

#if 0
//does not get any usable values...
int16_t adca_getDelta(uint8_t pinPos, uint8_t pinNeg, uint8_t reference, uint8_t gain) {
	ADCA_CTRLA = ADC_ENABLE_bm;
	ADCA_CTRLB = ADC_RESOLUTION_12BIT_gc | ADC_CONMODE_bm;
	ADCA_REFCTRL = reference | ADC_BANDGAP_bm; //+ bandgap
	ADCA_PRESCALER = ADC_PRESCALER_USE;
	if (pinNeg >= ADC_CH_MUXNEG_PIN4_gc) { //4..7: only with gain, 0..3 only without gain
		ADCA_CH0_CTRL = gain | ADC_CH_INPUTMODE_DIFFWGAIN_gc;
	} else {
		ADCA_CH0_CTRL = ADC_CH_GAIN_1X_gc | ADC_CH_INPUTMODE_DIFF_gc;
	}
	ADCA_CH0_MUXCTRL = pinPos | pinNeg;
	ADCA_CH0_CTRL |= ADC_CH_START_bm;
	while(!ADCA_CH0_INTFLAGS);
	ADCA_CH0_INTFLAGS = 1; //clear the bit
	return ADCA_CH0_RES;
}
#endif

uint16_t adca_get(uint8_t pin) {
	return adca_get2(pin, ADC_REFSEL_VCC_gc);
}

void adca_getQuad(uint8_t pin0, uint8_t pin1, uint8_t pin2, uint8_t pin3, uint8_t reference, uint16_t * result) {
	ADCA_CTRLA = ADC_ENABLE_bm;
	ADCA_CTRLB = ADC_RESOLUTION_12BIT_gc;
	ADCA_REFCTRL = reference | ADC_BANDGAP_bm; //+ bandgap
	//ADCA_PRESCALER = ADC_PRESCALER_DIV8_gc;
	ADCA_PRESCALER = ADC_PRESCALER_USE;
	ADCA_CH0_CTRL = ADC_CH_INPUTMODE_SINGLEENDED_gc;
	ADCA_CH0_MUXCTRL = (pin0 << 3);
	ADCA_CH0_CTRL |= ADC_CH_START_bm;
	ADCA_CH1_CTRL = ADC_CH_INPUTMODE_SINGLEENDED_gc;
	ADCA_CH1_MUXCTRL = (pin1 << 3);
	ADCA_CH1_CTRL |= ADC_CH_START_bm;
	ADCA_CH2_CTRL = ADC_CH_INPUTMODE_SINGLEENDED_gc;
	ADCA_CH2_MUXCTRL = (pin2 << 3);
	ADCA_CH2_CTRL |= ADC_CH_START_bm;
	ADCA_CH3_CTRL = ADC_CH_INPUTMODE_SINGLEENDED_gc;
	ADCA_CH3_MUXCTRL = (pin3 << 3);
	ADCA_CH3_CTRL |= ADC_CH_START_bm;
	while(!ADCA_CH0_INTFLAGS);
	ADCA_CH0_INTFLAGS = 1; //clear the bit
	result[0] = ADCA_CH0_RES;
	while(!ADCA_CH1_INTFLAGS);
	ADCA_CH1_INTFLAGS = 1; //clear the bit
	result[1] = ADCA_CH1_RES;
	while(!ADCA_CH2_INTFLAGS);
	ADCA_CH2_INTFLAGS = 1; //clear the bit
	result[2] = ADCA_CH2_RES;
	while(!ADCA_CH3_INTFLAGS);
	ADCA_CH3_INTFLAGS = 1; //clear the bit
	result[3] = ADCA_CH3_RES;
}


uint16_t adcb_get(uint8_t pin) {
	ADCB_CTRLA = ADC_ENABLE_bm;
	ADCB_CTRLB = ADC_RESOLUTION_12BIT_gc;
	ADCB_REFCTRL = ADC_REFSEL_VCC_gc | ADC_BANDGAP_bm;
	ADCB_PRESCALER = ADC_PRESCALER_USE;
	ADCB_CH0_CTRL = ADC_CH_INPUTMODE_SINGLEENDED_gc;
	ADCB_CH0_MUXCTRL = (pin << 3);
	ADCB_CH0_CTRL |= ADC_CH_START_bm;
	while(!ADCB_CH0_INTFLAGS);
	ADCB_CH0_INTFLAGS = 1; //clear the bit
	return ADCB_CH0_RES;
}

void adca_startup(void) {
	PR_PRPA &= ~PR_ADC_bm; //enable power
	adca_calibrate();
}

void adcb_startup(void) {
	PR_PRPB &= ~PR_ADC_bm; //enable power
	adcb_calibrate();
}

void adca_stop(void) {
	ADCA_REFCTRL &= ~ADC_BANDGAP_bm;
	ADCA_CTRLA &= ~ADC_ENABLE_bm;
	PR_PRPA |= PR_ADC_bm; //disable power
}

void adcb_stop(void) {
	ADCB_REFCTRL &= ~ADC_BANDGAP_bm;
	ADCB_CTRLA &= ~ADC_ENABLE_bm;
	PR_PRPB |= PR_ADC_bm; //disable power
}
