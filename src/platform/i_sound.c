// i_sound.c -- SDL audio mixer for 8-bit PCM samples (cf. i_sound.c)

#include <SDL.h>

#include "platform/platform.h"

#define NUMCHANNELS 16
#define MIXRATE 44100

typedef struct {
    const sfxdata_t *sfx;
    uint32_t pos; // 16.16 fixed point sample position
    uint32_t step;
    int left, right; // 0..256
} channel_t;

static channel_t channels[NUMCHANNELS];
static SDL_AudioDeviceID device;

static void I_Mix(void *userdata, Uint8 *stream, int len)
{
    (void)userdata;
    int16_t *out = (int16_t *)stream;
    int frames = len / 4;
    for (int i = 0; i < frames; i++) {
        int l = 0, r = 0;
        for (int c = 0; c < NUMCHANNELS; c++) {
            channel_t *ch = &channels[c];
            if (!ch->sfx)
                continue;
            int idx = ch->pos >> 16;
            if (idx >= ch->sfx->length) {
                ch->sfx = NULL;
                continue;
            }
            int s = (ch->sfx->data[idx] - 128) << 8;
            l += (s * ch->left) >> 8;
            r += (s * ch->right) >> 8;
            ch->pos += ch->step;
        }
        out[i * 2] = (int16_t)CLAMP(l, -32768, 32767);
        out[i * 2 + 1] = (int16_t)CLAMP(r, -32768, 32767);
    }
}

void I_InitSound(void)
{
    if (SDL_InitSubSystem(SDL_INIT_AUDIO) < 0) {
        I_Log("I_InitSound: %s", SDL_GetError());
        return;
    }
    SDL_AudioSpec want = {0}, have;
    want.freq = MIXRATE;
    want.format = AUDIO_S16SYS;
    want.channels = 2;
    want.samples = 1024;
    want.callback = I_Mix;
    device = SDL_OpenAudioDevice(NULL, 0, &want, &have, 0);
    if (!device) {
        I_Log("I_InitSound: %s", SDL_GetError());
        return;
    }
    SDL_PauseAudioDevice(device, 0);
}

void I_ShutdownSound(void)
{
    if (device)
        SDL_CloseAudioDevice(device);
}

void I_StartSound(const sfxdata_t *sfx, float vol, float sep)
{
    if (!device || !sfx || !sfx->length)
        return;
    SDL_LockAudioDevice(device);
    // take a free channel, or steal the one furthest along
    int best = 0;
    uint32_t bestpos = 0;
    for (int c = 0; c < NUMCHANNELS; c++) {
        if (!channels[c].sfx) {
            best = c;
            break;
        }
        if (channels[c].pos > bestpos) {
            bestpos = channels[c].pos;
            best = c;
        }
    }
    channel_t *ch = &channels[best];
    sep = CLAMP(sep, -1.f, 1.f);
    vol = CLAMP(vol, 0.f, 1.f);
    ch->sfx = sfx;
    ch->pos = 0;
    ch->step = (uint32_t)(((uint64_t)sfx->rate << 16) / MIXRATE);
    ch->left = (int)(256 * vol * (sep > 0 ? 1 - sep : 1));
    ch->right = (int)(256 * vol * (sep < 0 ? 1 + sep : 1));
    SDL_UnlockAudioDevice(device);
}
