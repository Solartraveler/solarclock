#ifndef CLOCKS_H
#define CLOCKS_H

void calibrate_start(void);
int16_t calibrate_update(void);

//switches to 16MHz with the PLL from the internal RC oscillator
void clock_highspeed(void);
//goes back to the 2MHz
void clock_normalspeed(void);



#endif
