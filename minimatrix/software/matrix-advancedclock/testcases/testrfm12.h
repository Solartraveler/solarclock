#ifndef TESTRFM12_H
#define TESTRFM12_H

#include <stdint.h>

int testrfm12(void);


//compatibility layer

//fake values:
#define TC_CLKSEL_DIV1_gc 1
#define TC_CLKSEL_OFF_gc 0
#define TC_OVFINTLVL_LO_gc 1
#define PR_TC1_bm 1
#define PMIC_LOLVLEN_bm 1;

#ifndef UNREFERENCED_PARAMETER
#define UNREFERENCED_PARAMETER(P) (void)(P)
#endif

typedef struct {
	uint8_t CTRLA;
	uint8_t CTRLB;
	uint8_t CTRLC;
	uint8_t CTRLD;
	uint8_t CTRLE;
	uint16_t PER;
	uint16_t CNT;
	uint8_t INTCTRLA;
	} timerfake_t;

typedef struct {
	uint8_t CTRL;
} pmicfake_t;

timerfake_t TCE1;
pmicfake_t PMIC;
uint8_t PR_PRPE;


uint16_t rfm12_command(uint16_t outdata);
uint8_t rfm12_ready(void);
uint8_t rfm12_irqstatus(void);
void rfm12_fireint(void);
void rfm12_reset(void);
uint8_t _crc_ibutton_update(uint8_t crc, uint8_t data);
void waitms(uint16_t x);
static inline void rfm12_portinit(void) { };
static inline void _MemoryBarrier(void) { };
static inline void rfm12_standby(void) { };
static inline void cli(void) { };
static inline void sei(void) { };

#endif
