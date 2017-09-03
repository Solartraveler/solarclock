/* Matrix-Advancedclock
  (c) 2017 by Malte Marwedel
  www.marwedels.de/malte

  This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*/
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "debug.h"
#include "rs232.h"
#include "config.h"

#define SAFERESERVE 8

//Good source of information: http://www.nongnu.org/avr-libc/user-manual/malloc.html

extern char *__brkval;
extern char __data_start[];

void stackCheckInit(void) {
	//fills the buffer between the heap end and the stack start with 0xEE
	uint8_t stack;
	uint8_t * heap = (uint8_t*)__brkval;
	if (!heap) {
		heap = (uint8_t*) __malloc_heap_start;
	}
	g_state.memMaxHeap = heap;
	while ((heap < &stack - SAFERESERVE)) {
		*heap = 0xEE;
		heap++;
	}
	g_state.memMinStack = &stack - SAFERESERVE;
}

void stackCheck(void) {
	uint8_t * heap = (uint8_t*)__brkval;
	if (!heap) {
		heap = (uint8_t*) __malloc_heap_start;
	}
	if (g_state.memMaxHeap < (void*)heap) {
		g_state.memMaxHeap = heap;
	}
	uint8_t * stackStart = heap + 1;
	while ((*stackStart == 0xEE) && (stackStart < (uint8_t*)RAMEND)) {
		stackStart++;
	}
	if (g_state.memMinStack > (void*)stackStart) {
		g_state.memMinStack = stackStart;
	}
	if (g_settings.debugRs232 == 0xF) {
		uint16_t fixed = (uint16_t)g_state.memMaxHeap - (uint16_t)__data_start; //internal SRAM offset
		//RAMEND is the last valid address (0x??FFF) - not a size. So add 1 to get a proper difference to first valid address.
		uint16_t stack = (RAMEND+1) - (uint16_t)g_state.memMinStack;
		uint16_t free = (uint16_t)g_state.memMinStack - (uint16_t)g_state.memMaxHeap;
		DbgPrintf_P(PSTR("Static+Heap=%iB, maxStack=%iB, minFree=%iB\r\n"), fixed, stack, free);
	}
}
