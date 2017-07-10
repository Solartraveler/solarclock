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
#include <stdio.h>
#include <string.h>
#include <avr/interrupt.h>

#include "main.h"

#include <util/delay_basic.h>
#include <avr/pgmspace.h>
#include <avr/sleep.h>
#include <avr/wdt.h>

#include "dcf77.h"
#include "rs232.h"
#include "clocks.h"
#include "displayRtc.h"
#include "config.h"
#include "timeconvert.h"


#include "dcf77statisticsdecode.h"
#include "logger.h"

//allowed: 2...16
#define DCF77HISTORY DCF77DATAMINUTES

//PD7: Input signal
//PD6: Vcc for receiver modul

#define DCF77SAMPLES 100
#define DCF77SECONDS 60
#define DCF77SIGWINDOW (DCF77SAMPLES/10)

//+-2% allowed adjust
#define DCF77PERMIN ((F_CPU/DCF77SAMPLES)*98/100)
#define DCF77PERMAX ((F_CPU/DCF77SAMPLES)*102/100)


//debug print settings
//#define DCF77DEBUGBITSIGNAL
//#define DCF77DEBUGMINUTESAMPLES
//#define DCF77DEBUGPARITY


#if DCF77HISTORY > 8
typedef uint64_t DCF77DATA;
#elif DCF77HISTORY > 4
typedef uint32_t DCF77DATA;
#elif DCF77HISTORY > 2
typedef uint16_t DCF77DATA;
#else
typedef uint8_t DCF77DATA;
#endif


uint8_t g_dcf77enabled;
uint8_t g_dcf77signalquality;
uint16_t g_dcf77samplestaken;

volatile uint8_t g_dcf77secondmatrix[DCF77SAMPLES];
volatile uint8_t g_dcf77secondindex;
volatile uint8_t g_dcf77sampled;

uint8_t g_dcf77signalindex;
DCF77DATA g_dcf77signalhistory[DCF77SECONDS];
DCF77DATA g_dcf77signalstartevidence[DCF77SECONDS];

uint8_t g_dcf77posmaxlast;
uint8_t g_dcf77nodeltacounter;

ISR(TCD0_OVF_vect) {
	//second start detection
	uint8_t index = g_dcf77secondindex;
	uint8_t secondsum = g_dcf77secondmatrix[index];
	secondsum &= 0x7F; //disable high bit
	if (PORTD.IN & 0x80) {
		if (secondsum > 0) {
			secondsum--;
		}
	} else {
		if (secondsum < 0x1E) { //max 32samples for sliding maximum
			secondsum++;
		}
		secondsum |= 0x80; //enable high bit
	}
	g_dcf77secondmatrix[index] = secondsum;
	index++;
	if (index >= DCF77SAMPLES) {
		index = 0;
	}
	g_dcf77secondindex = index;
	g_dcf77sampled++;
}

uint8_t dcf77_is_enabled(void) {
	return g_dcf77enabled;
}

uint8_t dcf77_is_idle(void) {
	if (!g_dcf77enabled) {
		return 1;
	}
	if ((PORTD.IN & 0x80) == 0) {
		return 0;
	}
	return 1;
}

//freq delta in 100th percent posivite: too fast, negative: too slow
void dcf77_enable(int16_t freqdelta) {
	uint8_t i;
	if (g_dcf77enabled) {
		return;
	}
	g_dcf77signalindex = 0;
	g_dcf77secondindex = 0;
	g_dcf77sampled = 0;
	g_dcf77nodeltacounter = 255;
	g_dcf77posmaxlast = 0;
	g_dcf77samplestaken = 0;
	for (i = 0; i < DCF77SECONDS; i++) {
		g_dcf77signalhistory[i] = 0;
		g_dcf77signalstartevidence[i] = 0;
	}
	for (i = 0; i < DCF77SAMPLES; i++) {
		g_dcf77secondmatrix[i] = 0;
	}
	PORTD.PIN7CTRL = PORT_OPC_PULLUP_gc;
	PORTD.PIN6CTRL = PORT_OPC_TOTEM_gc;
	PORTD.OUTCLR = 0x80;
	PORTD.DIRSET = 0x40;
	PORTD.OUTSET = 0x40;
	PR_PRPD &= ~(PR_TC0_bm); //power the timer
	TCD0.CTRLA = TC_CLKSEL_DIV1_gc; //divide by 1
	TCD0.CTRLB = 0x00; // select Modus: Normal
	TCD0.CTRLC = 0;
	TCD0.CTRLD = 0; //no capture
	TCD0.CTRLE = 0; //normal 16bit counter
	TCD0.PER = (((F_CPU/DCF77SAMPLES)*((int32_t)freqdelta+10000))/(int32_t)10000)-1;   //initial value (increase divider if F_CPU gets larger)
	TCD0.CNT = 0x0;    //reset counter
	TCD0.INTCTRLA = TC_OVFINTLVL_MED_gc; // medium prio interupt of overflow
	g_dcf77enabled = 1;
}

void dcf77_disable(void) {
	PORTD.PIN7CTRL = PORT_ISC_INPUT_DISABLE_gc;
	PORTD.PIN6CTRL = PORT_ISC_INPUT_DISABLE_gc;
	PORTD.OUTCLR = 0xC0;
	PORTD.DIRCLR = 0xC0;
	TCD0.CTRLA = TC_CLKSEL_OFF_gc;
	PR_PRPD |= (PR_TC0_bm); //power the timer off
	g_dcf77enabled = 0;
}


//If you program systems where safety is important, give buffer lenght as
//paramteter! Never ever do this in systems connected to the internet ;)
void dcf77_getstatus(char * targetstring) {
	sprintf_P(targetstring, PSTR("%s %i%%"), g_dcf77enabled ? "On" : "Off", g_dcf77signalquality);
}

/* Returns the sampled data byte for a given second (index) and a given minute
(history minute).
minutestart must be the index with the minute marker.
*/
static uint8_t d77x(uint8_t minutestart, uint8_t index, uint8_t historyminute) {
	index += minutestart + 1;
	if (index >= DCF77SECONDS) {
		index -=  DCF77SECONDS;
	}
	DCF77DATA value = g_dcf77signalhistory[index];
	uint8_t sample = value >> (historyminute*4) & 0xF;
	sample = sample + DCF77SIGNAL0;
	return sample;
}

static uint8_t dcf77_analyze(uint8_t startindex) {
	uint8_t minute = 0, hour = 0, day = 0, month = 0, year2digit = 0;
	uint16_t errorrate = 0;
	uint8_t datamod[DCF77DIAGRAMSIZE*DCF77DATAMINUTES];
	uint8_t updated = 0;
	char buffer[DEBUG_CHARS+1];
	buffer[DEBUG_CHARS] = '\0';
	if (g_dcf77samplestaken <= (DCF77DATAMINUTES * SECONDSINMINUTE - DCF77MINUTEOFFSET)) {
		//not enough data to analyze
		return 0;
	}

	for (int i = 0; i < DCF77DATAMINUTES; i++) {
		for (int j = 0; j < DCF77DIAGRAMSIZE; j++) {
			datamod[i*DCF77DIAGRAMSIZE+j] = d77x(startindex, DCF77SHORTENED + j, (DCF77DATAMINUTES - 1) - i);
		}
	}
	if (g_settings.debugRs232 == 0xB) {
		snprintf_P(buffer, DEBUG_CHARS, PSTR("DCF77 data:\r\n"));
		rs232_sendstring(buffer);
		for (int i = 0; i < DCF77DATAMINUTES; i++) {
			for (int j = 0; j < DCF77DIAGRAMSIZE; j++) {
				buffer[j] = datamod[i*DCF77DIAGRAMSIZE+j] + 'A';
			}
			buffer[DCF77DIAGRAMSIZE] = '\r';
			buffer[DCF77DIAGRAMSIZE+1] = '\n';
			buffer[DCF77DIAGRAMSIZE+2] = '\0';
			rs232_sendstring(buffer);
		}
	}
	clock_highspeed();
	uint8_t result = dcf77_statisticsdecode(datamod, &minute, &hour, &day, &month, &year2digit, &errorrate);
	clock_normalspeed();
	if (g_settings.debugRs232 == 0xB) {
		snprintf_P(buffer, DEBUG_CHARS, PSTR("result: %i error:%i %i:%02i\r\n"), result, errorrate, hour, minute);
		rs232_sendstring(buffer);
		snprintf_P(buffer, DEBUG_CHARS, PSTR("20%02i-%02i-%02i\r\n"), year2digit, month, day);
		rs232_sendstring(buffer);
	}
	if ((result == 2) && (errorrate <= (g_settings.dcf77Level * DCF77DATAMINUTES))) {
		uint32_t utime = timestampFromDate(day-1, month-1, year2digit,
		                 (uint32_t)minute*60 +
		                 (uint32_t)hour*60*60 +
		                 (DCF77DATAMINUTES-1) * SECONDSINMINUTE - 1);
		//second verify, as there are sometimes still wrong times getting through the check (P<0.5%)
		int32_t errorBetweenSync = utime - g_state.time;
		if ((g_state.dcf77Synced == 0) ||
		    ((errorBetweenSync > -60) && (errorBetweenSync < 60)) ||
		     (errorrate <= ((g_settings.dcf77Level *4 / 5) * DCF77DATAMINUTES))) { //if already synced, time delta must be small or extra low error (80% of common one)
			if (g_settings.debugRs232 == 0xB) {
				uint16_t dof = dayofyear(day-1, month-1, year2digit);
				snprintf_P(buffer, DEBUG_CHARS, PSTR("dof=%i utime=%lu\r\n"), dof, (long unsigned)utime);
				rs232_sendstring(buffer);
			}
			loggerlog_synced(utime, errorrate);
			if ((utime > g_state.timeStampLastSync) &&
			    (g_state.dcf77Synced)) {
				if ((errorBetweenSync > -30) && (errorBetweenSync < 30)) {
					uint32_t secondsBetweenSync = utime - g_state.timeStampLastSync;
					if (secondsBetweenSync) {
						int32_t dayDeltaMs = errorBetweenSync * (24L*60L*60L*100L) / (int32_t)secondsBetweenSync; //mul by 1000 would result in a int32 overflow
						dayDeltaMs *= 10;
						if (g_settings.debugRs232 == 0xB) {
							snprintf_P(buffer, DEBUG_CHARS, PSTR("errorSync =%li DeltaLastSync=%lu, Error=%lims/d\r\n"), (long)errorBetweenSync, (unsigned long)secondsBetweenSync, (long)dayDeltaMs);
							rs232_sendstring(buffer);
						}
						dayDeltaMs += g_state.timeLastDelta;
						if ((dayDeltaMs >= TIMECALIB_MIN) && (dayDeltaMs <= TIMECALIB_MAX)) {
							g_state.timeLastDelta = dayDeltaMs;
						}
					}
				}
			}
			g_state.time = utime;
			g_state.timeStampLastSync = utime;
			g_state.timescache = g_state.time % 60;
			g_state.timemcache = (g_state.time / 60) % 60;
			g_state.timehcache = (g_state.time / (60*60)) % 24;
			g_state.dcf77Synced = 1;
			//normally ignore summertime bits, but use it for the one undefined hour in the year
			//bit 17 = 1 for summertime, bit 18 = for winter time.
			uint8_t dcfsayssummertime = d77x(startindex, 17, DCF77DATAMINUTES - 1) > d77x(startindex, 18, DCF77DATAMINUTES - 1) ? 1 : 0;
			g_state.summertime = isSummertime(utime, dcfsayssummertime);
			updated = 1;
			g_state.accumulatedErrorCycles = 0;
			g_state.badCyclesRoundingError = 0;
		}
	}
	return updated;
}

uint8_t dcf77_update(void) {
	if (!g_dcf77enabled) {
		return 0;
	}
	if (g_dcf77sampled < (DCF77SAMPLES*10/9)) { //forces to read every bit only once
		return 0;
	}
	uint8_t i;
	//seek 100ms start window
	//todo: false detection of data as carrier if data at array overflow
	int16_t sumpos = 0;
	int16_t sumneg = 0;
	int16_t summax = 0;
	uint8_t posmax = 0;
	uint8_t updated = 0;
	if (g_dcf77samplestaken < 0xFFFF) {
		g_dcf77samplestaken++;
	}
	for (i = 0; i < DCF77SIGWINDOW; i++) { //prepare floating window
		sumpos += (g_dcf77secondmatrix[i] & 0x1f);
	}
	for (i = (DCF77SAMPLES - DCF77SIGWINDOW); i < (DCF77SAMPLES); i++) { //prepare floating window
		sumneg += (g_dcf77secondmatrix[i] & 0x1f);
	}
	if (sumpos > sumneg) { //index = 0, init case
		posmax = sumpos - sumneg;
	}
	for (i = 0; i < (DCF77SAMPLES-1); i++) { //index 1...99 case
		uint16_t indexposs = i + DCF77SIGWINDOW;
		uint16_t indexpose = i;
		uint16_t indexnege = i - DCF77SIGWINDOW;
		if (indexposs >= DCF77SAMPLES) { //pos index does an overflow to high
			indexposs -= DCF77SAMPLES;
		}
		if (indexnege >= DCF77SAMPLES) { //neg does an overflow to low
			indexnege = (DCF77SAMPLES - DCF77SIGWINDOW) + i ;
		}
		// uint16_t indexnegs = indexpose;
		sumpos += (g_dcf77secondmatrix[indexposs] & 0x1f);
		uint8_t indexv = (g_dcf77secondmatrix[indexpose] & 0x1f);
		sumpos -= indexv;
		sumneg += indexv;
		sumneg -= (g_dcf77secondmatrix[indexnege] & 0x1f);
		int16_t sum;
		if (sumpos > sumneg) {
			sum = sumpos - sumneg;
		} else {
			sum = 0;
		}
		if (sum > summax) {
			summax = sum;
			posmax = i + 1; //as we substracted [i] and add to negative, it woul be the next for which the sum fits
		}
	}
	//abort evaluate if in the middle of a bit sample -> come back later
	uint8_t secondindex = g_dcf77secondindex;
	if (((secondindex >= posmax) && (secondindex <= (posmax + DCF77SIGWINDOW*2)))
	    || ((posmax > (DCF77SAMPLES - DCF77SIGWINDOW*2)) && (secondindex < (posmax - (DCF77SAMPLES - DCF77SIGWINDOW*2))))) {
		g_dcf77sampled -= DCF77SIGWINDOW; //delay sampling to a later time
		return 0;
	}
	g_dcf77sampled -= DCF77SAMPLES; //consume a full second of samples
	//adjust periode if posmaxlast drifts
	int8_t posdelta = posmax - g_dcf77posmaxlast;
	if ((posmax < DCF77SIGWINDOW) && (g_dcf77posmaxlast > (DCF77SAMPLES - DCF77SIGWINDOW))) {
		posdelta = (posmax + DCF77SAMPLES) - g_dcf77posmaxlast;
	}
	if ((g_dcf77posmaxlast < DCF77SIGWINDOW) && (posmax > (DCF77SAMPLES - DCF77SIGWINDOW))) {
		posdelta = posmax - (g_dcf77posmaxlast + DCF77SAMPLES);
	}
	if (posdelta != 0) {
		int16_t proportion = TCD0.PER/(DCF77SAMPLES*4); //increment-decrement by 0.25 position at maximum
		proportion /= (g_dcf77nodeltacounter+1);
		if (proportion == 0) {
			proportion = 1;
		}
		if (posdelta < 0) { //timer too slow, dcf77 is faster
			proportion *= -1;
		}
		uint16_t newval = TCD0.PER + proportion;
		if (newval < DCF77PERMIN) {
			newval = DCF77PERMIN;
		}
		if (newval > DCF77PERMAX) {
			newval = DCF77PERMAX;
		}
		if (g_settings.debugRs232 == 0xB) {
			char buffer[DEBUG_CHARS+1];
			buffer[DEBUG_CHARS] = '\0';
			snprintf_P(buffer, DEBUG_CHARS, PSTR("adj %u>%u\r\n"), TCD0.PER, newval);
			rs232_sendstring(buffer);
		}
		TCD0.PERBUF = newval;
		g_dcf77nodeltacounter = 0;
	} else {
		if (g_dcf77nodeltacounter < 255) {
			g_dcf77nodeltacounter++;
		}
	}
	g_dcf77posmaxlast = posmax;
	//build up history buffer
	uint8_t positive = 0;
	for (i = 0; i < DCF77SIGWINDOW; i++) {
		uint16_t index = i + posmax + DCF77SIGWINDOW;
		if (index >= DCF77SAMPLES) {
			index -= DCF77SAMPLES;
		}
		if (g_dcf77secondmatrix[index] & 0x80) {
			positive++;
		}
	}
	g_dcf77signalhistory[g_dcf77signalindex] = (g_dcf77signalhistory[g_dcf77signalindex] << 4) | positive;
	//detect minute start
	uint8_t carriersignal = 0;
	for (i = 0; i < DCF77SIGWINDOW; i++) {
		uint8_t index = i + posmax;
		if (index >= DCF77SAMPLES) {
			index -= DCF77SAMPLES;
		}
		if (g_dcf77secondmatrix[index] & 0x80) {
			carriersignal++;
		}
	}
	g_dcf77signalstartevidence[g_dcf77signalindex] = (g_dcf77signalstartevidence[g_dcf77signalindex] << 4) | carriersignal;
	//find strongest starting minute signal
	uint8_t lowestcarrier = 0xFF;
	uint8_t lowestcarrierindex = 0;
	for (i = 0; i < DCF77SECONDS; i++) {
		DCF77DATA temp = g_dcf77signalstartevidence[i];
		uint8_t carrier = ((temp & 0xF000) >> 12) + ((temp & 0xF00) >> 8) +
		                  ((temp & 0xF0) >> 4) + (temp & 0xF);
#if DCF77HISTORY > 4
		carrier |= ((temp & 0xF0000000) >> 28) + ((temp & 0xF000000) >> 24) +
		            ((temp & 0xF00000) >> 20) + ((temp & 0xF0000) >> 16);
#endif
		if (carrier < lowestcarrier) {
			lowestcarrier = carrier;
			lowestcarrierindex = i;
		}
	}
	if ((lowestcarrierindex == g_dcf77signalindex) && (summax > ((DCF77SIGWINDOW*9)))) {
		updated = dcf77_analyze(lowestcarrierindex);
	}
	//calculate quality
	uint16_t delta = 0; //worst could be 5*59 = 0% signal
	for (i = 0; i < DCF77SECONDS; i++) {
		if (i != lowestcarrierindex) {
			if ((g_dcf77signalstartevidence[i] & 0xF) > (DCF77SIGWINDOW/2)) {
				uint8_t sigbit = g_dcf77signalhistory[i] & 0xF;
				if (sigbit < (DCF77SIGWINDOW/2)) {
					delta += sigbit; //difference from perfect zero
				} else {
					delta += DCF77SIGWINDOW - sigbit; //difference from perfect one
				}
			} else {
				delta += DCF77SIGWINDOW/2; //add maximum error if carrier is too weak
			}
		}
	}
	g_dcf77signalquality = 100 - (100*delta)/((DCF77SECONDS-1)*(DCF77SIGWINDOW/2));
	//inc index for seconds
	g_dcf77signalindex++;
	if (g_dcf77signalindex >= DCF77SECONDS) {
		g_dcf77signalindex = 0;
	}
	//debug
	if (g_settings.debugRs232 == 5) {
		for (i = 0; i < DCF77SAMPLES; i++) {
			rs232_puthex(g_dcf77secondmatrix[i]);
		}
		rs232_putchar('\n');
		rs232_putchar('\r');
		for (i = 0; i < posmax; i++) {
			rs232_sendstring("  ");
		}
		rs232_sendstring("# ");
	}
#ifdef DCF77DEBUGBITSIGNAL
	{
		char buffer[DEBUG_CHARS+1];
		snprintf_P(buffer, DEBUG_CHARS, PSTR("pos %u, max: %u carr: %u sig: %u\r\n"), (uint16_t)posmax, summax, (uint16_t)carriersignal, (uint16_t)positive);
		rs232_sendstring(buffer);
	}
#endif
	return updated;
}
