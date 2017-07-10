#ifndef DISPLAYRTC_H
#define DISPLAYRTC_H

#define F_RTC 32768

#define DISP_RTC_PER ((F_RTC/8)-1)

extern volatile uint8_t rtc_8thcounter;

uint8_t disp_rtc_setup(void);

void disp_configure_set(uint8_t brightness, uint16_t refreshrate);

void rtc_waitsafeoff(void);


/*+1 count slower, -1 count faster, 0 count as expected
This is relative to one tick of the crystal and DISP_RTC_PER
Example: Setting direction to +1 for one minute,
and then call to set back to 0, results in:
F_RTC / DISP_RTC_PER * 60 = extra F_RTC cycles slower
4096*60=480 rtc clock cycles extra
since every cycle is 1/32768sec -> Counting one minute takes 14.6ms longer.
*/
void rtc_finecalib(int8_t direction);

#endif
