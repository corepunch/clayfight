// m_menu.h -- title screen (cf. m_menu.h)

#ifndef __M_MENU__
#define __M_MENU__

#include "d_event.h"

void M_StartTitle(void);
bool M_Responder(const event_t *ev); // returns true when start is pressed
void M_Ticker(void);
void M_Drawer(void);

#endif
