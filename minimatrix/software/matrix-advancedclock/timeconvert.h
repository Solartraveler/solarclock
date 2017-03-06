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

#endif
