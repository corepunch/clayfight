// p_script.c -- Primal Rage's animation script interpreter and move tables
//
// Table addresses are linear addresses in PRAGE.EXE (PC version 1.0, 1995)
// after loading its LE objects (w_exe.c). Interpreter semantics follow the
// game's own interpreter at code address 0x2B780.

#include <stdio.h>

#include "game/game.h"

#define FRAMETAB 0xF8A74 // u32[NUMFRAMEIDS]: gra file id << 23 | frame record offset
#define NUMFRAMEIDS 18232
#define ANCHORS 0x124A0C // u8[NUMFRAMEIDS][2]: anchor x, y
#define SCRIPTTAB 0x118298 // u32[62 rows][10 character slots]
#define MOVES_A 0x12D78C // 11 bytes per move: damage, ..., strength at [4]
#define MOVES_B 0xF3488 // 20 bytes per move: handler, script row pointer, sounds, ?, score
#define MOVES_C 0xF6688 // 6 bytes per move: [2] = hit class
#define HITBOXES 0xE68A4 // [char][64 moves] 5 x (step, box index, active tics)
#define BOXSHAPES 0x11BC84 // u8[char][256][4]: x, y, w, h

#define MAXOPS 512 // instructions per advance before we call it a runaway

static bool active;

// Default frame duration per core row, as passed by the code that starts
// each animation (identical for all seven characters).
static const byte rowduration[PS_NUMROWS] = {
    2, 2, 3, 2, 2, 2, 1, 3, 1, 1, 3, 1, 1, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,
    2, 2, 2, 2, 2, 2, 2, 3, 3, 3, 2, 3, 3, 3, 3, 3, 2, 3, 2, 3, 3, 3, 3, 3, 1, 1, 4, 3, 3, 3, 3,
};

bool PS_Active(void)
{
    return active;
}

bool PS_FrameInfo(int frameid, gra_t **sprite, int *frame, int *ax, int *ay)
{
    frameid &= 0x7fff;
    if (frameid >= NUMFRAMEIDS)
        return false;
    uint32_t v = X_U32(FRAMETAB + 4 * frameid);
    gra_t *g = W_GraById(v >> 23);
    if (!g)
        return false;
    int off = (int)(v & 0x7fffff) - g->framechunk - 8;
    if (off < 0 || off % 12 || off / 12 >= g->numframes)
        return false;
    *sprite = g;
    *frame = off / 12;
    *ax = X_U8(ANCHORS + 2 * frameid);
    *ay = X_U8(ANCHORS + 2 * frameid + 1);
    return true;
}

bool PS_Init(void)
{
    active = false;
    if (!X_Loaded())
        return false;
    // sanity checks against the build these addresses came from: Sauron's
    // stance starts on frame id 3842 = S04REX.GRA #37
    gra_t *g;
    int frame, ax, ay;
    uint32_t stance = X_U32(SCRIPTTAB);
    if (!X_Valid(stance, 2) || X_U16(stance) != 3842 || !PS_FrameInfo(3842, &g, &frame, &ax, &ay)
        || frame != 37) {
        I_Log("PS_Init: PRAGE.EXE tables don't match the expected build; using built-in tables");
        return false;
    }
    active = true;
    I_Log("PS_Init: running Primal Rage's own animation scripts");
    return true;
}

uint32_t PS_RowScript(int character, int row)
{
    return X_U32(SCRIPTTAB + 4 * (row * 10 + character));
}

float PS_RowDuration(int row)
{
    return row >= 0 && row < PS_NUMROWS ? rowduration[row] : 2;
}

bool PS_Move(int character, int id, psmove_t *m)
{
    int k = character * 64 + id;
    *m = (psmove_t){0};
    uint32_t rowptr = X_U32(MOVES_B + 20 * k + 4);
    if (rowptr < SCRIPTTAB || rowptr >= SCRIPTTAB + PS_NUMROWS * 40)
        return false; // special moves start from code, not from the row table
    m->valid = true;
    m->row = (rowptr - SCRIPTTAB) / 40;
    m->damage = X_U8(MOVES_A + 11 * k);
    m->strength = X_U8(MOVES_A + 11 * k + 4);
    m->stun = X_U8(MOVES_A + 11 * k + 1);
    m->hitclass = X_U8(MOVES_C + 6 * k + 2);
    uint32_t t = HITBOXES + 0x3c0 * character + 15 * id;
    for (int j = 0; j < 5; j++) {
        int step = X_S8(t + 3 * j);
        int index = X_U8(t + 3 * j + 1);
        uint32_t shape = BOXSHAPES + 4 * (character * 256 + index);
        psbox_t b = {step, X_U8(t + 3 * j + 2), X_U8(shape), X_U8(shape + 1), X_U8(shape + 2), X_U8(shape + 3)};
        if (step < 0 || b.w < 1 || b.h < 1) // the game skips these too
            continue;
        m->boxes[m->numboxes++] = b;
    }
    return true;
}

// --- the interpreter ---------------------------------------------------------

typedef enum { RUN_SHOWN, RUN_PAUSED, RUN_STOPPED } runresult_t;

// Variable selectors: 0x40..0x45 are this object's bytes (v52 v53 v54 v55,
// slot, v58); 0x46..0x4B the linked object's; below 0x40 a global array.
static int globals[0x40];

static int GetVar(const psanim_t *a, int sel)
{
    if (sel >= 0x40 && sel < 0x40 + PS_NUMVARS)
        return a->vars[sel - 0x40];
    if (sel < 0x40)
        return globals[sel];
    return 0;
}

static void SetVar(psanim_t *a, int sel, int value)
{
    if (sel >= 0x40 && sel < 0x40 + PS_NUMVARS)
        a->vars[sel - 0x40] = value;
    else if (sel < 0x40)
        globals[sel] = value;
}

// --- native routines called from scripts (cf. Doom's action functions) ----

// 0x38294: the "breathing" idle. Every few tics pick a random direction and
// drift the frame index v52 through the cycle, wrapping at both ends.
static void N_Breathe(psanim_t *a)
{
    int *v52 = &a->vars[0], *v58 = &a->vars[5];
    if (--a->breathtimer < 0) {
        *v58 = P_Random() & 1 ? 1 : -1;
        a->breathtimer = a->cyclelen / 3 * 3;
    }
    *v52 += *v58;
    if (*v52 >= a->cyclelen)
        *v52 = 0;
    if (*v52 < 0)
        *v52 = a->cyclelen - 1;
}

// 0x3a214 (arg n): hit reactions spread the attacker's stun over n frames.
static void N_StunDuration(psanim_t *a, int arg)
{
    if (arg > 0 && a->linkstun > 0)
        a->duration = (float)a->linkstun / arg;
}

// 0x36658: frame timer and duration set to 3 (Vertigo's backward hop)
static void N_Duration3(psanim_t *a, int arg)
{
    (void)arg;
    a->duration = 3;
}

static void N_Breathe1(psanim_t *a, int arg)
{
    (void)arg;
    N_Breathe(a);
}

static const struct {
    uint32_t addr;
    bool ends; // control returns to game code: the animation is over
    void (*fn)(psanim_t *a, int arg);
} natives[] = {
    {0x38294, false, N_Breathe1},
    {0x3a214, false, N_StunDuration},
    {0x36658, false, N_Duration3},
    {0x40824, false, NULL}, // clears a flag on the linked object
    {0x385f4, false, NULL}, // victory pose finished (global flag)
    {0x3cafc, true, NULL}, // clear a per-player counter, then back to neutral
    {0x416dc, false, NULL}, // Diablo's victory flames (spawns an object)
    {0x154d0, false, NULL}, // per-player bookkeeping
    {0x3a1f0, false, NULL}, // set a per-player timer word
    {0x36c5c, false, NULL}, // crouch bookkeeping
    {0x370c0, true, NULL}, // back to neutral
    {0x3618c, true, NULL}, // end of walk step
    {0x35058, true, NULL}, // knocked down: game code takes over
    {0x383a0, true, NULL}, // end of victory pose
};

static bool N_Call(psanim_t *a, uint32_t addr, int arg)
{
    static uint32_t reported[32];
    static int numreported;
    for (size_t i = 0; i < sizeof natives / sizeof natives[0]; i++) {
        if (natives[i].addr == addr) {
            if (natives[i].fn)
                natives[i].fn(a, arg);
            return natives[i].ends;
        }
    }
    for (int i = 0; i < numreported; i++)
        if (reported[i] == addr)
            return false;
    if (numreported < 32)
        reported[numreported++] = addr;
    I_Log("PS: unhandled native call %#x", addr);
    return false;
}

// Number of frame ids the breathing index may walk through: consecutive ids
// that stay in the starting frame's file.
static int CycleLength(uint32_t base, bool table)
{
    gra_t *g0 = NULL, *g;
    int f, ax, ay, n = 0;
    for (; n < 64; n++) {
        int id = table ? X_U16(base + 2 * n) : (int)base + n;
        if ((table && id & 0x8000) || !PS_FrameInfo(id, &g, &f, &ax, &ay))
            break;
        if (!g0)
            g0 = g;
        else if (g != g0)
            break;
    }
    return MAX(n, 1);
}

static void Show(psanim_t *a, int word)
{
    a->frameid = word & 0x7fff;
    a->hflip = word & 0x8000;
    PS_FrameInfo(a->frameid, &a->sprite, &a->frame, &a->ax, &a->ay);
    a->step++;
    a->timer += a->timing ? X_U8(a->timing + a->vars[0]) : a->duration;
}

// Read an operand per the instruction's mode. *p points past the opcode words.
static int Operand(psanim_t *a, int mode, int sel, bool ext, uint32_t *p)
{
    switch (mode) {
    case 0x0000:
        return ext ? (int16_t)sel : (int8_t)sel;
    case 0x2000:
        return GetVar(a, sel);
    case 0x4000: {
        int v = (int)X_U32(*p);
        *p += 4;
        return v;
    }
    default: { // 0x6000: value from the data table, indexed by a variable
        int tabletype = X_U16(*p + 2) & 0xf000;
        *p += 4;
        int idx = GetVar(a, sel);
        if (!a->table)
            return 0;
        if (tabletype == 0x1000)
            return X_S8(a->table + 2 * idx);
        if (tabletype == 0x2000)
            return X_S8(a->table + 2 * idx + 1);
        return X_S8(a->table + idx);
    }
    }
}

static runresult_t Run(psanim_t *a)
{
    for (int guard = 0; guard < MAXOPS; guard++) {
        uint32_t ip = a->ip;
        int w = X_U16(ip);
        uint32_t p = ip + 2;
        if (!(w & 0x8000)) {
            a->ip = p;
            Show(a, w);
            return RUN_SHOWN;
        }
        int mode = w & 0x6000, opc = (w >> 8) & 0x1f, sel = w & 0xff;

        if (opc == 0x0d) { // FRAME_SELECT: frame = base + var, or table[var]
            bool table = mode == 0x6000;
            uint32_t base = table ? X_U32(p) : X_U16(p);
            p += table ? 4 : 2;
            if (!a->cyclelen)
                a->cyclelen = CycleLength(base, table);
            int v = GetVar(a, sel);
            a->ip = p;
            Show(a, table ? X_U16(base + 2 * v) : (int)base + v);
            return RUN_SHOWN;
        }
        bool ext = opc == 0x1f;
        if (ext) {
            opc = sel;
            sel = X_U16(p);
            p += 2;
        }
        bool varop = opc == 0x0e || opc == 0x0f || opc == 0x17; // low byte names a variable
        int value = varop && mode == 0 ? 0 : Operand(a, mode, sel, ext, &p);
        uint32_t next = p;

        switch (opc) {
        case 0x00: // END
        case 0x01: // HOLD (keep the last frame)
            a->done = true;
            return RUN_STOPPED;
        case 0x03: // GOTO
            next = (uint32_t)value;
            break;
        case 0x04: // LOOPN: jump back `sel` more times
            if (++a->loopcount <= sel)
                next = (uint32_t)value;
            else
                a->loopcount = 0;
            break;
        case 0x05: // WAIT_NONZERO: waits on game code; we don't block
            break;
        case 0x06: { // SWITCH on a variable
            int n = X_U16(p);
            next = p + 2 + 4 * (n + 1);
            if (value >= 1 && value <= n + 1)
                next = X_U32(p + 2 + 4 * (value - 1));
            break;
        }
        case 0x07: // DELAY: hold the current frame longer
        case 0x08: // RANDOM_DELAY
            a->timer += opc == 0x07 ? value : P_Random() % (MAX(value, 0) + 1);
            if (a->timer > PS_EPSILON) {
                a->ip = next;
                return RUN_PAUSED;
            }
            break;
        case 0x0a: // ADD_DURATION
            a->duration += value;
            break;
        case 0x0b: // SET_DURATION
            if (value > 0)
                a->duration = value;
            break;
        case 0x0c: // SPAWN object (projectiles, effects): not yet
            next = p + 4;
            break;
        case 0x0e: // CLEAR_VAR
            SetVar(a, sel, 0);
            break;
        case 0x0f: // SET_VAR
            if (mode == 0) {
                SetVar(a, sel, X_U16(p));
                next = p + 2;
            }
            break;
        case 0x10: // CALL native
        case 0x11: // CALL_ARG native, u16 argument
        case 0x15: // CALL_YIELD: native, then the script stops
            if (opc == 0x11)
                next = p + 2;
            if (N_Call(a, (uint32_t)value, opc == 0x11 ? X_U16(p) : 0) || opc == 0x15) {
                a->done = true;
                return RUN_STOPPED;
            }
            break;
        case 0x14: // FLIP_FACING
            a->flipfacing = true;
            break;
        case 0x17: // INC_VAR
            SetVar(a, sel, GetVar(a, sel) + 1);
            break;
        case 0x18: // INC_BRANCH_LT
        case 0x19: { // DEC_BRANCH_GE
            int v = GetVar(a, sel) + (opc == 0x18 ? 1 : -1);
            int limit = X_U16(p);
            SetVar(a, sel, v);
            next = p + 6;
            if (opc == 0x18 ? v < limit : v >= limit)
                next = X_U32(p + 2);
            break;
        }
        case 0x1a: // SET_DATA_TABLE
            a->table = (uint32_t)value;
            break;
        case 0x1b: // CLEAR_DATA_TABLE
            a->table = 0;
            break;
        case 0x1c: // SET_TIMING_TABLE
            a->timing = (uint32_t)value;
            break;
        case 0x1d: // CLEAR_TIMING_PTR
        case 0x1e: // TIMING_OFF
            a->timing = 0;
            break;
        case 0x20: // MOVE_X (root motion, arcade units)
            a->movex += value;
            break;
        case 0x21: // MOVE_Y
            a->movey += value;
            break;
        case 0x2d: // IF_SPRITE_Y_GE: landing test, handled by our physics
            next = p + 4;
            break;
        case 0x02: case 0x09: case 0x12: case 0x13: case 0x16: case 0x22:
        case 0x25: case 0x26: case 0x27: case 0x28: case 0x29: case 0x2a:
        case 0x2b: case 0x2c: case 0x2e:
            break; // attributes, velocities, sounds: owned by our play code for now
        default:
            I_Log("PS: unknown opcode %#x at %#x", opc, ip);
            a->done = true;
            return RUN_STOPPED;
        }
        a->ip = next;
    }
    I_Log("PS: runaway script at %#x", a->ip);
    a->done = true;
    return RUN_STOPPED;
}

void PS_Start(psanim_t *a, uint32_t script, float duration)
{
    *a = (psanim_t){.ip = script, .duration = duration, .step = -1, .linkstun = a->linkstun};
    if (Run(a) != RUN_SHOWN && a->step < 0)
        a->done = true;
}

bool PS_Tick(psanim_t *a)
{
    a->movex = a->movey = 0;
    a->flipfacing = false;
    if (a->done)
        return false;
    int step = a->step;
    a->timer -= 1;
    for (int guard = 0; a->timer <= PS_EPSILON && !a->done && guard < 16; guard++)
        if (Run(a) == RUN_PAUSED)
            break;
    return a->step != step;
}
