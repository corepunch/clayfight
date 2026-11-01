// Clayfight resource formats and compatibility adapters. Generic rendering,
// photo processing and audio are provided by the shared engine.

#ifndef CLAY_RESOURCES_H
#define CLAY_RESOURCES_H

#include "platform/platform.h"
#include "support/support.h"

// Arcade-native 336x240 coordinates, presented at exactly 2x (672x480).
// Sprites keep their original resolution; photos can supply native detail.
#define VIEWWIDTH (SCREENWIDTH / 2)
#define VIEWHEIGHT (SCREENHEIGHT / 2)
#define PIXELS_PER_UNIT 2

// --- m_argv.c: command line parameters (cf. m_argv.h) ----------------------
extern int myargc;
extern char **myargv;

// Index of the parameter, or 0 if absent.
int M_CheckParm(const char *check);
// Value following a parameter, or def.
const char *M_ParmValue(const char *check, const char *def);

// --- m_random.c: deterministic random numbers (cf. m_random.h) -------------
int P_Random(void); // play simulation only: 0..255
int M_Random(void); // menus and cosmetics: 0..255
void M_ClearRandom(void);

// --- w_gra.c: arcade motion-object and virtual art-bank access (cf. w_wad.h) ----------------
// A decoded frame. Coordinates of the bounding box are relative to the
// hotspot, y down.
typedef struct {
    int w, h;
    int x, y; // hotspot
    byte *pixels; // w*h palette indices, 0 = transparent
    byte *detail; // optional (w*2)*(h*2) photo pixels, same palette and bounds
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

extern lumpinfo_t *lumpinfo; // legacy directory interface; native ROM banks need no directory
extern int numlumps;

void W_Init(const char *datadir);
void W_DrawArcadeStage(int stage, int camx);
int W_ArcadeStageScroll(int stage, int camx, int y);
const palette_t *W_ArcadeStagePalette(int stage, int color);
patch_t *W_ArcadeShadow(int character, int frame);
const palette_t *W_ArcadeImpactPalette(bool blocked);
const palette_t *W_ArcadeTribePalette(int character,int player);
// Virtual HUD slots preserve the original authored health/stun tile maps.
enum { HUD_HEALTH_LEFT=0, HUD_HEALTH_RIGHT=121, HUD_STUN_LEFT=242,
       HUD_STUN_RIGHT=311, HUD_HEART=380, HUD_BRAIN=390, HUD_NAME=408,
       HUD_FRAMES=415 };
gra_t *W_LoadGra(const char *name); // cached; fatal if missing
gra_t *W_TryLoadGra(const char *name); // NULL if missing
gra_t *W_GraById(int id); // compatibility access to the arcade object bank
patch_t *W_Frame(gra_t *gra, int index);
void W_SoundDirectory(const char *directory);
const sfxdata_t *W_ArcadeSound(int command);
int W_GuessPalette(const patch_t *patch, const gra_t *gra);
int W_FramePalette(gra_t *gra, int frame); // recorded menu mappings, otherwise guessed
pixel_t W_RGB(int r, int g, int b);

// --- w_exe.c: big-endian 68020 ROM tables (cf. w_wad.h) ---------
// Four interleaved CPU chips provide scripts, moves, boxes and palettes.

bool X_Init(const char *path); // false if missing or not the expected build
bool X_Loaded(void);
bool X_Valid(uint32_t addr, int len);
byte X_U8(uint32_t addr);
int8_t X_S8(uint32_t addr);
uint16_t X_U16(uint32_t addr);
int16_t X_S16(uint32_t addr);
uint32_t X_U32(uint32_t addr);

// --- w_photo.c: shared photo atlas adapted to game resources --------------
// A photo creature's frames are gathered while its creature file is read,
// then processed together: background removal, one scale for every frame,
// one shared 31-colour palette, hotspot at the feet. The result is an
// in-memory gra_t, so the rest of the engine treats photographed clay the
// same as Primal Rage's art.

typedef struct photoset_s photoset_t;

photoset_t *W_PhotoBegin(const char *name);
// Register a photo (paths are deduplicated); returns its frame index.
int W_PhotoFrame(photoset_t *ps, const char *path);
gra_t *W_PhotoGra(photoset_t *ps); // valid at once; frames fill in at finish
// Process everything. backdrop: empty-scene shot or NULL; reference: frame
// whose figure gets `height` pixels tall.
void W_PhotoFinish(photoset_t *ps, const char *backdrop, int reference, int height);


// --- v_video.c: framebuffer, patch and text drawing (cf. v_video.h) --------
extern pixel_t screen[SCREENWIDTH * SCREENHEIGHT];

enum {
    V_FLIP = 1, // mirror around the hotspot
    V_FLASH = 2, // solid white (hit flash)
    V_SHADOW = 4, // darken what's underneath
    V_FADE = 8, // 50% blend
};

typedef enum { FONT_SMALL, FONT_BIG, FONT_TIMER } font_t;

void V_Init(void); // decodes the arcade character chip
void V_Clear(pixel_t color);
void V_FillRect(int x, int y, int w, int h, pixel_t color);
void V_VGradient(int y, int h, pixel_t top, pixel_t bottom);
pixel_t V_Lerp(pixel_t a, pixel_t b, float t);

// Draw with the hotspot at (x, y), optionally scaled. Coordinates and sizes
// use VIEWWIDTH x VIEWHEIGHT units; photos can supply native display pixels.
void V_DrawPatch(int x, int y, const patch_t *p, const palette_t *pal, int flags);
void V_DrawPatchScaled(int x, int y, const patch_t *p, const palette_t *pal, int flags, float sx, float sy);

int V_TextWidth(font_t font, const char *text);
void V_DrawText(font_t font, int x, int y, const char *text);
void V_DrawTextColor(font_t font, int x, int y, const char *text, pixel_t color);
void V_DrawTextScaled(font_t font, int x, int y, const char *text, pixel_t color, float sx, float sy);
void V_DrawTextCentered(font_t font, int cx, int y, const char *text);

#endif
