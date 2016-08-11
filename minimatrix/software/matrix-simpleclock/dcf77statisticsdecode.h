#ifndef DCF77STATISTICSDECODE_H
#define DCF77STATISTICSDECODE_H

//usually never change
#define SECONDSINMINUTE 60
#define MINUTESINHOUR 60
#define HOURSINDAY 24
#define MAXDAYSINMONTH 31
#define MONTHSINYEAR 12
#define TWODIGITYEARS 100

//change according to your sampling method
#define DCF77SIGNALOFF 0
#define DCF77SIGNAL0 10
#define DCF77SIGNAL1 20
#define DCF77DATAMINUTES 6

//set for a RAM saving input into the decoding function
#define DCF77DATASHORTFORMAT

//generated constants
#define DCF77SIGNALDELTA (DCF77SIGNAL1-DCF77SIGNAL0)
//there are four bits of error detection/correction information for the date decode
#define DCF77MAXERRORDATE ((DCF77SIGNAL1)*DCF77DATAMINUTES*4)
#define DCF77SIGNALMERGED0 (DCF77SIGNAL0*DCF77DATAMINUTES)
#define DCF77SIGNALMERGEDDELTA (DCF77SIGNALDELTA*DCF77DATAMINUTES)

#if ((DCF77SIGNAL1*DCF77DATAMINUTES) > 255)
#warning change datadelta to uint16_t
#endif

//common signal starts in DCF77 signal.

#define DCF77MINUTEOFFSET 21
#define DCF77MINUTESIZE 8
#define DCF77HOUROFFSET 29
#define DCF77HOURSIZE 7
#define DCF77DAYOFFSET 36
#define DCF77DAYSIZE 6
#define DCF77MONTHOFFSET 45
#define DCF77MONTHSIZE 5
#define DCF77YEAROFFSET 50
#define DCF77YEARSIZE 8
#define DCF77DAYOFWEEKOFFSET 42
#define DCF77DAYOFWEEKSIZE 3
#define DCF77DATEPARITYOFFSET 58


#ifdef DCF77DATASHORTFORMAT

#define DCF77SHORTENED 21
#define DCF77SHORTEND 1


#else

#define DCF77SHORTENED 0
#define DCF77SHORTEND 0

#endif

//data position in uint8_t* data array:

#define DCF77MINUTEDATA (DCF77MINUTEOFFSET - DCF77SHORTENED)
#define DCF77HOURDATA (DCF77HOUROFFSET - DCF77SHORTENED)
#define DCF77DAYDATA (DCF77DAYOFFSET - DCF77SHORTENED)
#define DCF77MONTHDATA (DCF77MONTHOFFSET - DCF77SHORTENED)
#define DCF77YEARDATA (DCF77YEAROFFSET - DCF77SHORTENED)
#define DCF77DAYOFWEEKDATA (DCF77DAYOFWEEKOFFSET - DCF77SHORTENED)
#define DCF77DATEPARITYDATA (DCF77DATEPARITYOFFSET - DCF77SHORTENED)

#define DCF77DIAGRAMSIZE (SECONDSINMINUTE - DCF77SHORTENED - DCF77SHORTEND)

/*returns 1 if decoding of time succeed.
          2 if time + date got decoded.
          0 if data are too bad to decode
 All pointers must be filled. No NULL check is performed for performance reason.
 data *must* have DCF77DIAGRAMSIZE * MINUTESINHOUR entries
 If DCF77DATASHORTFORMAT is NOT set:
 The input is given as the common DCF77 signal. 1byte for every second ->
 60 bytes for every minute (one DCF77 diagram).
 If DCF77DATASHORTFORMAT is given, the first 21 bits
 and the last bit are removed, reducing the required memory for the diagram
 down to 38bytes. The removed parts only contain status information, metrotime
 information and the minute end detection.
 errorrate gives the number of times where the dcf77 input signal did not match
 the best time + date.
 Iif only time was decoded, only the error of the time signal is returned,
 therefore the error value would be lower without a better signal.
*/
 uint8_t dcf77_statisticsdecode(const uint8_t * data, uint8_t * minute, uint8_t * hour,
                               uint8_t *day, uint8_t * month, uint8_t * year2digit,
                               uint16_t * errorrate);


#endif
