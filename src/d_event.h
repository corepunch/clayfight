// d_event.h -- input events and the event queue (cf. d_event.h)

#ifndef __D_EVENT__
#define __D_EVENT__

#include "pr_def.h"

typedef enum {
    ev_keydown,
    ev_keyup,
} evtype_t;

// Keyboard events carry an SDL_Scancode. Game controllers are mapped onto
// virtual keys above KEY_PAD so bindings treat both the same way.
#define KEY_PAD 1000
#define PADKEY(pad, k) (KEY_PAD + (pad) * 16 + (k))
enum { PK_UP, PK_DOWN, PK_LEFT, PK_RIGHT, PK_B0, PK_B1, PK_B2, PK_B3, PK_START, PK_BACK };

typedef struct {
    evtype_t type;
    int key;
} event_t;

typedef bool (*responder_t)(const event_t *ev);

void D_PostEvent(const event_t *ev);
void D_ProcessEvents(responder_t *responders, int count);

#endif
