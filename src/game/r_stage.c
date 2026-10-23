// r_stage.c -- arena backgrounds (cf. r_sky.c / r_plane.c)
//
// Stage files hold a wide sky panorama plus loose scenery sprites. Their
// placement is in game code we don't have yet, so the layout is rebuilt
// here: sky with parallax, a few props on the horizon, and a shaded floor
// with perspective lines so camera movement reads as depth.

#include <math.h>

#include "game/game.h"

// Names are the arcade arenas; the file mapping is a best guess.
stage_t stages[] = {
    {.file = "S04BEACH.GRA", .name = "THE COVE"},
    {.file = "S04VOLCN.GRA", .name = "THE INFERNO"},
    {.file = "S04CITYS.GRA", .name = "THE STRIP"},
    {.file = "S04GRAVE.GRA", .name = "THE TOMB"},
    {.file = "S04HIMAL.GRA", .name = "THE CLIFF"},
    {.file = "S04STONE.GRA", .name = "THE RUINS"},
    {.file = "S04JUNGL.GRA", .name = "THE HOLLOWS"},
};
int numstages = sizeof stages / sizeof stages[0];

void R_PrepareStage(stage_t *st)
{
    if (st->prepared)
        return;
    gra_t *gra = W_LoadGra(st->file);
    int area = 0;
    for (int i = 0; i < gra->numframes; i++) {
        patch_t *p = W_Frame(gra, i);
        if (p && p->w >= SCREENWIDTH && p->w * p->h > area) {
            area = p->w * p->h;
            st->sky = p;
        }
    }
    if (st->sky)
        st->skypal = &gra->palettes[W_GuessPalette(st->sky, gra)];

    // props: bottom-anchored scenery of a sensible size
    static const float slots[MAXPROPS] = {0.12f, 0.5f, 0.88f};
    for (int i = 0; i < gra->numframes && st->numprops < MAXPROPS; i++) {
        patch_t *p = W_Frame(gra, i);
        if (!p || p->w < 50 || p->w >= 260 || p->h < 40 || p->h > 130 || p->y < p->h - 8)
            continue;
        int k = st->numprops++;
        st->props[k] = p;
        st->proppal[k] = &gra->palettes[W_GuessPalette(p, gra)];
        st->propx[k] = slots[k] * (ARENA_WIDTH + 120) - 60;
    }

    // floor colours: average the bottom row of the sky for the horizon tint
    pixel_t horizon = W_RGB(90, 70, 60);
    if (st->sky) {
        long r = 0, g = 0, b = 0, n = 0;
        const patch_t *s = st->sky;
        for (int x = 0; x < s->w; x += 3) {
            int v = s->pixels[(s->h - 2) * s->w + x];
            if (!v)
                continue;
            pixel_t c = st->skypal->colors[v];
            r += (c >> 16) & 0xff;
            g += (c >> 8) & 0xff;
            b += c & 0xff;
            n++;
        }
        if (n)
            horizon = W_RGB(r / n, g / n, b / n);
    }
    st->floortop = V_Lerp(horizon, W_RGB(40, 30, 30), 0.45f);
    st->floorbottom = V_Lerp(horizon, W_RGB(10, 8, 8), 0.85f);
    st->floorline = V_Lerp(horizon, W_RGB(0, 0, 0), 0.65f);
    st->prepared = true;
}

void R_DrawStage(const stage_t *st, int camx)
{
    float t = (float)camx / (ARENA_WIDTH - SCREENWIDTH);

    V_VGradient(0, HORIZON_Y, W_RGB(20, 10, 30), W_RGB(60, 30, 50));
    if (st->sky) {
        const patch_t *s = st->sky;
        int x = -(int)lroundf(t * MAX(0, s->w - SCREENWIDTH));
        V_DrawPatch(x + s->x, HORIZON_Y - s->h + s->y, s, st->skypal, 0);
    }
    for (int i = 0; i < st->numprops; i++)
        V_DrawPatch((int)lroundf(st->propx[i] - camx * 0.75f), HORIZON_Y + 6, st->props[i], st->proppal[i], 0);

    // floor: gradient plus receding lines anchored to the camera
    int fh = SCREENHEIGHT - HORIZON_Y;
    V_VGradient(HORIZON_Y, fh, st->floortop, st->floorbottom);
    float vx = SCREENWIDTH / 2.f;
    for (int wx = -ARENA_WIDTH; wx <= ARENA_WIDTH * 2; wx += 48) {
        float bottom = (float)(wx - camx); // where the line meets the bottom edge
        for (int y = HORIZON_Y + 2; y < SCREENHEIGHT; y += 2) {
            float k = (float)(y - HORIZON_Y) / fh;
            V_FillRect((int)lroundf(vx + (bottom - vx) * k), y, 1, 1, st->floorline);
        }
    }
    for (int i = 1; i < 7; i++)
        V_FillRect(0, HORIZON_Y + fh * i * i / 49, SCREENWIDTH, 1, st->floorline);
}
