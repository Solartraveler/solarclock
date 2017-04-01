#ifndef TIMECONVERT_H
#define TIMECONVERT_H

/*
Note first day and first month start with 0, not 1.
First day returned will be 0, not 1.
*/
uint16_t dayofyear(uint8_t day, uint8_t month, uint16_t year);


void monthdayfromdayinyear(uint16_t dayinyear, uint16_t year, uint8_t * month, uint8_t * day);

uint32_t secondssince2000(uint8_t year2digits);
uint8_t yearsince2000(uint32_t seconds, uint16_t * dofy);

uint8_t calcweekdayfromtimestamp(uint32_t timestamp2000);

uint8_t isSummertime(uint32_t timestamp2000, uint8_t wasSummertime);

uint8_t daysInMonth(uint8_t month, uint8_t year2digit);

/* Calculates the timestamp since 1.1.2000 until 31.12.2099. Give year as two digits,
day and month must be given beginning with 0 for first day/month.*/
uint32_t timestampFromDate(uint8_t day, uint8_t month, uint8_t year2digit, uint32_t offset);

/*
Breaks the timestamp time up into day (0...30), month (0...11), year (0...99)
Returns the remaining timestamp of the day (hour,minutes,seconds)
*/
uint32_t dateFromTimestamp(uint32_t timestamp2000, uint8_t * day, uint8_t * month, uint8_t * year2digit, uint16_t * dayOfYear);

#endif
