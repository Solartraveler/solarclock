
#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>

#include "makedata.h"

uint8_t calcleapyear(int year) {
	//Wenn Jahr durch 4 ohne Rest teilbar, aber nicht durch 100
	uint8_t isschaltjahr;
	if (((year % 4) == 0)&&((year % 100) != 0)) {
  	isschaltjahr = 1;        //Dann Schaltjahr
	} else {
		isschaltjahr = 0;        //Dann kein Schaltjahr
	}
	if ((year % 400) == 0) {   //Wenn Jahr durch 400 Teilbar
		isschaltjahr = 1;        //Dann doch wieder Schaltjahr
	}
	return isschaltjahr;
}

uint8_t calcweekday(int day, int month, int year) {
	uint16_t tagdesjahres;
	uint8_t jahrdiff;
	uint16_t tempwochentag;
	//Tag des Jahres berechnen
	uint8_t isschaltjahr = calcleapyear(year);//Dazu muss bekannt sein, ob es sich um ein Schaltjahr handelt
	tagdesjahres = 31*(month-1);
	if (month > 2) {
		tagdesjahres = tagdesjahres -3 +isschaltjahr;
	}
	if (month > 4) { tagdesjahres--; }
	if (month > 6) { tagdesjahres--; }
	if (month > 9) { tagdesjahres--; }
	if (month > 11) { tagdesjahres--; }
	tagdesjahres += day;
	//Wochentag berechnen
	jahrdiff = year -2000; //Das Referenzjahr muss durch 400 ohne Rest teilbar sein.
	tempwochentag = jahrdiff + (jahrdiff-1) / 4 - (jahrdiff-1) / 100 + (jahrdiff-1)
	                / 400 + tagdesjahres;
	if (jahrdiff > 0) { //Weil das Referenzjahr auch ein Schaljahr ist
		tempwochentag++;
	}
	//Normalerweise wäre Samstag=1 (Weil der 1.1.2000 ein Samstag ist), so ist Sa.=6
	tempwochentag = (tempwochentag % 7)+5;
	if (tempwochentag > 7) { //�berlauf
		tempwochentag = tempwochentag -7;
	}
	return tempwochentag;
}

//usually never change
#define SECONDSINMINUTE 60
#define MINUTESINHOUR 60
#define HOURSINDAY 24
#define MONTHSINYEAR 12

const uint8_t signal[2] = {SIGNAL0, SIGNAL1};

const uint8_t daysInMonth[MONTHSINYEAR] = {31, 29, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};

uint8_t calcparity(uint8_t * data, uint8_t elem) {
	uint8_t parity = 0;
	for (int i = 0; i < elem; i++) {
		if (data[i] == SIGNAL1) {
			parity = 1 - parity;
		}
	}
	return parity;
}

void makedata(uint8_t * data, float errorrate, uint8_t day, uint8_t month, uint16_t year, uint8_t dayofweek, uint8_t hour, uint8_t minute) {
	//start
	data[0] = SIGNAL0;
	//metrotime
	for (int i = 1; i <= 14; i++) {
		data[i] = SIGNAL0;
	}
	//special data
	for (int i = 15; i <= 19; i++) {
		data[i] = SIGNAL0;
	}
	//always one (begin of time information)
	data[20] = SIGNAL1;
	//minute
	uint8_t tmin = minute % 10;
	for (int i = 21; i <= 24; i++) {
		data[i] = signal[tmin & 1];
		tmin >>= 1;
	}
	tmin = minute / 10;
	for (int i = 25; i <= 27; i++) {
		data[i] = signal[tmin & 1];
		tmin >>= 1;
	}
	//minute parity
	data[28] = signal[calcparity(data + 21, 7)];
	//hour
	uint8_t thour = hour % 10;
	for (int i = 29; i <= 32; i++) {
		data[i] = signal[thour & 1];
		thour >>= 1;
	}
	thour = hour / 10;
	for (int i = 33; i <= 34; i++) {
		data[i] = signal[thour & 1];
		thour >>= 1;
	}
	//hour parity
	data[35] = signal[calcparity(data + 29, 6)];
	//day
	uint8_t tday = day % 10;
	for (int i = 36; i <= 39; i++) {
		data[i] = signal[tday & 1];
		tday >>= 1;
	}
	tday = day / 10;
	for (int i = 40; i <= 41; i++) {
		data[i] = signal[tday & 1];
		tday >>= 1;
	}
	//day of week
	for (int i = 42; i <= 44; i++) {
		data[i] = signal[dayofweek & 1];
		dayofweek >>= 1;
	}
	//month
	uint8_t tmonth = month % 10;
	for (int i = 45; i <= 48; i++) {
		data[i] = signal[tmonth & 1];
		tmonth >>= 1;
	}
	tmonth = month / 10;
	for (int i = 49; i <= 49; i++) {
		data[i] = signal[tmonth & 1];
		tmonth >>= 1;
	}
	//year
	year = year % 100; //cut of two most significat digits
	uint8_t tyear = year % 10;
	for (int i = 50; i <= 53; i++) {
		data[i] = signal[tyear & 1];
		tyear >>= 1;
	}
	tyear = year / 10;
	for (int i = 54; i <= 57; i++) {
		data[i] = signal[tyear & 1];
		tyear >>= 1;
	}
	//date parity
	data[58] = signal[calcparity(data + 36, 22)];
	//minute detector
	data[59] = SIGNALOFF;
	//add the noise
	for (int i = 0; i < SECONDSINMINUTE; i++) {
		int noise = 0;
		float hasnoiselevel = 100 - (errorrate * 100);
		float hasnoise = rand() % 100;
		if (hasnoise > hasnoiselevel) {
			noise = (rand() % (SIGNAL1*2)) - SIGNAL1;
		}
		if ((int)data[i] + noise < 0) {
			data[i] = SIGNALOFF;
		} else {
			data[i] += noise;
			if (data[i] > SIGNAL1) {
				data[i] = SIGNAL1;
			}
		}
	}
}

void makedataall(uint8_t * datafiled, float errorrate, uint16_t year, uint8_t month, uint8_t day, uint8_t hour, uint8_t minute) {
	//calculateWeekday
	uint8_t dayofweek = calcweekday(day, month, year);
#ifndef UNIT_TEST
	printf("Day in week: %i (1:Mon, 7:Sun)\n", dayofweek);
#endif
	//generateData
	memset(datafiled, 0, SECONDSINMINUTE*DCF77DATAMINUTES);
	for (int i = 0; i < DCF77DATAMINUTES; i++) {
		makedata(datafiled + i*SECONDSINMINUTE, errorrate, day, month, year, dayofweek, hour, minute);
		//calc next minute
		minute++;
		if (minute == MINUTESINHOUR) {
			minute = 0;
			hour++;
		}
		if (hour == HOURSINDAY) {
			hour = 0;
			day++;
			dayofweek++;
		}
		if (dayofweek == 8) {
			dayofweek = 1;
		}
		if ((day > daysInMonth[month-1]) || ((month == 2) && (day > 28) && calcleapyear(year))) {
			day = 1;
			month++;
		}
		if (month > MONTHSINYEAR) {
			month = 1;
			year++;
		}
	}
}

#ifndef UNIT_TEST

int main(int argc, char ** argv) {
	if (argc != 4) {
		printf("Give ERRORRATE TIME DATE. Ex: 0.1 12:23 1.2.2044\n");
		return 1;
	}
	srand(42);
	unsigned int hour, minute, day, month, year;
	float errorrate;
	sscanf(argv[1], "%f", &errorrate);
	sscanf(argv[2], "%i:%i", &hour, &minute);
	sscanf(argv[3], "%i.%i.%i", &day, &month, &year);
	//sanity input
	if ((minute >= MINUTESINHOUR) || (hour >= HOURSINDAY) ||
	    (month > MONTHSINYEAR) || (month < 1) || (day < 1) ||
	    (day > daysInMonth[month-1])) {
		printf("Error: Invalid input\n");
		return 1;
	}
	//generate filename
	char filename[128];
	sprintf(filename,"dcf77data_%04i-%02i-%02i_%02i-%02i.txt", year, month, day, hour, minute);
	//make data
	uint8_t datafiled[SECONDSINMINUTE*DATAMINUTES];
	makedataall(datafiled, errorrate, day, month, year, hour, minute);
	//make data human readable
	for (int i = 0; i < SECONDSINMINUTE*DATAMINUTES; i++) {
		datafiled[i] += 'A';
	}
	//write to file
	FILE * f = fopen(filename, "wb");
	if (f) {
		if (fwrite(datafiled, SECONDSINMINUTE*DATAMINUTES, 1, f) != 1) {
			printf("Write to file failed\n");
			fclose(f);
			return 1;
		}
		fclose(f);
	} else {
		printf("Create file %s failed\n", filename);
		return 1;
	}
	return 0;
}

#endif
