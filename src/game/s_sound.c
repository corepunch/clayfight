// s_sound.c -- sound effect registry and playback (cf. s_sound.c, sounds.c)
//
// Generic effects live in S04SD*.GRA (one sample each); every fighter also
// has S04<prefix>SD.GRA with its voice samples. Which file is which effect is
// a best guess from the names: the trigger tables are in PRAGE.EXE.

#include <stdio.h>
#include <string.h>

#include "game/game.h"

#define MAXVARIANTS 5

static const char *sfxfiles[NUMSFX][MAXVARIANTS] = {
    [sfx_hit] = {"S04SDH1.GRA", "S04SDH2.GRA", "S04SDH3.GRA", "S04SDH4.GRA", "S04SDH5.GRA"},
    [sfx_hithard] = {"S04SDH6.GRA", "S04SDH7.GRA", "S04SDH8.GRA", "S04SDH9.GRA"},
    [sfx_block] = {"S04SDBCR.GRA"},
    [sfx_swish] = {"S04SDSWD.GRA", "S04SDSWR.GRA"},
    [sfx_thud] = {"S04SDT1.GRA", "S04SDT2.GRA", "S04SDQUK.GRA"},
    [sfx_crowd] = {"S04SDC1.GRA", "S04SDC2.GRA", "S04SDC3.GRA"},
    [sfx_select] = {"S04SDZAP.GRA"},
};

static sfxdata_t *sfx[NUMSFX][MAXVARIANTS];
static int nvariants[NUMSFX];

typedef struct {
    char prefix[8];
    sfxdata_t *samples;
    int count;
} voice_t;

#define MAXVOICES 8
static voice_t voices[MAXVOICES];
static int numvoices;

void S_Init(void)
{
    I_InitSound();
    for (int i = 0; i < NUMSFX; i++) {
        for (int k = 0; k < MAXVARIANTS && sfxfiles[i][k]; k++) {
            sfxdata_t *s;
            if (W_LoadSounds(sfxfiles[i][k], &s) > 0)
                sfx[i][nvariants[i]++] = s;
        }
    }
}

static float S_Sep(float x)
{
    return x < 0 ? 0 : ((x - level.camx) / VIEWWIDTH - 0.5f) * 1.2f;
}

void S_StartSound(sfxenum_t id, float x, float vol)
{
    if (!options.sound || !options.volume) return;
    vol *= options.volume / 10.f;
    if (!nvariants[id])
        return;
    I_StartSound(sfx[id][M_Random() % nvariants[id]], vol, S_Sep(x));
}

static voice_t *S_Voice(const char *prefix)
{
    for (int i = 0; i < numvoices; i++)
        if (!strcmp(voices[i].prefix, prefix))
            return &voices[i];
    if (numvoices == MAXVOICES)
        return NULL;
    voice_t *v = &voices[numvoices++];
    snprintf(v->prefix, sizeof v->prefix, "%s", prefix);
    char name[32];
    snprintf(name, sizeof name, "S04%sSD.GRA", prefix);
    v->count = W_LoadSounds(name, &v->samples);
    return v;
}

void S_StartVoice(const char *prefix, int which, float x, float vol)
{
    if (!options.sound || !options.volume) return;
    vol *= options.volume / 10.f;
    voice_t *v = S_Voice(prefix);
    if (v && v->count)
        I_StartSound(&v->samples[which % v->count], vol, S_Sep(x));
}
