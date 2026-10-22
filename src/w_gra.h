// w_gra.h -- Primal Rage .GRA archive access (cf. w_wad.h)

#ifndef __W_GRA__
#define __W_GRA__

#include "pr_def.h"
#include "i_sound.h"

// A decoded frame. Coordinates of the bounding box are relative to the
// hotspot, y down.
typedef struct {
    int w, h;
    int x, y; // hotspot
    byte *pixels; // w*h palette indices, 0 = transparent
    int maxval;
    bool hasbox;
    int bx0, by0, bx1, by1;
    // forward reach per row, filled in by info.c (P_PatchProfile)
    int16_t *profile;
    int front;
} patch_t;

typedef struct {
    int count;
    pixel_t colors[256]; // colors[v] for pixel value v (1-based); [0] unused
} palette_t;

typedef struct {
    int w, h, x, y;
    int offset; // file offset of the RLE data
    bool decoded;
    patch_t *patch; // NULL for empty or marker frames
} frame_t;

typedef struct {
    char name[16];
    byte *data;
    int size;
    int framechunk; // file offset of the type-6 chunk (frame ids point into it)
    int numframes;
    frame_t *frames;
    int numpalettes;
    palette_t *palettes;
} gra_t;

typedef struct {
    char name[13];
    int size;
} lumpinfo_t;

extern lumpinfo_t *lumpinfo; // INDEX directory, in file-id order
extern int numlumps;

void W_Init(const char *datadir);
gra_t *W_LoadGra(const char *name); // cached; fatal if missing
gra_t *W_TryLoadGra(const char *name); // NULL if missing
gra_t *W_GraById(int id); // by INDEX position (the file ids used in PRAGE.EXE)
patch_t *W_Frame(gra_t *gra, int index);
int W_LoadSounds(const char *name, sfxdata_t **out);
int W_GuessPalette(const patch_t *patch, const gra_t *gra);
pixel_t W_RGB(int r, int g, int b);

#endif
