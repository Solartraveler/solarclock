#ifndef MAKEDATA_H
#define MAKEDATA_H

#include <stdint.h>

//can be variable
#ifndef UNIT_TEST
#define DCF77DATAMINUTES 30
#define SIGNALOFF 0
#define SIGNAL0 7
#define SIGNAL1 15

#else

#include "../dcf77statisticsdecode.h"

#define SIGNALOFF DCF77SIGNALOFF
#define SIGNAL0 DCF77SIGNAL0
#define SIGNAL1 DCF77SIGNAL1
#endif

void makedataall(uint8_t * datafiled, float errorrate, uint16_t year, uint8_t month, uint8_t day, uint8_t hour, uint8_t minute);

#endif