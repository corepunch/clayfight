// i_sound.h -- SDL audio mixer for 8-bit PCM samples (cf. i_sound.h)

#ifndef __I_SOUND__
#define __I_SOUND__

#include "pr_def.h"

typedef struct {
    const byte *data; // unsigned 8-bit mono
    int length;
    int rate;
} sfxdata_t;

void I_InitSound(void);
void I_ShutdownSound(void);
// vol 0..1, sep -1 (left) .. 1 (right)
void I_StartSound(const sfxdata_t *sfx, float vol, float sep);

#endif
