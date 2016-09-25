#ifndef GUI_H
#define GUI_H



typedef void dispUpdateFunc(void);

extern dispUpdateFunc * g_dispUpdate; //NULL is a legitimate value

void updateLightText(void);
void updateDispbrightText(void);

void gui_init(void);

#endif

