#ifndef PINTEST_H
#define PINTEST_H

#define PINTEST_NUMPINS (9*8+4)
#define PINTEST_NUMPORTS ((PINTEST_NUMPINS + 7)/8)

uint8_t pintest_runtest(void);


#endif