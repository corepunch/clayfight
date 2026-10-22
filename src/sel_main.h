// sel_main.h -- character select screen (cf. wi_stuff.h)

#ifndef __SEL_MAIN__
#define __SEL_MAIN__

#include "d_event.h"

void SEL_Start(void);
bool SEL_Responder(const event_t *ev);
void SEL_Ticker(void);
void SEL_Drawer(void);

#endif
