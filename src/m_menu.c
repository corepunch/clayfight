// m_menu.c -- title screen (cf. m_menu.c, D_PageDrawer in d_main.c)

#include "pr_def.h"
#include "g_game.h"
#include "m_menu.h"
#include "sel_main.h"
#include "v_video.h"
#include "w_gra.h"

// S04TITLE.GRA is a layered composition in the original 272x170 viewport:
// frames 39-55 zoom a stone slab in, 25-38 build the PRIMAL letters and
// 19-24 are the RAGE logo. Negative hotspots are offsets from the viewport's
// top-left corner.
#define VIEW_X ((SCREENWIDTH - 272) / 2)
#define VIEW_Y 6
#define SLAB_FIRST 39
#define SLAB_LAST 45 // the frame that fills the screen width
#define PRIMAL_FRAME 37
#define RAGE_FRAME 19

static gra_t *titlegra;
static int titlepal[64];
static int pagetic;

void M_StartTitle(void)
{
    if (!titlegra) {
        titlegra = W_LoadGra("S04TITLE.GRA");
        for (int i = 0; i < 64; i++)
            titlepal[i] = -1;
    }
    pagetic = 0;
    gamestate = GS_TITLE;
}

bool M_Responder(const event_t *ev)
{
    int pl, bit;
    if (ev->type == ev_keydown && G_Binding(ev->key, &pl, &bit) && (bit & (BT_START | BT_ATTACKS))) {
        SEL_Start();
        return true;
    }
    return false;
}

void M_Ticker(void)
{
    pagetic++;
}

static void M_DrawTitleFrame(int frame, int x, int y)
{
    patch_t *p = W_Frame(titlegra, frame);
    if (!p)
        return;
    if (titlepal[frame] < 0)
        titlepal[frame] = W_GuessPalette(p, titlegra);
    V_DrawPatch(x, y, p, &titlegra->palettes[titlepal[frame]], 0);
}

void M_Drawer(void)
{
    V_Clear(0xff000000);
    // the slab zooms in, centred on its hotspot
    int slab = MIN(SLAB_FIRST + pagetic / 6, SLAB_LAST);
    M_DrawTitleFrame(slab, VIEW_X + 136, VIEW_Y + 85);
    if (pagetic > 50)
        M_DrawTitleFrame(PRIMAL_FRAME, VIEW_X, VIEW_Y);
    if (pagetic > 80)
        M_DrawTitleFrame(RAGE_FRAME, VIEW_X, VIEW_Y);
    if (pagetic & 32)
        V_DrawTextCentered(FONT_SMALL, SCREENWIDTH / 2, SCREENHEIGHT - 20, "PRESS START");
    V_DrawTextCentered(FONT_SMALL, SCREENWIDTH / 2, SCREENHEIGHT - 9, "WASD + R T F G     ARROWS + O P L ;");
}
