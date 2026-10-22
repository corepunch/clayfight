// pr_def.h -- global engine constants and basic types (cf. doomdef.h)

#ifndef __PR_DEF__
#define __PR_DEF__

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

typedef uint8_t byte;
typedef uint32_t pixel_t; // 0xAARRGGBB

#define SCREENWIDTH 320
#define SCREENHEIGHT 200

// Fixed simulation rate. Doom runs at 35Hz; arcade fighters run at 60Hz.
#define TICRATE 60

#define MAXPLAYERS 2

typedef enum {
    GS_TITLE,
    GS_SELECT,
    GS_FIGHT,
} gamestate_t;

// ticcmd_t.buttons bits. Primal Rage's four buttons are HI/LO x QUICK/FIERCE.
enum {
    BT_UP = 1 << 0,
    BT_DOWN = 1 << 1,
    BT_LEFT = 1 << 2,
    BT_RIGHT = 1 << 3,
    BT_HQ = 1 << 4, // high quick
    BT_HF = 1 << 5, // high fierce
    BT_LQ = 1 << 6, // low quick
    BT_LF = 1 << 7, // low fierce
    BT_START = 1 << 8,
    BT_ATTACKS = BT_HQ | BT_HF | BT_LQ | BT_LF,
};

typedef struct {
    int buttons;
} ticcmd_t;

// Arena geometry, in world pixels. z grows upward from the floor (0).
#define ARENA_WIDTH 480
#define FLOOR_Y 182 // screen row of the floor line
#define GRAVITY 0.42f

#define ROUND_TIME 60 // seconds
#define ROUNDS_TO_WIN 2
#define MAX_HEALTH 100

#define MIN(a, b) ((a) < (b) ? (a) : (b))
#define MAX(a, b) ((a) > (b) ? (a) : (b))
#define CLAMP(x, lo, hi) MIN(MAX(x, lo), hi)

#endif
