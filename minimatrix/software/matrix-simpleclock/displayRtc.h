#ifndef DISPLAYRTC_H
#define DISPLAYRTC_H

#define F_RTC 32768

extern volatile uint8_t rtc_8thcounter;

uint8_t disp_rtc_setup(void);

void disp_configure_set(uint8_t brightness, uint16_t refreshrate);

void rtc_waitsafeoff(void);

#endif
