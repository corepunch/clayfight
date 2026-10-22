// s_sound.h -- sound effects (cf. s_sound.h, sounds.h)

#ifndef __S_SOUND__
#define __S_SOUND__

typedef enum {
    sfx_hit,
    sfx_hithard,
    sfx_block,
    sfx_swish,
    sfx_thud,
    sfx_crowd,
    sfx_select,
    NUMSFX
} sfxenum_t;

void S_Init(void);
// x: world x of the source (for stereo), or < 0 for centred
void S_StartSound(sfxenum_t id, float x, float vol);
// play sample `which` of a fighter's voice file (S04REXSD.GRA ...)
void S_StartVoice(const char *prefix, int which, float x, float vol);

#endif
