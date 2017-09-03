#ifndef PCCOMPAT_H
#define PCCOMPAT_H

#define DEBUG_CHARS 128
#define PSTR(X) X
#define rs232_sendstring printf
#define rs232_sendstring_P printf
#define snprintf_P snprintf
#define DbgPrintf_P printf
#define rs232_puthex(X) printf("%02x", X)
#define rs232_putchar(X) printf("%c", X)
#define pgm_read_byte(X) (*(X))

#define F_CPU 2000000

void menu_keypress(uint16_t key);

void loggerlog_batreport(void);

#endif