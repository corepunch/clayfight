// d_event.c -- the event queue (cf. D_PostEvent / D_ProcessEvents in d_main.c)

#include "platform/platform.h"

#define MAXEVENTS 64

static event_t events[MAXEVENTS];
static int eventhead, eventtail;

void D_PostEvent(const event_t *ev)
{
    events[eventhead] = *ev;
    eventhead = (eventhead + 1) % MAXEVENTS;
}

// Drain the queue through the responders; the first one to eat an event wins.
void D_ProcessEvents(responder_t *responders, int count)
{
    for (; eventtail != eventhead; eventtail = (eventtail + 1) % MAXEVENTS) {
        const event_t *ev = &events[eventtail];
        for (int i = 0; i < count; i++)
            if (responders[i](ev))
                break;
    }
}
