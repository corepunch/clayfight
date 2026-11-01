// Exercise framebuffer density, photo detail and legacy sprite sampling
// without SDL, proprietary data, or a window.
#include <assert.h>
#include <stdio.h>

#include "game/resources.h"

// Font loading is unused in these drawing tests.
gra_t *W_LoadGra(const char *name) { return NULL; }
patch_t *W_Frame(gra_t *gra, int frame) { return NULL; }
pixel_t W_RGB(int r, int g, int b) { return 0xff000000u | r << 16 | g << 8 | b; }

static pixel_t at(int x, int y) { return screen[y * SCREENWIDTH + x]; }

int main(void)
{
    const pixel_t bg = 0xff102030;
    palette_t pal = {0};
    for (int i = 1; i <= 16; i++)
        pal.colors[i] = 0xff000000u | i;
    byte pixels[] = {0, 1, 2, 3};
    patch_t p = {.w = 2, .h = 2, .x = 1, .y = 1, .pixels = pixels};

    V_Clear(bg);
    V_FillRect(-1, -1, 2, 2, pal.colors[1]);
    assert(at(0, 0) == pal.colors[1] && at(1, 1) == pal.colors[1]);
    assert(at(2, 1) == bg && at(1, 2) == bg);
    V_Clear(bg);
    V_DrawPatch(1, 1, &p, &pal, 0);
    assert(at(0, 0) == bg); // transparent source pixel
    assert(at(2, 0) == pal.colors[1] && at(3, 1) == pal.colors[1]);
    assert(at(0, 2) == pal.colors[2] && at(3, 3) == pal.colors[3]);
    assert(at(4, 3) == bg);

    V_Clear(bg);
    V_DrawPatch(0, 0, &p, &pal, 0); // clip top and left, keep hotspot
    assert(at(0, 0) == pal.colors[3] && at(2, 0) == bg);
    V_Clear(bg);
    V_DrawPatch(0, 0, &p, &pal, V_FLIP);
    assert(at(0, 0) == pal.colors[2]);

    byte detail[16];
    for (int i = 0; i < 16; i++)
        detail[i] = i + 1;
    p.detail = detail;
    V_Clear(bg);
    V_DrawPatch(1, 1, &p, &pal, 0);
    for (int y = 0; y < 4; y++)
        for (int x = 0; x < 4; x++)
            assert(at(x, y) == pal.colors[1 + y * 4 + x]);
    assert(at(4, 3) == bg); // same bounds as the legacy patch

    V_DrawPatch(1, 1, &p, &pal, V_FLIP);
    assert(at(0, 0) == pal.colors[4] && at(3, 3) == pal.colors[13]);
    V_Clear(bg);
    V_DrawPatchScaled(1, 1, &p, &pal, 0, 0.5f, 0.5f);
    assert(at(1, 1) == pal.colors[1] && at(2, 2) == pal.colors[11]);
    assert(at(3, 2) == bg);
    V_DrawPatch(1, 1, &p, &pal, V_FLASH);
    assert(at(0, 0) == 0xffffffff && at(3, 3) == 0xffffffff);
    V_Clear(bg);
    V_DrawPatch(-1000, 0, &p, &pal, 0);
    V_DrawPatchScaled(1, 1, &p, &pal, 0, 0, 1);
    assert(at(0, 0) == bg);
    puts("video: legacy 2x, native detail, clipping, hotspot, flip, scale, flash passed");
    return 0;
}
