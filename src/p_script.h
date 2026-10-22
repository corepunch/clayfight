// p_script.h -- Primal Rage's own animation scripts and move tables, run
// straight out of PRAGE.EXE (see w_exe.c)
//
// An animation is a stream of 16-bit words. A word below 0x8000 shows a
// "frame id" (bit 15 of a frame word mirrors it); anything else is an
// opcode: number = (w >> 8) & 0x1F, operand mode = w & 0x6000, selector =
// low byte (opcode 0x1F: the real opcode is the low byte and the selector is
// the next word). Frame ids index a global table of (GRA file id << 23 |
// frame record offset). Each script runs with a frame timer: every tic the
// timer drops by one; at zero the script advances to its next frame and the
// timer gains that frame's duration.
//
// The game code calls native routines from scripts (CALL/CALL_YIELD). Those
// are bound to C functions here by address -- the role of Doom's action
// pointers. Unknown routines are skipped and reported once.

#ifndef __P_SCRIPT__
#define __P_SCRIPT__

#include "pr_def.h"
#include "w_gra.h"

#define PS_NUMROWS 62 // core animations per character
#define PS_NUMVARS 6
#define PS_EPSILON 1e-3f // frame timers are floats; fractional durations must not drift a tic

// PC pixels per arcade unit: the PC art is the arcade art scaled down.
#define ARCADE_XSCALE 0.81f
#define ARCADE_YSCALE 0.708f

// the 62 core rows, named from what they show (the game names none of them)
enum {
    ROW_STANCE = 0,
    ROW_CROUCH = 1,
    ROW_CROUCHED = 2,
    ROW_STANDUP = 3,
    ROW_JUMP = 4,
    ROW_WALKF = 7,
    ROW_WALKB = 10,
    ROW_JUMPMOVE = 12,
    ROW_LAND = 13,
    ROW_BLOCK = 38,
    ROW_CROUCHBLOCK = 40,
    ROW_HIT = 42,
    ROW_CROUCHHIT = 43,
    ROW_HITHARD = 44,
    ROW_FALL = 46,
    ROW_LYING = 52,
    ROW_GETUP = 53,
    ROW_DIZZY = 54,
    ROW_WIN = 58,
};

// A script in progress (the animation part of the game's object struct).
typedef struct {
    uint32_t ip;
    float timer, duration;
    uint32_t timing; // per-frame tick table (indexed by v52), 0 = off
    uint32_t table; // MOVE_X/MOVE_Y data table
    int vars[PS_NUMVARS]; // v52 v53 v54 v55 slot v58
    int loopcount;
    int step; // frames advanced since the start (attack boxes key on it)
    int cyclelen; // range of the breathing index (v52)
    int breathtimer;
    bool done;
    // what to show
    int frameid;
    bool hflip;
    gra_t *sprite;
    int frame;
    int ax, ay; // per-frame anchor
    float movex, movey; // root motion accumulated this tic
    bool flipfacing; // FLIP_FACING executed this tic
    int linkstun; // stun of the attack that started a hit reaction (kept across PS_Start)
} psanim_t;

typedef struct {
    int step, ticks; // starts when `step` frames have played, lasts `ticks`
    int x, y, w, h; // in 3-pixel units, anchor-relative
} psbox_t;

typedef struct {
    bool valid;
    int row;
    int damage;
    int strength; // 1 light .. 3 heavy
    int stun; // A[1]: hit reactions stretch to this many tics
    int hitclass; // raw C[2]
    int numboxes;
    psbox_t boxes[5];
} psmove_t;

bool PS_Init(void); // after X_Init; false if the tables don't check out
bool PS_Active(void);
uint32_t PS_RowScript(int character, int row);
float PS_RowDuration(int row);
bool PS_Move(int character, int id, psmove_t *out);
bool PS_FrameInfo(int frameid, gra_t **sprite, int *frame, int *ax, int *ay);

void PS_Start(psanim_t *a, uint32_t script, float duration);
// Advance one tic. Returns true when a new frame came up (a->step grew).
bool PS_Tick(psanim_t *a);

#endif
