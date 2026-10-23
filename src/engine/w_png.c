// w_png.c -- a small PNG reader (inflate + PNG filters), so photographed
// frames need no libraries. Non-interlaced 8/16-bit gray, RGB, palette,
// gray+alpha and RGBA images; that covers what phones and editors write.
// The inflate follows the structure of Mark Adler's puff.c.

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "engine/engine.h"

// --- inflate (RFC 1951) ------------------------------------------------------

typedef struct {
    const byte *in;
    size_t inlen, inpos;
    uint32_t bitbuf;
    int bitcnt;
    byte *out;
    size_t outlen, outcap;
    bool error;
} inflate_t;

typedef struct {
    short count[16]; // codes of each length
    short symbol[320]; // symbols ordered by code
} huffman_t;

static int Bits(inflate_t *s, int need)
{
    uint32_t val = s->bitbuf;
    while (s->bitcnt < need) {
        if (s->inpos >= s->inlen) {
            s->error = true;
            return 0;
        }
        val |= (uint32_t)s->in[s->inpos++] << s->bitcnt;
        s->bitcnt += 8;
    }
    s->bitbuf = val >> need;
    s->bitcnt -= need;
    return (int)(val & ((1u << need) - 1));
}

static void Emit(inflate_t *s, byte b)
{
    if (s->outlen == s->outcap) {
        s->outcap = s->outcap ? s->outcap * 2 : 65536;
        byte *grown = realloc(s->out, s->outcap);
        if (!grown)
            I_Error("PNG: out of memory");
        s->out = grown;
    }
    s->out[s->outlen++] = b;
}

static int Decode(inflate_t *s, const huffman_t *h)
{
    int code = 0, first = 0, index = 0;
    for (int len = 1; len < 16; len++) {
        code |= Bits(s, 1);
        int count = h->count[len];
        if (code - count < first)
            return h->symbol[index + (code - first)];
        index += count;
        first = (first + count) << 1;
        code <<= 1;
    }
    s->error = true;
    return 0;
}

static void Construct(huffman_t *h, const short *length, int n)
{
    short offs[16];
    memset(h->count, 0, sizeof h->count);
    for (int i = 0; i < n; i++)
        h->count[length[i]]++;
    offs[1] = 0;
    for (int len = 1; len < 15; len++)
        offs[len + 1] = offs[len] + h->count[len];
    for (int i = 0; i < n; i++)
        if (length[i])
            h->symbol[offs[length[i]]++] = (short)i;
}

static const short lbase[29] = {3, 4, 5, 6, 7, 8, 9, 10, 11, 13, 15, 17, 19, 23, 27, 31, 35, 43, 51, 59, 67, 83, 99, 115, 131, 163, 195, 227, 258};
static const short lext[29] = {0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 2, 2, 2, 2, 3, 3, 3, 3, 4, 4, 4, 4, 5, 5, 5, 5, 0};
static const short dbase[30] = {1, 2, 3, 4, 5, 7, 9, 13, 17, 25, 33, 49, 65, 97, 129, 193, 257, 385, 513, 769, 1025, 1537, 2049, 3073, 4097, 6145, 8193, 12289, 16385, 24577};
static const short dext[30] = {0, 0, 0, 0, 1, 1, 2, 2, 3, 3, 4, 4, 5, 5, 6, 6, 7, 7, 8, 8, 9, 9, 10, 10, 11, 11, 12, 12, 13, 13};

static void Codes(inflate_t *s, const huffman_t *lencode, const huffman_t *distcode)
{
    for (;;) {
        int sym = Decode(s, lencode);
        if (s->error)
            return;
        if (sym < 256) {
            Emit(s, (byte)sym);
        } else if (sym == 256) {
            return;
        } else {
            sym -= 257;
            if (sym >= 29) {
                s->error = true;
                return;
            }
            int len = lbase[sym] + Bits(s, lext[sym]);
            int dsym = Decode(s, distcode);
            if (dsym >= 30) {
                s->error = true;
                return;
            }
            size_t dist = dbase[dsym] + Bits(s, dext[dsym]);
            if (dist > s->outlen) {
                s->error = true;
                return;
            }
            while (len--)
                Emit(s, s->out[s->outlen - dist]);
        }
    }
}

static void Fixed(inflate_t *s)
{
    static huffman_t lencode, distcode;
    static bool built;
    if (!built) {
        short lengths[320];
        int i = 0;
        for (; i < 144; i++) lengths[i] = 8;
        for (; i < 256; i++) lengths[i] = 9;
        for (; i < 280; i++) lengths[i] = 7;
        for (; i < 288; i++) lengths[i] = 8;
        Construct(&lencode, lengths, 288);
        for (i = 0; i < 30; i++) lengths[i] = 5;
        Construct(&distcode, lengths, 30);
        built = true;
    }
    Codes(s, &lencode, &distcode);
}

static void Dynamic(inflate_t *s)
{
    static const short order[19] = {16, 17, 18, 0, 8, 7, 9, 6, 10, 5, 11, 4, 12, 3, 13, 2, 14, 1, 15};
    short lengths[320] = {0};
    huffman_t lencode, distcode;
    int nlen = Bits(s, 5) + 257, ndist = Bits(s, 5) + 1, ncode = Bits(s, 4) + 4;
    if (nlen > 286 || ndist > 30) {
        s->error = true;
        return;
    }
    for (int i = 0; i < ncode; i++)
        lengths[order[i]] = (short)Bits(s, 3);
    Construct(&lencode, lengths, 19);
    for (int i = 0; i < nlen + ndist && !s->error;) {
        int sym = Decode(s, &lencode);
        if (sym < 16) {
            lengths[i++] = (short)sym;
            continue;
        }
        int len = 0, rep;
        if (sym == 16) {
            if (!i) {
                s->error = true;
                return;
            }
            len = lengths[i - 1];
            rep = 3 + Bits(s, 2);
        } else if (sym == 17) {
            rep = 3 + Bits(s, 3);
        } else {
            rep = 11 + Bits(s, 7);
        }
        if (i + rep > nlen + ndist) {
            s->error = true;
            return;
        }
        while (rep--)
            lengths[i++] = (short)len;
    }
    Construct(&lencode, lengths, nlen);
    Construct(&distcode, lengths + nlen, ndist);
    Codes(s, &lencode, &distcode);
}

static bool Inflate(inflate_t *s)
{
    int last;
    do {
        last = Bits(s, 1);
        int type = Bits(s, 2);
        if (type == 0) { // stored
            s->bitbuf = 0;
            s->bitcnt = 0;
            if (s->inpos + 4 > s->inlen)
                return false;
            unsigned len = s->in[s->inpos] | s->in[s->inpos + 1] << 8;
            s->inpos += 4;
            if (s->inpos + len > s->inlen)
                return false;
            while (len--)
                Emit(s, s->in[s->inpos++]);
        } else if (type == 1) {
            Fixed(s);
        } else if (type == 2) {
            Dynamic(s);
        } else {
            return false;
        }
        if (s->error)
            return false;
    } while (!last);
    return true;
}

// --- PNG -------------------------------------------------------------------------

static uint32_t be32(const byte *p) { return (uint32_t)p[0] << 24 | p[1] << 16 | p[2] << 8 | p[3]; }

static int Paeth(int a, int b, int c)
{
    int p = a + b - c, pa = abs(p - a), pb = abs(p - b), pc = abs(p - c);
    return pa <= pb && pa <= pc ? a : pb <= pc ? b : c;
}

byte *W_LoadPNG(const char *path, int *width, int *height, char *err, size_t errlen)
{
#define FAIL(...) do { snprintf(err, errlen, __VA_ARGS__); free(data); free(idat); free(z.out); return NULL; } while (0)
    byte *data = NULL, *idat = NULL;
    inflate_t z = {0};
    FILE *fp = fopen(path, "rb");
    if (!fp) {
        snprintf(err, errlen, "can't open %s", path);
        return NULL;
    }
    fseek(fp, 0, SEEK_END);
    long size = ftell(fp);
    fseek(fp, 0, SEEK_SET);
    data = I_Malloc(size + 1);
    bool ok = fread(data, 1, size, fp) == (size_t)size;
    fclose(fp);
    if (!ok || size < 8 || memcmp(data, "\x89PNG\r\n\x1a\n", 8))
        FAIL("%s is not a PNG file (convert JPEG/HEIC photos with: sips -s format png IN --out OUT.png)", path);

    int w = 0, h = 0, depth = 0, ctype = 0, interlace = 0;
    byte plte[256][3] = {{0}};
    size_t idatlen = 0;
    for (long p = 8; p + 12 <= size;) {
        uint32_t len = be32(data + p);
        const byte *type = data + p + 4, *body = data + p + 8;
        if (p + 12 + (long)len > size)
            FAIL("%s is truncated", path);
        if (!memcmp(type, "IHDR", 4) && len >= 13) {
            w = (int)be32(body);
            h = (int)be32(body + 4);
            depth = body[8];
            ctype = body[9];
            interlace = body[12];
        } else if (!memcmp(type, "PLTE", 4)) {
            memcpy(plte, body, MIN(len, sizeof plte));
        } else if (!memcmp(type, "IDAT", 4)) {
            byte *grown = realloc(idat, idatlen + len);
            if (!grown)
                FAIL("out of memory");
            idat = grown;
            memcpy(idat + idatlen, body, len);
            idatlen += len;
        } else if (!memcmp(type, "IEND", 4)) {
            break;
        }
        p += 12 + len;
    }
    static const int channels[7] = {1, 0, 3, 1, 2, 0, 4};
    if (w <= 0 || h <= 0 || w > 16384 || h > 16384 || ctype > 6 || !channels[ctype] || !idat)
        FAIL("%s: unsupported PNG", path);
    if (interlace)
        FAIL("%s is interlaced; save it without interlacing", path);
    if (depth != 8 && depth != 16 && !(ctype == 3 && depth <= 8))
        FAIL("%s: %d-bit PNGs aren't supported", path, depth);

    z.in = idat + 2; // skip the zlib header
    z.inlen = idatlen > 6 ? idatlen - 6 : 0;
    if (!Inflate(&z))
        FAIL("%s: corrupt image data", path);

    int nch = channels[ctype];
    size_t bpp = (size_t)MAX(1, nch * depth / 8); // bytes per pixel, for filtering
    size_t stride = ((size_t)w * nch * depth + 7) / 8;
    if (z.outlen < (stride + 1) * h)
        FAIL("%s: image data too short", path);
    byte *rgba = I_Malloc((size_t)w * h * 4);
    byte *prev = I_Malloc(stride), *cur = I_Malloc(stride);
    for (int y = 0; y < h; y++) {
        const byte *row = z.out + y * (stride + 1);
        int filter = row[0];
        for (size_t i = 0; i < stride; i++) {
            int a = i >= bpp ? cur[i - bpp] : 0, b = prev[i], c = i >= bpp ? prev[i - bpp] : 0;
            int x = row[1 + i];
            switch (filter) {
            case 1: x += a; break;
            case 2: x += b; break;
            case 3: x += (a + b) / 2; break;
            case 4: x += Paeth(a, b, c); break;
            default: break;
            }
            cur[i] = (byte)x;
        }
        for (int x = 0; x < w; x++) {
            byte *o = rgba + ((size_t)y * w + x) * 4;
            int s = depth == 16 ? 2 : 1; // 16-bit: keep the high byte
            const byte *px = cur + (size_t)x * nch * s;
            switch (ctype) {
            case 0: o[0] = o[1] = o[2] = px[0]; o[3] = 255; break;
            case 2: o[0] = px[0]; o[1] = px[s]; o[2] = px[2 * s]; o[3] = 255; break;
            case 3: {
                int bitpos = x * depth, v = (cur[bitpos / 8] >> (8 - depth - bitpos % 8)) & ((1 << depth) - 1);
                memcpy(o, plte[v], 3);
                o[3] = 255;
                break;
            }
            case 4: o[0] = o[1] = o[2] = px[0]; o[3] = px[s]; break;
            default: o[0] = px[0]; o[1] = px[s]; o[2] = px[2 * s]; o[3] = px[3 * s]; break;
            }
        }
        byte *t = prev;
        prev = cur;
        cur = t;
    }
    free(prev);
    free(cur);
    free(data);
    free(idat);
    free(z.out);
    *width = w;
    *height = h;
    return rgba;
#undef FAIL
}
