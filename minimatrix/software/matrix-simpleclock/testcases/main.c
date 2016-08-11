#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>

#include "../config.h"

#include "testcharger.h"
#include "testdcf77decoder.h"

settings_t g_settings;
sysstate_t g_state;

int g_lastaction;

void menu_keypress(uint16_t key) {
	g_lastaction = key;
}

int main(void) {
	int errors = 0;
	errors += testcharger();
	errors += testdcf77decoder();
	if (errors) {
		printf("Errors: %i\n", errors);
		return 1;
	}
	printf("No errors :)\n");
	return 0;
}
