#ifndef SOUND_H
#define SOUND_H

/*
volume: 255=maximum, 0=minium
frequency: [Hz]
duration: [ms] for the beep until it goes off.
*/
void sound_beep(uint8_t volume, uint16_t frequency, uint16_t duration);

#endif