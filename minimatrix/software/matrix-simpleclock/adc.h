#ifndef ADC_H
#define ADC_H

uint16_t adca_get2(uint8_t pin, uint8_t reference);
int16_t adca_getDelta(uint8_t pinPos, uint8_t pinNeg, uint8_t reference, uint8_t gain);
uint16_t adca_get(uint8_t pin);
void adca_getQuad(uint8_t pin0, uint8_t pin1, uint8_t pin2, uint8_t pin3, uint8_t reference, uint16_t * result);
uint16_t adcb_get(uint8_t pin);

void adca_startup(void);
void adcb_startup(void);
void adca_stop(void);
void adcb_stop(void);

#endif
