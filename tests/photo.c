// Transparent cutouts must never acquire their hidden background colours.
#include <assert.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "game/resources.h"

void *I_Malloc(size_t size) { void *p = calloc(1, size); assert(p); return p; }
void I_Log(const char *fmt, ...) {}
void I_Error(const char *fmt, ...) { abort(); }
pixel_t W_RGB(int r, int g, int b) { return 0xff000000u | r << 16 | g << 8 | b; }

byte *image_load_rgba(const char *path, int *w, int *h, char *err, size_t errlen)
{
    *w = *h = 12;
    byte *rgba = I_Malloc(12 * 12 * 4);
    for (int y = 0; y < 12; y++) for (int x = 0; x < 12; x++) {
        byte *p = rgba + (y * 12 + x) * 4;
        // Same cutout, different RGB in fully transparent pixels.
        memset(p, !strcmp(path, "white") ? 255 : 0, 3);
        if (x < 2 || x > 9 || y < 2 || y > 9) continue;
        p[0] = 180; p[1] = 40; p[2] = 20; p[3] = 255;
        if ((y == 4 || y == 5) && (x == 4 || x == 5)) memset(p, 255, 3); // white eye
        if (x == 7 && y == 6) p[3] = 0; // a real hole, not white clay
    }
    return rgba;
}

static gra_t *load(const char *path)
{
    photoset_t *set = W_PhotoBegin(path);
    W_PhotoFrame(set, path);
    W_PhotoFinish(set, NULL, 0, 4);
    return W_PhotoGra(set);
}

int main(void)
{
    gra_t *a = load("white"), *b = load("black");
    patch_t *p = a->frames[0].patch, *q = b->frames[0].patch;
    assert(p->w == 4 && p->h == 4 && p->detail);
    assert(p->x == q->x && p->y == q->y);
    assert(!memcmp(a->palettes, b->palettes, 3 * sizeof(palette_t)));
    assert(!memcmp(p->pixels, q->pixels, 16));
    assert(!memcmp(p->detail, q->detail, 64));
    assert(!p->detail[4 * 8 + 5]); // transparent hole survives at display density
    assert(a->palettes[0].colors[p->detail[2 * 8 + 2]] == 0xffffffff); // white eye survives
    // All boundary pixels remain clay-coloured; no white matte fringe.
    for (int y = 0; y < 8; y++) for (int x = 0; x < 8; x++) {
        if (x && y && x != 7 && y != 7) continue;
        pixel_t c = a->palettes[0].colors[p->detail[y * 8 + x]];
        assert(((c >> 16) & 255) == 180 && ((c >> 8) & 255) == 40 && (c & 255) == 20);
    }
    puts("photo: transparent RGB cannot create fringes; holes and white eyes survive");
    return 0;
}
