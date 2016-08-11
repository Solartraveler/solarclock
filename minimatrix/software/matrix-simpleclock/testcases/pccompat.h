#ifndef PCCOMPAT_H
#define PCCOMPAT_H

#define DEBUG_CHARS 128
#define PSTR(X) X
#define rs232_sendstring printf
#define snprintf_P snprintf

void menu_keypress(uint16_t key);

#endif