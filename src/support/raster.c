/* Caller-owned CPU surfaces and palette sprites. No game/resource dependencies. */
#include "support/support.h"
#define raster_min(a,b) ((a)<(b)?(a):(b))
#define raster_max(a,b) ((a)>(b)?(a):(b))
uint32_t raster_rgb(int r, int g, int b)
{
    return 0xff000000u | (uint32_t)clampi(r,0,255)<<16 | (uint32_t)clampi(g,0,255)<<8 | (uint32_t)clampi(b,0,255);
}
void raster_clear(raster dst, uint32_t color)
{
    for (int i = 0; i < dst.w * dst.h; i++)
        dst.pixels[i] = color;
}

void raster_fill(raster dst, int x, int y, int w, int h, uint32_t color)
{
    x *= dst.units;
    y *= dst.units;
    w *= dst.units;
    h *= dst.units;
    int x0 = raster_max(0, x), y0 = raster_max(0, y);
    int x1 = raster_min(dst.w, x + w), y1 = raster_min(dst.h, y + h);
    for (int yy = y0; yy < y1; yy++)
        for (int xx = x0; xx < x1; xx++)
            dst.pixels[yy * dst.w + xx] = color;
}

uint32_t raster_lerp(uint32_t a, uint32_t b, float t)
{
    int c[3];
    for (int k = 0; k < 3; k++) {
        int s = 16 - k * 8;
        c[k] = (int)lroundf(((a >> s) & 0xff) * (1 - t) + ((b >> s) & 0xff) * t);
    }
    return raster_rgb(c[0], c[1], c[2]);
}

void raster_gradient(raster dst, int y, int h, uint32_t top, uint32_t bottom)
{
    for (int i = 0; i < h; i++)
        raster_fill(dst, 0, y + i, (dst.w / dst.units), 1, raster_lerp(top, bottom, h > 1 ? (float)i / (h - 1) : 0));
}

static uint32_t raster_blend(uint32_t dst, uint32_t src)
{
    return 0xff000000u | (((dst & 0xfefefe) >> 1) + ((src & 0xfefefe) >> 1));
}

void raster_draw(raster dst, int x, int y, indexed_sprite src, const uint32_t *pal, int flags, float sx, float sy)
{
    const indexed_sprite *p = &src;
    if (dst.units <= 0 || (p->detail && p->density <= 0) || !dst.pixels || !p->pixels || p->w <= 0 || p->h <= 0 || sx <= 0 || sy <= 0)
        return;
    x *= dst.units;
    y *= dst.units;
    sx *= dst.units;
    sy *= dst.units;
    int density = p->detail ? p->density : 1;
    const uint8_t *pixels = p->detail ? p->detail : p->pixels;
    int sw = p->w * density, sh = p->h * density;
    int dw = (int)lroundf(p->w * sx), dh = (int)lroundf(p->h * sy);
    bool flip = flags & RASTER_FLIP;
    int left = (int)lroundf(flip ? x + p->x * sx - dw : x - p->x * sx);
    int top = (int)lroundf(y - p->y * sy);
    int cx0 = raster_max(0, -left), cx1 = raster_min(dw, dst.w - left);
    int cy0 = raster_max(0, -top), cy1 = raster_min(dh, dst.h - top);
    if (cx0 >= cx1 || cy0 >= cy1)
        return;
    for (int dy = cy0; dy < cy1; dy++) {
        const uint8_t *srow = pixels + raster_min(sh - 1, (int)(dy * density / sy)) * sw;
        uint32_t *d = dst.pixels + (top + dy) * dst.w + left + cx0;
        for (int dx = cx0; dx < cx1; dx++, d++) {
            int px = raster_min(sw - 1, (int)(dx * density / sx));
            if (flip)
                px = sw - 1 - px;
            int v = srow[px];
            if (!v)
                continue;
            uint32_t c = pal && pal[v] ? pal[v] : 0xffff00ff;
            if (flags & RASTER_SHADOW)
                *d = raster_blend(*d, 0xff000000);
            else if (flags & RASTER_FLASH)
                *d = 0xffffffff;
            else if (flags & RASTER_FADE)
                *d = raster_blend(*d, c);
            else
                *d = c;
        }
    }
}


#undef raster_min
#undef raster_max
