// w_photo.c -- turn photographs of a clay figure into a sprite file
//
// Per photo, at a working size of at most WORKSIZE pixels:
//  1. background model: the empty backdrop shot if there is one, otherwise a
//     smooth (quadratic) colour surface fitted to the photo's border, which
//     follows the light falling off across a paper sweep;
//  2. difference: mostly colour (chroma), plus brightness -- weighted down
//     where the pixel is darker than the paper, so shadows stay background;
//  3. Otsu threshold, then keep the main blob (and big pieces near it) and
//     fill small holes.
// Then for the whole set: one scale (the reference frame's figure becomes
// `height` pixels tall), area-filtered resampling, a 31-colour median-cut
// palette shared by every frame, and the hotspot at the feet.

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "pr_def.h"
#include "i_system.h"
#include "w_photo.h"

#define WORKSIZE 480
#define MAXPHOTOS 64
#define NUMCOLORS 31

typedef struct {
    int w, h;
    float *rgb; // w*h*3, 0..255
} image_t;

typedef struct {
    char path[512];
    image_t img;
    byte *mask; // 1 = figure
    int x0, y0, x1, y1; // figure bounds
} photo_t;

struct photoset_s {
    char name[32];
    gra_t *gra;
    photo_t photos[MAXPHOTOS];
    int count;
};

photoset_t *W_PhotoBegin(const char *name)
{
    photoset_t *ps = I_Malloc(sizeof *ps);
    snprintf(ps->name, sizeof ps->name, "%s", name);
    ps->gra = I_Malloc(sizeof *ps->gra);
    snprintf(ps->gra->name, sizeof ps->gra->name, "photo:%.9s", name);
    ps->gra->frames = I_Malloc(MAXPHOTOS * sizeof *ps->gra->frames);
    return ps;
}

gra_t *W_PhotoGra(photoset_t *ps)
{
    return ps->gra;
}

int W_PhotoFrame(photoset_t *ps, const char *path)
{
    for (int i = 0; i < ps->count; i++)
        if (!strcmp(ps->photos[i].path, path))
            return i;
    if (ps->count == MAXPHOTOS)
        I_Error("%s: more than %d photos", ps->name, MAXPHOTOS);
    snprintf(ps->photos[ps->count].path, sizeof ps->photos[0].path, "%s", path);
    ps->gra->numframes = ps->count + 1;
    return ps->count++;
}

// Load a PNG and box-filter it down to the working size.
static image_t W_LoadWorkImage(const char *path)
{
    char err[600];
    int w, h;
    byte *rgba = W_LoadPNG(path, &w, &h, err, sizeof err);
    if (!rgba)
        I_Error("%s", err);
    int f = MAX(1, (MAX(w, h) + WORKSIZE - 1) / WORKSIZE);
    image_t img = {w / f, h / f, NULL};
    img.rgb = I_Malloc((size_t)img.w * img.h * 3 * sizeof(float));
    for (int y = 0; y < img.h; y++) {
        for (int x = 0; x < img.w; x++) {
            float acc[3] = {0};
            for (int dy = 0; dy < f; dy++)
                for (int dx = 0; dx < f; dx++)
                    for (int c = 0; c < 3; c++)
                        acc[c] += rgba[(((size_t)y * f + dy) * w + x * f + dx) * 4 + c];
            for (int c = 0; c < 3; c++)
                img.rgb[((size_t)y * img.w + x) * 3 + c] = acc[c] / (f * f);
        }
    }
    free(rgba);
    return img;
}

// Least-squares quadratic surface c0 + c1 x + c2 y + c3 x^2 + c4 xy + c5 y^2
// per channel, fitted to a band around the image border.
static void W_FitBorder(const image_t *img, float *bg)
{
    double ata[6][6] = {{0}}, atb[3][6] = {{0}};
    int band = MAX(3, MIN(img->w, img->h) / 25);
    for (int y = 0; y < img->h; y++) {
        for (int x = 0; x < img->w; x++) {
            if (x >= band && x < img->w - band && y >= band && y < img->h - band)
                continue;
            double u = (double)x / img->w - 0.5, v = (double)y / img->h - 0.5;
            double t[6] = {1, u, v, u * u, u * v, v * v};
            const float *px = img->rgb + ((size_t)y * img->w + x) * 3;
            for (int i = 0; i < 6; i++) {
                for (int j = 0; j < 6; j++)
                    ata[i][j] += t[i] * t[j];
                for (int c = 0; c < 3; c++)
                    atb[c][i] += t[i] * px[c];
            }
        }
    }
    double coef[3][6];
    for (int c = 0; c < 3; c++) { // Gaussian elimination on a copy
        double m[6][7];
        for (int i = 0; i < 6; i++) {
            memcpy(m[i], ata[i], sizeof ata[i]);
            m[i][6] = atb[c][i];
        }
        for (int col = 0; col < 6; col++) {
            int piv = col;
            for (int r = col + 1; r < 6; r++)
                if (fabs(m[r][col]) > fabs(m[piv][col]))
                    piv = r;
            for (int k = 0; k < 7; k++) {
                double tmp = m[col][k];
                m[col][k] = m[piv][k];
                m[piv][k] = tmp;
            }
            if (fabs(m[col][col]) < 1e-12)
                continue;
            for (int r = 0; r < 6; r++) {
                if (r == col)
                    continue;
                double k = m[r][col] / m[col][col];
                for (int j = col; j < 7; j++)
                    m[r][j] -= k * m[col][j];
            }
        }
        for (int i = 0; i < 6; i++)
            coef[c][i] = fabs(m[i][i]) > 1e-12 ? m[i][6] / m[i][i] : 0;
    }
    for (int y = 0; y < img->h; y++) {
        for (int x = 0; x < img->w; x++) {
            double u = (double)x / img->w - 0.5, v = (double)y / img->h - 0.5;
            double t[6] = {1, u, v, u * u, u * v, v * v};
            for (int c = 0; c < 3; c++) {
                double s = 0;
                for (int i = 0; i < 6; i++)
                    s += coef[c][i] * t[i];
                bg[((size_t)y * img->w + x) * 3 + c] = (float)s;
            }
        }
    }
}

// How different from the background a pixel looks, 0..255.
static float W_Difference(const float *px, const float *bg)
{
    float l = (px[0] + px[1] + px[2]) / 3 + 1, lb = (bg[0] + bg[1] + bg[2]) / 3 + 1;
    float dc = 0;
    for (int c = 0; c < 3; c++)
        dc += fabsf(px[c] / l - bg[c] / lb);
    float dl = l - lb;
    // shadows are darker paper of the same colour: count darkening lightly
    float d = dc * 160 + (dl > 0 ? dl : -dl * 0.3f);
    return MIN(d, 255.f);
}

static int W_Otsu(const int *hist, int n)
{
    long total = 0, sum = 0;
    for (int i = 0; i < n; i++) {
        total += hist[i];
        sum += (long)i * hist[i];
    }
    long wb = 0, sumb = 0;
    double best = -1;
    int thresh = n / 2;
    for (int t = 0; t < n; t++) {
        wb += hist[t];
        if (!wb || wb == total)
            continue;
        sumb += (long)t * hist[t];
        double mb = (double)sumb / wb, mf = (double)(sum - sumb) / (total - wb);
        double between = (double)wb * (total - wb) * (mb - mf) * (mb - mf);
        if (between > best) {
            best = between;
            thresh = t;
        }
    }
    return thresh;
}

// Label 4-connected regions of value `val`; returns the label count.
static int W_Label(const byte *mask, int w, int h, byte val, int *labels, int *areas, int maxlabels)
{
    int n = 0;
    int *stack = I_Malloc((size_t)w * h * sizeof *stack);
    for (int i = 0; i < w * h; i++)
        labels[i] = -1;
    for (int i = 0; i < w * h; i++) {
        if (mask[i] != val || labels[i] >= 0)
            continue;
        int id = n < maxlabels ? n++ : maxlabels - 1, sp = 0, area = 0;
        stack[sp++] = i;
        labels[i] = id;
        while (sp) {
            int p = stack[--sp], x = p % w, y = p / w;
            area++;
            int nb[4] = {x > 0 ? p - 1 : -1, x < w - 1 ? p + 1 : -1, y > 0 ? p - w : -1, y < h - 1 ? p + w : -1};
            for (int k = 0; k < 4; k++) {
                if (nb[k] >= 0 && mask[nb[k]] == val && labels[nb[k]] < 0) {
                    labels[nb[k]] = id;
                    stack[sp++] = nb[k];
                }
            }
        }
        areas[id] += area;
    }
    free(stack);
    return n;
}

static void W_Segment(photo_t *ph, const image_t *backdrop)
{
    const image_t *img = &ph->img;
    int w = img->w, h = img->h, n = w * h;
    float *bg = I_Malloc((size_t)n * 3 * sizeof(float));
    if (backdrop && backdrop->w == w && backdrop->h == h)
        memcpy(bg, backdrop->rgb, (size_t)n * 3 * sizeof(float));
    else
        W_FitBorder(img, bg);

    byte *diff = I_Malloc(n);
    int hist[256] = {0};
    for (int i = 0; i < n; i++) {
        diff[i] = (byte)W_Difference(img->rgb + i * 3, bg + i * 3);
        hist[diff[i]]++;
    }
    // hysteresis: confident pixels seed the figure, which then grows into
    // weaker differences touching it -- white eyes or pale horns on white
    // paper are only a little brighter than the paper, but they're attached
    int strong = MAX(W_Otsu(hist, 256), 24), weak = MAX(strong * 3 / 10, 10);
    ph->mask = I_Malloc(n);
    byte *m = ph->mask;
    int *stack = I_Malloc((size_t)n * sizeof *stack), sp = 0;
    for (int i = 0; i < n; i++)
        if ((m[i] = diff[i] > strong))
            stack[sp++] = i;
    while (sp) {
        int p = stack[--sp], x = p % w, y = p / w;
        int nb[4] = {x > 0 ? p - 1 : -1, x < w - 1 ? p + 1 : -1, y > 0 ? p - w : -1, y < h - 1 ? p + w : -1};
        for (int k = 0; k < 4; k++) {
            if (nb[k] >= 0 && !m[nb[k]] && diff[nb[k]] > weak) {
                m[nb[k]] = 1;
                stack[sp++] = nb[k];
            }
        }
    }
    free(stack);
    // 3x3 majority filter: drops sensor noise, smooths the outline
    byte *tmp = I_Malloc(n);
    for (int y = 0; y < h; y++) {
        for (int x = 0; x < w; x++) {
            int c = 0;
            for (int dy = -1; dy <= 1; dy++)
                for (int dx = -1; dx <= 1; dx++)
                    c += m[CLAMP(y + dy, 0, h - 1) * w + CLAMP(x + dx, 0, w - 1)];
            tmp[y * w + x] = c >= 5;
        }
    }
    memcpy(m, tmp, n);

    // keep the biggest blob and anything at least 6% of its size
    int *labels = I_Malloc((size_t)n * sizeof *labels), areas[4096] = {0};
    int nl = W_Label(m, w, h, 1, labels, areas, 4096), big = 0;
    for (int i = 1; i < nl; i++)
        if (areas[i] > areas[big])
            big = i;
    for (int i = 0; i < n; i++)
        m[i] = labels[i] >= 0 && areas[labels[i]] * 16 >= areas[big];
    // fill holes (background islands that don't touch the border) if small
    memset(areas, 0, sizeof areas);
    nl = W_Label(m, w, h, 0, labels, areas, 4096);
    byte *touches = I_Malloc(nl + 1);
    for (int x = 0; x < w; x++) {
        if (labels[x] >= 0) touches[labels[x]] = 1;
        if (labels[(h - 1) * w + x] >= 0) touches[labels[(h - 1) * w + x]] = 1;
    }
    for (int y = 0; y < h; y++) {
        if (labels[y * w] >= 0) touches[labels[y * w]] = 1;
        if (labels[y * w + w - 1] >= 0) touches[labels[y * w + w - 1]] = 1;
    }
    for (int i = 0; i < n; i++)
        if (labels[i] >= 0 && !touches[labels[i]] && areas[labels[i]] * 300 < n)
            m[i] = 1;

    ph->x0 = w;
    ph->y0 = h;
    ph->x1 = ph->y1 = -1;
    for (int y = 0; y < h; y++) {
        for (int x = 0; x < w; x++) {
            if (!m[y * w + x])
                continue;
            ph->x0 = MIN(ph->x0, x);
            ph->x1 = MAX(ph->x1, x);
            ph->y0 = MIN(ph->y0, y);
            ph->y1 = MAX(ph->y1, y);
        }
    }
    if (ph->x1 < 0)
        I_Error("%s: couldn't find the figure (is the background plain?)", ph->path);
    free(touches);
    free(labels);
    free(tmp);
    free(diff);
    free(bg);
}

// --- palette: median cut over every frame's pixels --------------------------------

typedef struct {
    byte (*px)[3];
    int count;
} cbox_t;

static int W_Longest(const cbox_t *b, int *range)
{
    int lo[3] = {255, 255, 255}, hi[3] = {0, 0, 0};
    for (int i = 0; i < b->count; i++)
        for (int c = 0; c < 3; c++) {
            lo[c] = MIN(lo[c], b->px[i][c]);
            hi[c] = MAX(hi[c], b->px[i][c]);
        }
    int axis = 0;
    for (int c = 1; c < 3; c++)
        if (hi[c] - lo[c] > hi[axis] - lo[axis])
            axis = c;
    *range = hi[axis] - lo[axis];
    return axis;
}

static int sortaxis;
static int W_CmpAxis(const void *a, const void *b)
{
    return ((const byte *)a)[sortaxis] - ((const byte *)b)[sortaxis];
}

static void W_MedianCut(byte (*px)[3], int count, pixel_t *out, int ncolors)
{
    cbox_t boxes[NUMCOLORS] = {{px, count}};
    int n = 1;
    while (n < ncolors) {
        int pick = -1, best = 0, axis = 0;
        for (int i = 0; i < n; i++) { // split the box with the widest spread (weighted by size)
            int range, a = W_Longest(&boxes[i], &range);
            if (boxes[i].count > 1 && range * (long)boxes[i].count > best) {
                best = (int)(range * (long)boxes[i].count);
                pick = i;
                axis = a;
            }
        }
        if (pick < 0)
            break;
        sortaxis = axis;
        qsort(boxes[pick].px, boxes[pick].count, 3, W_CmpAxis);
        int half = boxes[pick].count / 2;
        boxes[n] = (cbox_t){boxes[pick].px + half, boxes[pick].count - half};
        boxes[pick].count = half;
        n++;
    }
    for (int i = 0; i < ncolors; i++) {
        long s[3] = {0};
        int c = i < n ? boxes[i].count : 0;
        for (int k = 0; k < c; k++)
            for (int ch = 0; ch < 3; ch++)
                s[ch] += boxes[i].px[k][ch];
        out[i] = c ? W_RGB((int)(s[0] / c), (int)(s[1] / c), (int)(s[2] / c)) : W_RGB(0, 0, 0);
    }
}

static int W_Nearest(const pixel_t *pal, int n, int r, int g, int b)
{
    int best = 0, bestd = 1 << 30;
    for (int i = 0; i < n; i++) {
        int dr = (int)((pal[i] >> 16) & 0xff) - r, dg = (int)((pal[i] >> 8) & 0xff) - g, db = (int)(pal[i] & 0xff) - b;
        int d = dr * dr * 3 + dg * dg * 4 + db * db * 2;
        if (d < bestd) {
            bestd = d;
            best = i;
        }
    }
    return best;
}

// --- the whole set ---------------------------------------------------------------

typedef struct {
    int w, h;
    float *rgb; // resampled colour
    byte *on; // figure pixels
} sprite_t;

void W_PhotoFinish(photoset_t *ps, const char *backdroppath, int reference, int height)
{
    image_t backdrop = {0};
    if (backdroppath)
        backdrop = W_LoadWorkImage(backdroppath);
    for (int i = 0; i < ps->count; i++) {
        photo_t *ph = &ps->photos[i];
        ph->img = W_LoadWorkImage(ph->path);
        W_Segment(ph, backdroppath ? &backdrop : NULL);
    }
    const photo_t *ref = &ps->photos[CLAMP(reference, 0, ps->count - 1)];
    float scale = (float)height / (ref->y1 - ref->y0 + 1);

    // resample each figure to game size, averaging only figure pixels
    sprite_t *spr = I_Malloc(ps->count * sizeof *spr);
    long total = 0;
    for (int i = 0; i < ps->count; i++) {
        const photo_t *ph = &ps->photos[i];
        sprite_t *s = &spr[i];
        s->w = MAX(1, (int)ceilf((ph->x1 - ph->x0 + 1) * scale));
        s->h = MAX(1, (int)ceilf((ph->y1 - ph->y0 + 1) * scale));
        s->rgb = I_Malloc((size_t)s->w * s->h * 3 * sizeof(float));
        s->on = I_Malloc((size_t)s->w * s->h);
        for (int y = 0; y < s->h; y++) {
            for (int x = 0; x < s->w; x++) {
                int sx0 = ph->x0 + (int)(x / scale), sx1 = ph->x0 + (int)((x + 1) / scale);
                int sy0 = ph->y0 + (int)(y / scale), sy1 = ph->y0 + (int)((y + 1) / scale);
                sx1 = MAX(sx1, sx0 + 1);
                sy1 = MAX(sy1, sy0 + 1);
                int in = 0, all = 0;
                float acc[3] = {0};
                for (int yy = sy0; yy < sy1 && yy < ph->img.h; yy++) {
                    for (int xx = sx0; xx < sx1 && xx < ph->img.w; xx++) {
                        all++;
                        if (!ph->mask[yy * ph->img.w + xx])
                            continue;
                        in++;
                        for (int c = 0; c < 3; c++)
                            acc[c] += ph->img.rgb[((size_t)yy * ph->img.w + xx) * 3 + c];
                    }
                }
                size_t o = (size_t)y * s->w + x;
                s->on[o] = all && in * 2 >= all;
                if (in)
                    for (int c = 0; c < 3; c++)
                        s->rgb[o * 3 + c] = acc[c] / in;
                total += s->on[o];
            }
        }
    }

    // one palette for the whole creature, plus colour-swapped alternates
    byte(*px)[3] = I_Malloc(MAX(total, 1) * 3);
    long k = 0;
    for (int i = 0; i < ps->count; i++) {
        for (int p = 0; p < spr[i].w * spr[i].h; p++) {
            if (!spr[i].on[p])
                continue;
            for (int c = 0; c < 3; c++)
                px[k][c] = (byte)CLAMP((int)spr[i].rgb[p * 3 + c], 0, 255);
            k++;
        }
    }
    pixel_t colors[NUMCOLORS];
    W_MedianCut(px, (int)total, colors, NUMCOLORS);
    free(px);
    gra_t *g = ps->gra;
    g->numpalettes = 3;
    g->palettes = I_Malloc(3 * sizeof *g->palettes);
    for (int p = 0; p < 3; p++) {
        g->palettes[p].count = NUMCOLORS;
        for (int i = 0; i < NUMCOLORS; i++) {
            int r = (colors[i] >> 16) & 0xff, gg = (colors[i] >> 8) & 0xff, b = colors[i] & 0xff;
            g->palettes[p].colors[i + 1] = p == 0 ? colors[i] : p == 1 ? W_RGB(b, r, gg) : W_RGB(gg, b, r);
        }
    }

    // patches, hotspot at the feet: bottom row, centre of the lowest rows
    for (int i = 0; i < ps->count; i++) {
        sprite_t *s = &spr[i];
        patch_t *pt = I_Malloc(sizeof *pt);
        pt->w = s->w;
        pt->h = s->h;
        pt->pixels = I_Malloc((size_t)s->w * s->h);
        int bottom = 0;
        for (int p = 0; p < s->w * s->h; p++) {
            if (!s->on[p])
                continue;
            pt->pixels[p] = (byte)(1 + W_Nearest(colors, NUMCOLORS, (int)s->rgb[p * 3], (int)s->rgb[p * 3 + 1], (int)s->rgb[p * 3 + 2]));
            bottom = MAX(bottom, p / s->w);
        }
        int feet = MAX(2, s->h / 16);
        long sx = 0, n = 0;
        for (int y = MAX(0, bottom - feet); y <= bottom; y++)
            for (int x = 0; x < s->w; x++)
                if (s->on[y * s->w + x]) {
                    sx += x;
                    n++;
                }
        pt->x = n ? (int)(sx / n) : s->w / 2;
        pt->y = bottom;
        pt->maxval = NUMCOLORS;
        pt->hasbox = true;
        pt->bx0 = -pt->x;
        pt->by0 = -pt->y;
        pt->bx1 = s->w - 1 - pt->x;
        pt->by1 = s->h - 1 - pt->y;
        g->frames[i] = (frame_t){.w = s->w, .h = s->h, .x = pt->x, .y = pt->y, .decoded = true, .patch = pt};
        free(s->rgb);
        free(s->on);
        free(ps->photos[i].img.rgb);
        free(ps->photos[i].mask);
    }
    g->numframes = ps->count;
    free(spr);
    free(backdrop.rgb);
    I_Log("W_PhotoFinish: %s: %d photos, scale %.3f", ps->name, ps->count, scale);
}
