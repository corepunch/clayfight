// platform.h -- everything that talks to the machine: SDL video, input,
// sound and timing (cf. Doom's i_*.h, doomtype.h, d_event.h). The engine and
// game layers build on this; nothing here knows about them.

#ifndef __PLATFORM__
#define __PLATFORM__

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

// --- basic types and the screen (cf. doomtype.h) ----------------------------

typedef uint8_t byte;
typedef uint32_t pixel_t; // 0xAARRGGBB

#define SCREENWIDTH 320
#define SCREENHEIGHT 200

// Fixed simulation rate. Doom runs at 35Hz; arcade fighters run at 60Hz.
#define TICRATE 60

#define MIN(a, b) ((a) < (b) ? (a) : (b))
#define MAX(a, b) ((a) > (b) ? (a) : (b))
#define CLAMP(x, lo, hi) MIN(MAX(x, lo), hi)

// --- i_system.c: timing, memory and errors (cf. i_system.h) ----------------
int I_GetTime(void); // tics since startup
void I_Error(const char *fmt, ...) __attribute__((noreturn, format(printf, 1, 2)));
void I_Log(const char *fmt, ...) __attribute__((format(printf, 1, 2)));
void *I_Malloc(size_t size); // zeroed, never returns NULL

// --- i_video.c: window, framebuffer and input polling (cf. i_video.h) ------
void I_InitGraphics(bool headless);
void I_ShutdownGraphics(void);
void I_FinishUpdate(const pixel_t *screen);
void I_StartTic(void); // pump SDL events into the event queue
bool I_QuitRequested(void);
void I_SaveScreenshot(const pixel_t *screen, const char *path);

// --- i_sound.c: SDL audio mixer for 8-bit PCM samples (cf. i_sound.h) ------
typedef struct {
    const byte *data; // unsigned 8-bit mono
    int length;
    int rate;
} sfxdata_t;

void I_InitSound(void);
void I_ShutdownSound(void);
// vol 0..1, sep -1 (left) .. 1 (right)
void I_StartSound(const sfxdata_t *sfx, float vol, float sep);

// --- d_event.c: input events and the event queue (cf. d_event.h) -----------
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
