/* Mouse-PC-Demo for MenuInterpreter
  Version 1.0
  (c) 2010, 2012, 2015 by Malte Marwedel
  Modified for Matrix-Simpleclock
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

#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <GL/glut.h>

//only for scroll wheel support:
#include <GL/freeglut.h>

#include "../gui.h"
#include "../menu-interpreter.h"
#include "../config.h"

settings_t g_settings; //permanent settings
sysstate_t g_state; //floating state of main program

extern unsigned char menu_focus_key_next;
extern unsigned char menu_focus_key_prev;
extern unsigned char menu_key_enter;

int redraw_ms = 1000/8; //redraw time in miliseconds (time of AVR cycle)

uint16_t dispx = 800, dispy = 600;

unsigned char screen[MENU_SCREEN_Y][MENU_SCREEN_X/8+1];

/*
0: left click
1: middle click
2: right click
3: scrollup
4: scrolldown
5: left
6: right
7: up
8: down
*/
unsigned char usekeys[9] = {0, 0, 0, 3, 2, 1, 4, 3, 2};


/* Functions for the lib */

void menu_screen_set(SCREENPOS x, SCREENPOS y, unsigned char color) {
	if ((x < MENU_SCREEN_X) && (y < MENU_SCREEN_Y)) {
		unsigned char b = screen[y][x/8];
		if (color & 1) {
			b |= (1<< (x % 8));
		} else
			b &= ~(1 << (x % 8));
		screen[y][x/8] = b;
	} else {
#ifdef DEBUG
		printf("Warning: Drawing %i, %i is out of bounds\n", x, y);
#endif
	}
}

void menu_screen_flush(void) {
	uint16_t i, j;
	glClear(GL_COLOR_BUFFER_BIT);
	for (i = 0; i < MENU_SCREEN_Y; i++) {
		j = 0;
		while (j < MENU_SCREEN_X) {
			unsigned char b = screen[i][j/8];
			unsigned char k;
			for (k = 0; k < 8; k++) {
				if (b & (1<<k)) { //set pixel
					glPushMatrix();
					glColor3f(0.9,0.9,0.9);
					float xscale = 1.0/(float)MENU_SCREEN_X;
					float yscale = 1.0/(float)MENU_SCREEN_Y;
					glTranslatef((xscale*(float)j*2)-1+xscale, yscale*(float)i*(-2)+1-yscale ,0.0);
					glutSolidSphere(xscale, 10, 5);
					glTranslatef(0.0, yscale*0.2 ,0.0);
					glutSolidSphere(xscale, 10, 5);
					glTranslatef(0.0, yscale*0.2 ,0.0);
					glutSolidSphere(xscale, 10, 5);
					glTranslatef(0.0, -yscale*0.8 ,0.0);
					glutSolidSphere(xscale, 10, 5);
					glTranslatef(0.0, yscale*0.2 ,0.0);
					glutSolidSphere(xscale, 10, 5);

					glPopMatrix();
				} /*else {
					glPushMatrix();
					glColor3f(0.0,0.0,0.0);
					float xscale = 1.0/(float)MENU_SCREEN_X;
					float yscale = 1.0/(float)MENU_SCREEN_Y;
					glTranslatef((xscale*(float)j*2)-1+xscale, yscale*(float)i*(-2)+1-yscale ,0.0);
					glutSolidSphere(xscale, 10, 5);
					glPopMatrix();
				}*/
				j++;
				if (j >= MENU_SCREEN_X)
					break;
			}
		}
	}
	glutSwapBuffers();
	glFlush();
#ifdef MENU_TEST_PROG
	//simple way to guess a useful mapping for each screen
	if (menu_key_enter > 0) {
		usekeys[0] = usekeys[6] = menu_key_enter;
	} else
		usekeys[0] = usekeys[6] = 1;
	if (menu_focus_key_next > 0) {
		usekeys[8] = menu_focus_key_next;
	} else
		usekeys[8] = 4;
	if (menu_focus_key_prev > 0) {
		usekeys[7] = menu_focus_key_prev;
	} else
		usekeys[7] = 3;
#endif
}

void menu_screen_clear(void) {
	int i, j;
	for (i = 0; i < MENU_SCREEN_Y; i++) {
		for (j = 0; j < (MENU_SCREEN_X/8+1); j++) {
			screen[i][j] = 0;
		}
	}
}

void dcf77_getstatus(char* text) {
	sprintf(text, "On 0%%");
}

static void animategfx(void) {
#ifdef MENU_USE_MULTIGFX
	int i;
	for (i = 0; i < MENU_MULTIGFX_MAX; i++) {
		//printf("Increment multi image %i from index %i\r\n", i, menu_gfxindexstate[i]);
		menu_gfxindexstate[i]++;
	}
#endif
}

static void update_window_size(int width, int height) {
	if ((dispx != width) || (dispy != height)) {
		printf("Old size: %i %i, new size: %ix%i, RESIZING WINDOW DOES NOT WORK AS YOU MAY EXPECT\n", dispx, dispy, width, height);
		dispx = width;
		dispy = height;
		menu_screen_flush();
	}
}

void input_mouse_key(int button, int state, int x, int y) {
	if (state == GLUT_UP) {
		float px = (float)x/(float)dispx*(float)MENU_SCREEN_X;
		float py = (float)y/(float)dispy*(float)MENU_SCREEN_Y;
		printf("Fire mouse at position %i %i button %i\n", (int)px, (int)py, button);
		if (button == GLUT_LEFT_BUTTON) {
			menu_mouse((int)px, (int)py, usekeys[0]);
		}
		if (button == GLUT_MIDDLE_BUTTON) {
			menu_mouse((int)px, (int)py, usekeys[1]);
		}
		if (button == GLUT_RIGHT_BUTTON) {
			menu_mouse((int)px, (int)py, usekeys[2]);
		}
		if (button == 3) {
			menu_mouse((int)px, (int)py, usekeys[3]);
		}
		if (button == 4) {
			menu_mouse((int)px, (int)py, usekeys[4]);
		}
	}
}

void input_mouse_move(int x, int y) {
	static char positions[100];
	static int opx = 0, opy = 0;
	float px = (float)x/(float)dispx*(float)MENU_SCREEN_X;
	float py = (float)y/(float)dispy*(float)MENU_SCREEN_Y;
	if ((opx != (int) px) || (opy != (int)py)) {
		opx = (int)px;
		opy = (int)py;
		sprintf(positions, "%i,%i", opx, opy);
#ifdef MENU_TEXT_MICEPOS
		menu_strings[MENU_TEXT_MICEPOS] = (unsigned char *)positions;
		//menu_redraw(); may be time consuming
#endif
	}
}

void input_key_special(int key, int x, int y) {
	if (key == GLUT_KEY_LEFT) {
		printf("Call menu_keypress(%i)\n", usekeys[5]);
		menu_keypress(usekeys[5]);
	}
	if (key == GLUT_KEY_RIGHT) {
		printf("Call menu_keypress(%i)\n", usekeys[6]);
		menu_keypress(usekeys[6]);
	}
	if (key == GLUT_KEY_UP) {
		printf("Call menu_keypress(%i)\n", usekeys[7]);
		menu_keypress(usekeys[7]);
	}
	if (key == GLUT_KEY_DOWN) {
		printf("Call menu_keypress(%i)\n", usekeys[8]);
		menu_keypress(usekeys[8]);
	}
	if (key == GLUT_KEY_F2) {
		menu_keypress(101); //show low bat warning
	}
	if (key == GLUT_KEY_F3) {
		menu_keypress(102); //disable low bat warning
	}
}

static void redraw(int param) {
	g_state.subsecond++;
	if (g_state.subsecond == 8) {
		g_state.subsecond = 0;
		if (g_state.timerCountdownSecs) {
			g_state.timerCountdownSecs--;
		}
		animategfx();
	}
	g_state.time = time(NULL) - 946684800; //[seconds] since 1.1.2000
	if (g_dispUpdate) {
		g_dispUpdate();
	}
	if ((g_dispUpdate) || (g_state.subsecond == 0)) {
		menu_redraw();
		menu_screen_flush();
		glutPostRedisplay();
	}
	glutTimerFunc(redraw_ms, redraw, 0);
}

void init_window(void) {
	glutInitDisplayMode(GLUT_RGBA | GLUT_DOUBLE);
	glutInitWindowSize(dispx, dispy);
	glutInitWindowPosition(100,20);
	glutCreateWindow("AdvancedClock PC GUI demo");
	glutDisplayFunc(menu_screen_flush);
	glutReshapeFunc(update_window_size);
	glutTimerFunc(redraw_ms, redraw, 0);
	glClearColor(0.0,0.0,0.0,0.0);
}

void demo_init(void) {
	int i;
	//some default values
	for (i = 0; i < ALARMS; i++) {
		g_settings.alarmHour[i] = 8;
		g_settings.alarmMinute[i] = 0+i*5;
		g_settings.alarmWeekdays[i] = 0x7F;
	}
	g_settings.timerMinutes = 5;
	g_settings.brightnessAuto = 1;
	g_settings.brightness = 70;
	g_settings.clockShowSeconds = 1;
	g_settings.soundAutoOffMinutes = 10;
	g_settings.soundVolume = 20;
	g_settings.soundFrequency = 500;
	g_settings.displayRefresh = 100;
	g_settings.batteryCapacity = 1000;
	g_settings.currentResCal = CURRENTRESCAL_NORMAL;
	g_settings.consumptionLedOneMax = CONSUMPTIONLEDONEMAX_NORMAL;
	g_settings.dcf77Level = DCF77LEVEL_NORMAL;
	g_state.time = time(NULL) - 946684800; //[seconds] since 1.1.2000
	g_state.ldr = 2048;
	g_state.brightnessLdr = 100;
	g_state.keyDebugAd = 700;
	g_state.gradcelsius10 = 123; //[1/10°C]
	g_state.chargerCurrent = 10;
	g_state.batVoltage = 3300;
	g_state.performanceRcRunning = 100;
	g_state.performanceCpuRunning = 25;
}

int main(int argc, char ** argv) {
	dispx = MENU_SCREEN_X*20;
	dispy = MENU_SCREEN_Y*20 + 20;
	if ((dispx > 1000) || (dispy > 1000)) {
		dispx = 1000;
		dispy = 1000*MENU_SCREEN_Y/MENU_SCREEN_X;
	}
	glutInit(&argc, argv); //*must* be called before gui_init
	init_window();
	menu_redraw();
	demo_init();
	gui_init(); //*must* be called before the first menu_keypress()
	int i;
	for (i = 1; i < argc; i++) {
		int x = atoi(argv[i]);
		 if ((x > 0) && (x <= 255)) {
				menu_keypress(x);
		}
	}
	glutMouseFunc(input_mouse_key);
	glutPassiveMotionFunc(input_mouse_move);
	glutSpecialFunc(input_key_special);
	glutMainLoop();
	return 0;
}
