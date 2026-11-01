// Clayfight framebuffer size, sprite adapters and original font mappings.
#include <ctype.h>
#include <math.h>
#include "game/resources.h"
pixel_t screen[SCREENWIDTH * SCREENHEIGHT];
static raster surface(void) { return (raster){screen, SCREENWIDTH, SCREENHEIGHT, PIXELS_PER_UNIT}; }
void V_Clear(pixel_t c) { raster_clear(surface(), c); }
void V_FillRect(int x,int y,int w,int h,pixel_t c) { raster_fill(surface(),x,y,w,h,c); }
pixel_t V_Lerp(pixel_t a,pixel_t b,float t) { return raster_lerp(a,b,t); }
void V_VGradient(int y,int h,pixel_t a,pixel_t b) { raster_gradient(surface(),y,h,a,b); }
void V_DrawPatchScaled(int x,int y,const patch_t *p,const palette_t *pal,int flags,float sx,float sy) {
    if (!p) return;
    indexed_sprite src={p->w,p->h,p->x,p->y,p->pixels,p->detail,PIXELS_PER_UNIT};
    raster_draw(surface(),x,y,src,pal?pal->colors:NULL,flags,sx,sy);
}
void V_DrawPatch(int x,int y,const patch_t *p,const palette_t *pal,int flags) { V_DrawPatchScaled(x,y,p,pal,flags,1,1); }

// --- original compact, intro and timer alphabets ----------------------

static gra_t *fonts,*large,*stone;
static gra_t *Font(font_t font) {return font==FONT_BIG?large:font==FONT_TIMER?stone:fonts;}

void V_Init(void)
{
    fonts=W_LoadGra("fonts");large=W_LoadGra("large-font");stone=W_LoadGra("stone-font");
}
static const patch_t *glyph(font_t font,char ch)
{
    int c=toupper((byte)ch)&127;
    if(c<32 || (c==32 && font!=FONT_BIG))return NULL;
    return W_Frame(Font(font),c);
}
static int Advance(font_t font,const patch_t *p)
{
    if(font==FONT_BIG)return p?p->w:8; // native one/two 8-pixel alpha cells
    return p?p->w+1:font==FONT_SMALL?4:8;
}

int V_TextWidth(font_t font, const char *text)
{
    int w = 0;
    for (; *text; text++) {
        const patch_t *p = glyph(font, *text);
        w += Advance(font,p);
    }
    return w;
}

static void DrawText(font_t font, int x, int y, const char *text, const palette_t *pal, float sx, float sy)
{
    float pen = x;
    for (; *text; text++) {
        const patch_t *p = glyph(font, *text);
        if (!p) {
            pen += Advance(font,p)*sx;
            continue;
        }
        V_DrawPatchScaled((int)lroundf(pen + p->x * sx), y + (int)lroundf(p->y * sy), p, pal, 0, sx, sy);
        pen += Advance(font,p)*sx;
    }
}

void V_DrawText(font_t font, int x, int y, const char *text)
{
    DrawText(font, x, y, text, &Font(font)->palettes[0], 1, 1);
}

void V_DrawTextColor(font_t font, int x, int y, const char *text, pixel_t color)
{
    V_DrawTextScaled(font, x, y, text, color, 1, 1);
}

void V_DrawTextScaled(font_t font, int x, int y, const char *text, pixel_t color, float sx, float sy)
{
    palette_t pal = Font(font)->palettes[0];
    for (int i = 1; i <= pal.count; i++) {
        pixel_t c = pal.colors[i];
        pal.colors[i] = W_RGB(((color >> 16) & 255) * ((c >> 16) & 255) / 255,
                             ((color >> 8) & 255) * ((c >> 8) & 255) / 255,
                             (color & 255) * (c & 255) / 255);
    }
    DrawText(font, x, y, text, &pal, sx, sy);
}

void V_DrawTextCentered(font_t font, int cx, int y, const char *text)
{
    int width=V_TextWidth(font,text);
    float scale=MIN(1.f,(VIEWWIDTH-12.f)/MAX(1,width));
    DrawText(font,cx-(int)lroundf(width*scale)/2,y,text,
             &Font(font)->palettes[0],scale,scale);
}
