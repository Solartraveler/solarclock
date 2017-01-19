#ifndef GUI_H
#define GUI_H

/*If returning non zero, the displayed text has changed and the menu must be
  redrawn. The easiest way is to return always 1. Howerver a more sophisticated
  update mechanism detects changes and avoid updating if nothing changed.
  This saves power.
*/
typedef uint8_t dispUpdateFunc(void);

extern dispUpdateFunc * g_dispUpdate; //NULL is a legitimate value

uint8_t updateLightText(void);
uint8_t updateDispbrightText(void);

void gui_init(void);

#endif

