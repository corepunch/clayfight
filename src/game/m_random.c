// m_random.c -- deterministic random numbers (cf. m_random.c)
//
// Doom indexes a fixed 256-entry table so demos and netgames replay exactly.
// Same contract here: the play simulation only uses P_Random, cosmetic code
// uses M_Random, and both reset at the start of a match.

#include "game/resources.h"

static byte rndtable[256];
static int prndindex, rndindex;
static bool initialized;

static void M_InitTable(void)
{
    uint32_t s = 0x1d872b41;
    for (int i = 0; i < 256; i++) {
        s ^= s << 13;
        s ^= s >> 17;
        s ^= s << 5;
        rndtable[i] = s & 0xff;
    }
    initialized = true;
}

int P_Random(void)
{
    if (!initialized)
        M_InitTable();
    prndindex = (prndindex + 1) & 0xff;
    return rndtable[prndindex];
}

int M_Random(void)
{
    if (!initialized)
        M_InitTable();
    rndindex = (rndindex + 1) & 0xff;
    return rndtable[rndindex];
}

void M_ClearRandom(void)
{
    prndindex = rndindex = 0;
}
