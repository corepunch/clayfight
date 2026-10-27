// w_gra.c -- Primal Rage .GRA archive access (cf. w_wad.c)
//
// The PC port ships its data as S04*.GRA files plus an INDEX directory, the
// way Doom ships one WAD with a lump directory. A .GRA is a chain of chunks:
//
//   u16 type, u16 0x3334 ('43'), u32 next   (next = absolute offset of next chunk)
//
//   type 2  RLE pixel data for every frame (starts at file offset 8)
//   type 5  palettes: repeated { u32 count; count x (r,g,b,0) }
//   type 6  frame table to EOF, 12 bytes each:
//           u16 w, u16 h, s16 xhot, s16 yhot, u32 ptr
//           pixel offset = ((ptr >> 16) & 0x7f) << 16 | (ptr & 0xffff)
//
// Frame rows are RLE coded and always exactly w pixels wide:
//   c < 0x80       literal: c bytes follow
//   0x80 | n       run: next byte repeated n times
//   0xC0 | n       skip n transparent pixels
// Pixel values are 1-based indices into a ~31 colour sub-palette; which
// sub-palette a frame uses is decided by game code, not by the file.
//
// Sound files (S04SD*.GRA, S04xxxSD.GRA) reuse the type-2 header and then hold
// { u32 length; length bytes of unsigned 8-bit PCM } records.

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "engine/engine.h"

#define MAGIC 0x3334
#define CH_PIXELS 2
#define CH_PALETTE 5
#define CH_FRAMES 6
#define SAMPLE_RATE 11025

lumpinfo_t *lumpinfo;
int numlumps;

static char datadir[512];

#define MAXGRA 512
static gra_t *grafiles[MAXGRA];
static int numgra;

static uint16_t rd16(const byte *p) { return p[0] | p[1] << 8; }
static uint32_t rd32(const byte *p) { return p[0] | p[1] << 8 | p[2] << 16 | (uint32_t)p[3] << 24; }

pixel_t W_RGB(int r, int g, int b)
{
    return 0xff000000u | (uint32_t)CLAMP(r, 0, 255) << 16 | (uint32_t)CLAMP(g, 0, 255) << 8 | (uint32_t)CLAMP(b, 0, 255);
}

// Read a whole file from the data directory, trying upper then lower case.
static byte *W_ReadFile(const char *name, int *size)
{
    char path[640], fname[64];
    for (int pass = 0; pass < 2; pass++) {
        int i;
        for (i = 0; name[i] && i < 63; i++)
            fname[i] = pass ? tolower((byte)name[i]) : toupper((byte)name[i]);
        fname[i] = 0;
        snprintf(path, sizeof path, "%s/%s", datadir, fname);
        FILE *f = fopen(path, "rb");
        if (!f)
            continue;
        fseek(f, 0, SEEK_END);
        long n = ftell(f);
        fseek(f, 0, SEEK_SET);
        byte *buf = I_Malloc(n);
        if (fread(buf, 1, n, f) != (size_t)n)
            I_Error("W_ReadFile: short read on %s", path);
        fclose(f);
        *size = (int)n;
        return buf;
    }
    return NULL;
}

void W_Init(const char *dir)
{
    snprintf(datadir, sizeof datadir, "%s", dir);
    int size;
    byte *buf = W_ReadFile("INDEX", &size);
    if (!buf)
        I_Error("W_Init: no INDEX in '%s'.\nPoint -data at the Primal Rage PC files.", datadir);
    numlumps = size / 20;
    lumpinfo = I_Malloc(numlumps * sizeof *lumpinfo);
    for (int i = 0; i < numlumps; i++) {
        for (int k = 0; k < 12; k++)
            lumpinfo[i].name[k] = toupper(buf[i * 20 + k]);
        lumpinfo[i].size = rd32(buf + i * 20 + 12) & 0xffffff;
    }
    free(buf);
    I_Log("W_Init: %d files in %s", numlumps, datadir);
}

static void W_ParsePalettes(gra_t *g, int p, int end)
{
    // count first, then fill
    int n = 0;
    for (int q = p; q + 4 <= end; q += 4 + rd32(g->data + q) * 4)
        n++;
    g->numpalettes = n;
    g->palettes = I_Malloc(n * sizeof *g->palettes);
    for (int k = 0; k < n; k++) {
        int count = rd32(g->data + p);
        p += 4;
        palette_t *pal = &g->palettes[k];
        pal->count = MIN(count, 255);
        for (int i = 0; i < count; i++, p += 4)
            if (i < 255)
                pal->colors[i + 1] = W_RGB(g->data[p], g->data[p + 1], g->data[p + 2]);
    }
}

static gra_t *W_Parse(const char *name, byte *data, int size)
{
    gra_t *g = I_Malloc(sizeof *g);
    snprintf(g->name, sizeof g->name, "%s", name);
    g->data = data;
    g->size = size;
    int p = 0;
    for (;;) {
        if (p + 8 > size || rd16(data + p + 2) != MAGIC)
            I_Error("W: %s: bad chunk at %d", name, p);
        int type = rd16(data + p);
        int next = rd32(data + p + 4);
        if (type == CH_FRAMES) {
            g->framechunk = p;
            p += 8;
            break;
        }
        if (type == CH_PALETTE)
            W_ParsePalettes(g, p + 8, next);
        else if (type != CH_PIXELS)
            I_Error("W: %s: unknown chunk type %d", name, type);
        p = next;
    }
    g->numframes = (size - p) / 12;
    g->frames = I_Malloc(MAX(g->numframes, 1) * sizeof *g->frames);
    for (int i = 0; i < g->numframes; i++, p += 12) {
        frame_t *f = &g->frames[i];
        uint32_t ptr = rd32(data + p + 8);
        f->w = rd16(data + p);
        f->h = rd16(data + p + 2);
        f->x = (int16_t)rd16(data + p + 4);
        f->y = (int16_t)rd16(data + p + 6);
        f->offset = (int)(((ptr >> 16) & 0x7f) << 16 | (ptr & 0xffff));
    }
    return g;
}

gra_t *W_TryLoadGra(const char *name)
{
    for (int i = 0; i < numgra; i++)
        if (!strcasecmp(grafiles[i]->name, name))
            return grafiles[i];
    int size;
    byte *data = W_ReadFile(name, &size);
    if (!data)
        return NULL;
    if (numgra == MAXGRA)
        I_Error("W_TryLoadGra: too many files");
    gra_t *g = W_Parse(name, data, size);
    grafiles[numgra++] = g;
    return g;
}

gra_t *W_LoadGra(const char *name)
{
    gra_t *g = W_TryLoadGra(name);
    if (!g)
        I_Error("W_LoadGra: can't find %s in %s", name, datadir);
    return g;
}

gra_t *W_GraById(int id)
{
    return id >= 0 && id < numlumps ? W_TryLoadGra(lumpinfo[id].name) : NULL;
}

patch_t *W_Frame(gra_t *gra, int index)
{
    if (!gra || index < 0 || index >= gra->numframes)
        return NULL;
    frame_t *f = &gra->frames[index];
    if (f->decoded)
        return f->patch;
    f->decoded = true;
    if (f->w == 0 || f->h == 0 || f->w > 1024 || f->h > 1024)
        return NULL; // empty or marker entry

    patch_t *pt = I_Malloc(sizeof *pt);
    int w = f->w, h = f->h;
    pt->w = w;
    pt->h = h;
    pt->x = f->x;
    pt->y = f->y;
    pt->pixels = I_Malloc(w * h);
    const byte *src = gra->data;
    int p = f->offset;
    int x0 = w, y0 = h, x1 = -1, y1 = -1;
    for (int y = 0; y < h; y++) {
        byte *row = pt->pixels + y * w;
        int x = 0;
        while (x < w) {
            if (p >= gra->size)
                I_Error("W_Frame: %s[%d] runs past end of file", gra->name, index);
            int c = src[p++];
            int n;
            if (c < 0x80) {
                n = MIN(c, w - x);
                memcpy(row + x, src + p, n);
                p += c;
            } else if (c < 0xc0) {
                n = MIN(c & 0x3f, w - x);
                memset(row + x, src[p++], n);
            } else {
                x += c & 0x3f;
                continue;
            }
            if (n) {
                x0 = MIN(x0, x);
                x1 = MAX(x1, x + n - 1);
                y0 = MIN(y0, y);
                y1 = y;
            }
            x += n;
        }
    }
    for (int i = 0; i < w * h; i++)
        pt->maxval = MAX(pt->maxval, pt->pixels[i]);
    if (x1 >= 0) {
        pt->hasbox = true;
        pt->bx0 = x0 - f->x;
        pt->by0 = y0 - f->y;
        pt->bx1 = x1 - f->x;
        pt->by1 = y1 - f->y;
    }
    f->patch = pt;
    return pt;
}

int W_LoadSounds(const char *name, sfxdata_t **out)
{
    int size;
    byte *data = W_ReadFile(name, &size);
    *out = NULL;
    if (!data)
        return 0;
    int n = 0;
    for (int p = 8; p + 4 <= size;) {
        uint32_t len = rd32(data + p);
        if (!len || p + 4 + (int)len > size)
            break;
        n++;
        p += 4 + len;
    }
    *out = I_Malloc(MAX(n, 1) * sizeof **out);
    int k = 0;
    for (int p = 8; k < n; k++) {
        uint32_t len = rd32(data + p);
        (*out)[k] = (sfxdata_t){data + p + 4, (int)len, SAMPLE_RATE};
        p += 4 + len;
    }
    return n; // data stays allocated: the samples point into it
}

// Which sub-palette a frame uses lives in game code we don't have, so guess:
// consider the smallest palettes that cover the frame's highest index, then
// pick the one that makes neighbouring pixels most similar (real art is
// smooth; the wrong palette turns it into noise).
static int W_Roughness(const patch_t *pt, const palette_t *pal)
{
    long sum = 0, n = 0;
    int step = MAX(1, pt->w * pt->h / 6000);
    for (int i = 0; i < pt->w * pt->h - pt->w - 1; i += step) {
        int a = pt->pixels[i];
        if (!a)
            continue;
        for (int k = 0; k < 2; k++) {
            int b = pt->pixels[k ? i + pt->w : i + 1];
            if (!b)
                continue;
            pixel_t ca = pal->colors[a], cb = pal->colors[b];
            for (int s = 0; s < 24; s += 8)
                sum += abs((int)((ca >> s) & 0xff) - (int)((cb >> s) & 0xff));
            n++;
        }
    }
    return n ? (int)(sum * 16 / n) : 1 << 30;
}

int W_GuessPalette(const patch_t *pt, const gra_t *gra)
{
    if (!pt || !gra->numpalettes)
        return 0;
    int size = 1 << 30;
    for (int k = 0; k < gra->numpalettes; k++)
        if (gra->palettes[k].count >= pt->maxval)
            size = MIN(size, gra->palettes[k].count);
    int best = 0, bestscore = 1 << 30;
    for (int k = 0; k < gra->numpalettes; k++) {
        if (gra->palettes[k].count != size)
            continue;
        int s = W_Roughness(pt, &gra->palettes[k]);
        if (s < bestscore) {
            bestscore = s;
            best = k;
        }
    }
    return best;
}

int W_FramePalette(gra_t *gra, int frame)
{
    // S04SELMO object descriptors in PRAGE.EXE explicitly bind these ranges
    // to palettes. Smoothness cannot distinguish intentional colour variants.
    if (!strcasecmp(gra->name, "S04SELMO.GRA")) {
        static const struct { int first, last, pal; } mappings[] = {
            {91, 95, 4}, {97, 101, 6}, {113, 117, 11}, {125, 129, 12},
            {130, 134, 13}, {135, 137, 14}, {141, 145, 16}, {149, 153, 18},
            {118, 118, 20}, {119, 119, 21}, {120, 120, 22}, {121, 121, 23},
            {122, 122, 24}, {123, 123, 25}, {124, 124, 26},
        };
        for (unsigned i = 0; i < sizeof mappings / sizeof mappings[0]; i++)
            if (frame >= mappings[i].first && frame <= mappings[i].last)
                return mappings[i].pal;
    }
    return W_GuessPalette(W_Frame(gra, frame), gra);
}
