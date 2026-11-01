/* Indexed drawing and photo processing, extracted from Nile Archive 5de9dcf. */
#ifndef CLAY_SUPPORT_H
#define CLAY_SUPPORT_H
#include "platform/platform.h"
#include <math.h>
static inline int clampi(int x,int lo,int hi) { return x<lo?lo:x>hi?hi:x; }
typedef struct { uint32_t *pixels; int w, h, units; } raster;
typedef struct {
    int w, h, x, y;                /* size and hotspot in logical units */
    const uint8_t *pixels;         /* palette index 0 is transparent */
    const uint8_t *detail;         /* optional higher-density pixels */
    int density;                  /* detail pixels per logical pixel */
} indexed_sprite;
enum { RASTER_FLIP=1, RASTER_FLASH=2, RASTER_SHADOW=4, RASTER_FADE=8 };
uint32_t raster_rgb(int r, int g, int b);
uint32_t raster_lerp(uint32_t a, uint32_t b, float t);
void raster_clear(raster dst, uint32_t color);
void raster_fill(raster dst, int x, int y, int w, int h, uint32_t color);
void raster_gradient(raster dst, int y, int h, uint32_t top, uint32_t bottom);
void raster_draw(raster dst, int x, int y, indexed_sprite src,
                 const uint32_t *palette, int flags, float sx, float sy);
/* Literal filesystem path; returned RGBA storage is released with free(). */
uint8_t *image_load_rgba(const char *path, int *w, int *h, char *error, size_t capacity);

/* Photograph processing: background removal, common scale and palette. */
typedef struct {
    int w, h, x, y;
    uint8_t *pixels, *detail;
} photo_frame;
typedef struct {
    int count, density;
    photo_frame *frames;
    uint32_t colors[32];           /* transparent + 31 shared colors */
} photo_atlas;
typedef struct photo_set photo_set;
photo_set *photo_begin(const char *name);
int photo_add(photo_set *set, const char *path); /* deduplicates paths, max 64 */
/* Aborts with a diagnostic on invalid photos, matching native asset loaders. */
photo_atlas *photo_finish(photo_set *set, const char *backdrop, int reference, int height, int density);
void photo_free(photo_set *set);    /* releases the set, atlas, frames and pixel storage */


#endif
