// v_video.h -- software framebuffer, patch and text drawing (cf. v_video.h)

#ifndef __V_VIDEO__
#define __V_VIDEO__

#include "pr_def.h"
#include "w_gra.h"

extern pixel_t screen[SCREENWIDTH * SCREENHEIGHT];

enum {
    V_FLIP = 1, // mirror around the hotspot
    V_FLASH = 2, // solid white (hit flash)
    V_SHADOW = 4, // darken what's underneath
    V_FADE = 8, // 50% blend
};

typedef enum { FONT_SMALL, FONT_BIG } font_t;

void V_Init(void); // needs S04FONTS.GRA
void V_Clear(pixel_t color);
void V_FillRect(int x, int y, int w, int h, pixel_t color);
void V_VGradient(int y, int h, pixel_t top, pixel_t bottom);
pixel_t V_Lerp(pixel_t a, pixel_t b, float t);

// Draw with the hotspot at (x, y), optionally scaled.
void V_DrawPatch(int x, int y, const patch_t *p, const palette_t *pal, int flags);
void V_DrawPatchScaled(int x, int y, const patch_t *p, const palette_t *pal, int flags, float sx, float sy);

int V_TextWidth(font_t font, const char *text);
void V_DrawText(font_t font, int x, int y, const char *text);
void V_DrawTextCentered(font_t font, int cx, int y, const char *text);

#endif
