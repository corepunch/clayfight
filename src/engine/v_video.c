// v_video.c -- software framebuffer, patch and text drawing (cf. v_video.c)

#include <ctype.h>
#include <math.h>
#include <string.h>

#include "engine/engine.h"

pixel_t screen[SCREENWIDTH * SCREENHEIGHT];

void V_Clear(pixel_t color)
{
    for (int i = 0; i < SCREENWIDTH * SCREENHEIGHT; i++)
        screen[i] = color;
}

void V_FillRect(int x, int y, int w, int h, pixel_t color)
{
    int x0 = MAX(0, x), y0 = MAX(0, y);
    int x1 = MIN(SCREENWIDTH, x + w), y1 = MIN(SCREENHEIGHT, y + h);
    for (int yy = y0; yy < y1; yy++)
        for (int xx = x0; xx < x1; xx++)
            screen[yy * SCREENWIDTH + xx] = color;
}

pixel_t V_Lerp(pixel_t a, pixel_t b, float t)
{
    int c[3];
    for (int k = 0; k < 3; k++) {
        int s = 16 - k * 8;
        c[k] = (int)lroundf(((a >> s) & 0xff) * (1 - t) + ((b >> s) & 0xff) * t);
    }
    return W_RGB(c[0], c[1], c[2]);
}

void V_VGradient(int y, int h, pixel_t top, pixel_t bottom)
{
    for (int i = 0; i < h; i++)
        V_FillRect(0, y + i, SCREENWIDTH, 1, V_Lerp(top, bottom, h > 1 ? (float)i / (h - 1) : 0));
}

static pixel_t blend(pixel_t dst, pixel_t src)
{
    return 0xff000000u | (((dst & 0xfefefe) >> 1) + ((src & 0xfefefe) >> 1));
}

void V_DrawPatchScaled(int x, int y, const patch_t *p, const palette_t *pal, int flags, float sx, float sy)
{
    if (!p)
        return;
    int dw = (int)lroundf(p->w * sx), dh = (int)lroundf(p->h * sy);
    bool flip = flags & V_FLIP;
    int left = (int)lroundf(flip ? x + p->x * sx - dw : x - p->x * sx);
    int top = (int)lroundf(y - p->y * sy);
    int cx0 = MAX(0, -left), cx1 = MIN(dw, SCREENWIDTH - left);
    int cy0 = MAX(0, -top), cy1 = MIN(dh, SCREENHEIGHT - top);
    for (int dy = cy0; dy < cy1; dy++) {
        const byte *srow = p->pixels + MIN(p->h - 1, (int)(dy / sy)) * p->w;
        pixel_t *d = screen + (top + dy) * SCREENWIDTH + left + cx0;
        for (int dx = cx0; dx < cx1; dx++, d++) {
            int px = MIN(p->w - 1, (int)(dx / sx));
            if (flip)
                px = p->w - 1 - px;
            int v = srow[px];
            if (!v)
                continue;
            pixel_t c = pal && pal->colors[v] ? pal->colors[v] : 0xffff00ff;
            if (flags & V_SHADOW)
                *d = blend(*d, 0xff000000);
            else if (flags & V_FLASH)
                *d = 0xffffffff;
            else if (flags & V_FADE)
                *d = blend(*d, c);
            else
                *d = c;
        }
    }
}

void V_DrawPatch(int x, int y, const patch_t *p, const palette_t *pal, int flags)
{
    V_DrawPatchScaled(x, y, p, pal, flags, 1, 1);
}

// --- text, using the two complete alphabets in S04FONTS.GRA ------------------

static gra_t *fonts;
static short smallmap[128], bigmap[128];

static void map_run(short *map, const char *chars, int first)
{
    for (int i = 0; chars[i]; i++)
        map[(byte)chars[i]] = (short)(first + i);
}

void V_Init(void)
{
    fonts = W_LoadGra("S04FONTS.GRA");
    for (int i = 0; i < 128; i++)
        smallmap[i] = bigmap[i] = -1;
    // small 6px font: ASCII, with placeholder glyphs at a few slots
    map_run(smallmap, "!\"#$%&'", 111);
    map_run(smallmap, "()*+,-./", 119);
    map_run(smallmap, "01", 127);
    map_run(smallmap, "23456789:", 130);
    map_run(smallmap, "<", 139);
    map_run(smallmap, "=>?@", 142);
    map_run(smallmap, "ABCDE", 146);
    map_run(smallmap, "FGHIJKLMNO", 152);
    map_run(smallmap, "PQRSTUVWXY", 163);
    map_run(smallmap, "Z", 174);
    // big gold 11px font
    map_run(bigmap, "BCDEFGHIJKLMNOPQRSTUVWXYZ", 181);
    map_run(bigmap, "123456789", 206);
    map_run(bigmap, "0", 215);
    map_run(bigmap, "!.?", 217);
    map_run(bigmap, "A", 230);
}

static const patch_t *glyph(font_t font, char ch)
{
    int c = toupper((byte)ch) & 127;
    int i = font == FONT_BIG ? bigmap[c] : smallmap[c];
    return i >= 0 ? W_Frame(fonts, i) : NULL;
}

int V_TextWidth(font_t font, const char *text)
{
    int w = 0;
    for (; *text; text++) {
        const patch_t *p = glyph(font, *text);
        w += p ? p->w + 1 : (font == FONT_BIG ? 8 : 4);
    }
    return w;
}

void V_DrawText(font_t font, int x, int y, const char *text)
{
    const palette_t *pal = &fonts->palettes[0];
    for (; *text; text++) {
        const patch_t *p = glyph(font, *text);
        if (!p) {
            x += font == FONT_BIG ? 8 : 4;
            continue;
        }
        V_DrawPatch(x + p->x, y + p->y, p, pal, 0); // top-left at the pen
        x += p->w + 1;
    }
}

void V_DrawTextCentered(font_t font, int cx, int y, const char *text)
{
    V_DrawText(font, cx - V_TextWidth(font, text) / 2, y, text);
}
