#ifndef DEBUG_H
#define DEBUG_H

#define DEBrc5Init 2
#define DEBrc5Stop 4
#define DEBrun1xS 10
#define DEBrun2xS 20
#define DEBrun4xS 40
#define DEBrun8xS 80

#define DEBupdate_rfm12control 200
#define DEBrfm12_init 202
#define DEBrfm12_standby 204
#define DEBrfm12_update 206
#define DEBrfm12_timer 208


#define FUNCDEBUG

#ifdef FUNCDEBUG

extern uint8_t g_debug; //number identifying the last called or left function

#include "config.h"

#define DEBUG_FUNC_ENTER(X) g_debug = DEB ## X ;
#define DEBUG_FUNC_LEAVE(X) g_debug = DEB ## X +1;

#else

#define DEBUG_FUNC_ENTER(X)
#define DEBUG_FUNC_LEAVE(X)

#endif


#endif
