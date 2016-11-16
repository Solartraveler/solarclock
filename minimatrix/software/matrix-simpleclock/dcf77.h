#ifndef DCF77_H
#define DCF77_H


void dcf77_enable(int16_t freqdelta);
void dcf77_disable(void);
uint8_t dcf77_is_enabled(void);
uint8_t dcf77_is_idle(void);


void dcf77_getstatus(char * targetstring);

//call this function at least every second
//returns 1 if update occured, 0 if no update or disabled
uint8_t dcf77_update(void);

#endif
