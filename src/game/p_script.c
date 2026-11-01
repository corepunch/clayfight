// p_script.c -- Primal Rage's animation script interpreter and move tables
//
// Addresses refer to the interleaved big-endian arcade 2.3 program chips.
// Retail fighters run their ROM bytecode directly; photos use authored states.

#include <stdio.h>

#include "game/game.h"

#define NUMFRAMEIDS 12000
#define SCRIPTTAB 0x6d964 // 65 rows x 10 slots, arcade 2.3 Dec 1994
#define MOVES_A 0xfbba0
#define MOVES_B 0x6fca4
#define MOVES_C 0x72ea4
#define HITBOXES 0x7d328
#define BOXSHAPES 0x49f7c
#define FRAMEANCHORS 0xfd148 // collision anchors, distinct from RLE drawing pivots

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
    gra_t *g = W_LoadGra("arcade");
    patch_t *p = W_Frame(g, frameid);
    if (!p) return false;
    *sprite = g; *frame = frameid;
    *ax = X_U8(FRAMEANCHORS + frameid * 2);
    *ay = X_U8(FRAMEANCHORS + frameid * 2 + 1);
    return true;
}

bool PS_Init(void)
{
    active = false;
    if (!X_Loaded())
        return false;
    // sanity checks against the build these addresses came from: Sauron's
    // stance starts on arcade motion object 3818.
    gra_t *g;
    int frame, ax, ay;
    uint32_t stance = X_U32(SCRIPTTAB);
    if (!X_Valid(stance, 2) || X_U16(stance) != 3818 || !PS_FrameInfo(3818, &g, &frame, &ax, &ay)
        || frame != 3818) {
        I_Log("PS_Init: Arcade ROM tables do not match the supported revision");
        return false;
    }
    active = true;
    I_Log("PS_Init: running arcade 2.3 animation scripts");
    return true;
}

uint32_t PS_RowScript(int character, int row)
{
    int arcade_row = row + (row >= 12) + (row >= 54);
    return X_U32(SCRIPTTAB + 4 * (arcade_row * 10 + character));
}

float PS_RowDuration(int row)
{
    return row == 0 || row == 1 ? 3 : row >= 0 && row < PS_NUMROWS ? rowduration[row] : 2;
}

bool PS_Move(int character, int id, psmove_t *m)
{
    int k = character * 64 + id;
    *m = (psmove_t){0};
    uint32_t rowptr = X_U32(MOVES_B + 20 * k + 4);
    if (rowptr < SCRIPTTAB || rowptr >= SCRIPTTAB + 65 * 40)
        return false; // special moves start from code, not from the row table
    m->valid = true;
    int r = (rowptr - SCRIPTTAB) / 40;
    m->row = r - (r >= 13) - (r >= 56);
    m->damage = X_U8(MOVES_A + 11 * k);
    m->strength = X_U8(MOVES_A + 11 * k + 4);
    m->stun = X_U8(MOVES_A + 11 * k + 1);
    m->hitclass = X_U8(MOVES_C + 6 * k + 3) & 7;
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

static int AnimRandom(psanim_t *a)
{
    if(!a->cosmetic_random)return P_Random();
    uint32_t r=a->cosmetic_random;r^=r<<13;r^=r>>17;r^=r<<5;
    a->cosmetic_random=r;return r&255;
}

// 0x1c4c4: the "breathing" idle. Every few tics pick a random direction and
// drift the frame index v52 through the cycle, wrapping at both ends.
static void N_Breathe(psanim_t *a)
{
    int *v52 = &a->vars[0], *v58 = &a->vars[5];
    if (--a->breathtimer < 0) {
        *v58 = AnimRandom(a) & 1 ? 1 : -1;
        a->breathtimer = a->cyclelen / 3 * 3;
    }
    *v52 += *v58;
    if (*v52 >= a->cyclelen)
        *v52 = 0;
    if (*v52 < 0)
        *v52 = a->cyclelen - 1;
}

// 0x150d4 (arg n): hit reactions spread the attacker's stun over n frames.
static void N_StunDuration(psanim_t *a, int arg)
{
    if (arg > 0 && a->linkstun > 0)
        a->duration = (float)a->linkstun / arg;
}

// 0x1a8ac: frame timer and duration set to 3 (Vertigo's backward hop)
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

static void N_Crouch(psanim_t *a, int arg)
{
    (void)arg; a->vars[0]=0; a->duration=3; a->cyclelen=20;
}

static const struct {
    uint32_t addr;
    bool ends; // control returns to game code: the animation is over
    void (*fn)(psanim_t *a, int arg);
} natives[] = {
    {0x1c4c4, false, N_Breathe1},
    {0x150d4, false, N_StunDuration},
    {0x1a8ac, false, N_Duration3},
    {0x2d03c, false, NULL}, // clears a flag on the linked object
    {0x1c5f2, false, NULL}, // victory pose finished (global flag)
    {0x1b382, true, NULL}, // clear a per-player counter, then back to neutral
    {0x1c8b6, false, NULL}, // Diablo's victory flames (spawns an object)
    {0x42fee, false, NULL}, // per-player bookkeeping
    {0x1506c, false, NULL}, // per-player action timer, maintained by our simulation
    {0x3dae8, true, NULL}, // clear player flag and return to neutral
    {0x1ae76, false, N_Crouch}, // crouch bookkeeping
    {0x1a45c, true, NULL}, // end of walk step
    {0x1960c, true, NULL}, // knocked down: game code takes over
};

static bool N_Call(psanim_t *a, uint32_t addr, int arg)
{
    // Native scenery chance routine: select the eruption branch one time in n.
    if(a->cosmetic_random && addr==0x23336){a->vars[1]=AnimRandom(a)%MAX(1,arg)==0;return false;}
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
    if (!table) return 30;
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
            a->timer += opc == 0x07 ? value : AnimRandom(a) % (MAX(value, 0) + 1);
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

void PS_StartScenery(psanim_t *a, uint32_t script, float duration, uint32_t seed)
{
    *a = (psanim_t){.ip = script, .duration = duration, .step = -1, .linkstun = a->linkstun, .cosmetic_random=seed};
    if (Run(a) != RUN_SHOWN && a->step < 0)
        a->done = true;
}

void PS_Start(psanim_t *a, uint32_t script, float duration)
{
    PS_StartScenery(a,script,duration,0);
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
