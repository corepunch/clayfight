// sel_main.c -- character select screen (cf. wi_stuff.c: its own ticker,
// responder and drawer, entered and left through d_main/g_game)
//
// Art comes from S04SELMO.GRA: a temple backdrop, a 5-frame animated
// portrait and a name plate per fighter. The frame -> sub-palette mapping is
// in game code, so palettes are guessed (W_GuessPalette) and cached.

#include <stdlib.h>

#include "game/game.h"

#define BACKDROP 136 // 272x170 temple
#define LAUNCH_DELAY 50

static gra_t *selgra;
static int cursor[2] = {4, 4}; // start on Sauron
static bool chosen[2];
static bool joined[2] = {true, false}; // the right player joins by pressing a button
static int seltime, launch;
static int palcache[512];

void SEL_Start(void)
{
    if (!selgra) {
        selgra = W_LoadGra("S04SELMO.GRA");
        for (int i = 0; i < 512; i++)
            palcache[i] = -1;
    }
    chosen[0] = chosen[1] = false;
    seltime = launch = 0;
    gamestate = GS_SELECT;
}

static void SEL_DrawFrame(int frame, int x, int y, int flags, float scale)
{
    patch_t *p = W_Frame(selgra, frame);
    if (!p)
        return;
    if (palcache[frame] < 0)
        palcache[frame] = W_GuessPalette(p, selgra);
    V_DrawPatchScaled(x, y, p, &selgra->palettes[palcache[frame]], flags, scale, scale);
}

bool SEL_Responder(const event_t *ev)
{
    int pl, bit;
    if (ev->type != ev_keydown || !G_Binding(ev->key, &pl, &bit))
        return false;
    if (!joined[pl]) {
        if (bit & (BT_ATTACKS | BT_START)) {
            joined[pl] = true;
            S_StartSound(sfx_select, -1, 0.8f);
        }
        return true;
    }
    if (chosen[pl])
        return true;
    if (bit & (BT_LEFT | BT_UP))
        cursor[pl] = (cursor[pl] + numroster - 1) % numroster;
    if (bit & (BT_RIGHT | BT_DOWN))
        cursor[pl] = (cursor[pl] + 1) % numroster;
    if ((bit & (BT_ATTACKS | BT_START)) && P_Playable(roster[cursor[pl]])) {
        chosen[pl] = true;
        S_StartSound(sfx_select, -1, 0.8f);
    }
    return true;
}

void SEL_Ticker(void)
{
    seltime++;
    // a lone player fights the CPU, which picks as soon as the player has
    if (chosen[0] && !joined[1] && !chosen[1]) {
        int playable[16], n = 0;
        for (int i = 0; i < numroster; i++)
            if (P_Playable(roster[i]))
                playable[n++] = i;
        cursor[1] = playable[M_Random() % n];
        chosen[1] = true;
    }
    if (chosen[0] && chosen[1] && ++launch == LAUNCH_DELAY) {
        bool cpu[2] = {false, !joined[1]};
        G_InitNew(roster[cursor[0]], roster[cursor[1]], cpu, &stages[M_Random() % numstages]);
    }
}

static void SEL_Box(int x, int y, int size, pixel_t c)
{
    V_FillRect(x, y, size, 1, c);
    V_FillRect(x, y + size - 1, size, 1, c);
    V_FillRect(x, y, 1, size, c);
    V_FillRect(x + size - 1, y, 1, size, c);
}

// A creature's stand pose, scaled to fit a w x h box, feet on its bottom edge.
static void SEL_DrawCreature(const fighterdef_t *d, int x, int y, int w, int h, bool mirror)
{
    const state_t *st = &d->states[d->anims[ANIM_STAND].first];
    patch_t *p = P_StatePatch(st);
    if (!p)
        return;
    V_FillRect(x, y, w, h, 0xff101418);
    float sc = MIN((float)(w - 4) / p->w, (float)(h - 4) / p->h);
    bool flip = d->facesleft == !mirror; // face the middle of the screen
    int px = (int)(x + w / 2 + (flip ? -1 : 1) * (p->x - p->w / 2.f) * sc);
    V_DrawPatchScaled(px, y + h - 2, p, &d->base->palettes[d->palette], flip ? V_FLIP : 0, sc, sc);
}

void SEL_Drawer(void)
{
    V_Clear(0xff000000);
    patch_t *bg = W_Frame(selgra, BACKDROP);
    SEL_DrawFrame(BACKDROP, (SCREENWIDTH - bg->w) / 2 + bg->x, bg->y, 0, 1);
    V_FillRect(0, 150, SCREENWIDTH, SCREENHEIGHT - 150, 0xff000000);
    V_DrawTextCentered(FONT_SMALL, SCREENWIDTH / 2, 4, "SELECT CHARACTER");

    // the two players' current picks, big
    for (int pl = 0; pl < 2; pl++) {
        int cx = pl ? 262 : 58;
        if (!joined[pl]) {
            if (seltime & 32) {
                V_DrawTextCentered(FONT_SMALL, cx, 70, pl ? "RIGHT PLAYER" : "LEFT PLAYER");
                V_DrawTextCentered(FONT_SMALL, cx, 80, "PRESS A BUTTON");
                V_DrawTextCentered(FONT_SMALL, cx, 90, "OR FIGHT THE CPU");
            }
            if (!chosen[pl])
                continue;
        }
        const fighterdef_t *d = roster[cursor[pl]];
        if (d->portrait < 0) { // no portrait art (photo creatures): show the creature itself
            int x = pl ? SCREENWIDTH - 6 - 110 : 6;
            SEL_DrawCreature(d, x, 24, 110, 113, pl == 1);
            V_DrawTextCentered(FONT_BIG, x + 55, 24 + 113 + 4, d->name);
            continue;
        }
        int frame = d->portrait + (chosen[pl] ? 4 : seltime / 10 % 4);
        patch_t *p = W_Frame(selgra, frame);
        int x = pl ? SCREENWIDTH - 6 - p->w : 6;
        SEL_DrawFrame(frame, x + p->x, 24 + p->y, pl ? V_FLIP : 0, 1);
        patch_t *lab = W_Frame(selgra, d->label);
        if (lab)
            SEL_DrawFrame(d->label, x + (p->w - lab->w) / 2 + lab->x, 24 + p->h + 2 + lab->y, 0, 1);
        if (!P_Playable(d))
            V_DrawTextCentered(FONT_SMALL, x + p->w / 2, 74, "COMING SOON");
    }

    // roster strip with cursors; cells shrink to fit everyone
    const int cell = MIN(44, (SCREENWIDTH - 4) / numroster), size = cell - 4, y = 156;
    int x0 = (SCREENWIDTH - cell * numroster) / 2;
    for (int i = 0; i < numroster; i++) {
        int x = x0 + i * cell + 2;
        if (roster[i]->portrait >= 0)
            SEL_DrawFrame(roster[i]->portrait, x, y, P_Playable(roster[i]) ? 0 : V_FADE, size / 110.f);
        else
            SEL_DrawCreature(roster[i], x, y, size, size, false);
        for (int pl = 0; pl < 2; pl++) {
            if (cursor[pl] != i || (!joined[pl] && !chosen[pl]) || (!chosen[pl] && (seltime & 8)))
                continue;
            SEL_Box(x - 2 + pl * 2, y - 2 + pl * 2, size + 2 - pl * 4, pl ? 0xff3070f0 : 0xfff04030);
        }
    }
    if (chosen[0] && chosen[1])
        V_DrawTextCentered(FONT_BIG, SCREENWIDTH / 2, 132, "FIGHT!");
}
