// sel_main.c -- character select screen (cf. wi_stuff.c: its own ticker,
// responder and drawer, entered and left through d_main/g_game)
//
// Art comes from select: the temple's four-over-three reliefs become
// coloured heads under the two cursors. Clay fighters have their own pages.

#include <stdlib.h>
#include <stdio.h>
#include <strings.h>
#include <math.h>

#include "game/game.h"

#define BACKDROP 136 // 336x240 arcade temple
#define LAUNCH_DELAY 50

static gra_t *selgra;
static int cursor[2]; // both start on Sauron (set in SEL_Start)
static bool chosen[2];
static bool joined[2] = {true, false}; // the right player joins by pressing a button
static int seltime, launch, teamcount[2], rosterpage;
static fighterdef_t *teams[2][4];
static int palcache[512];

// The temple's stone heads, in visual order (four above three). Keep the
// original roster on this page; photographs occupy matching pages of tiles.
static const int headframes[7] = {140, 139, 148, 147, 103, 102, 111};
static const int headpalettes[7] = {28, 30, 29, 1, 2, 0, 27};
static const int tilex[7] = {57, 98, 138, 179, 77, 118, 158};
static const int tiley[7] = {19, 19, 19, 19, 46, 46, 46};
static int slots[MAXROSTER + 7], numslots;

// The complete arcade temple fills the native view, with every texel 2x.
// Tile registration converts the old authored cell coordinates only.
#define TEMPLE_X 0
#define TEMPLE_Y 0
#define TEMPLE_SX (336.f / 272.f)
#define TEMPLE_SY (240.f / 170.f)
#define PODIUM_TOP 200
#define PREVIEW_FEET (PODIUM_TOP + 4)
static int TileX(int cell) { return (int)lroundf(TEMPLE_X + tilex[cell] * TEMPLE_SX); }
static int TileY(int cell) { return (int)lroundf(TEMPLE_Y + tiley[cell] * TEMPLE_SY); }

static int SEL_Slot(int fighter)
{
    for (int i = 0; i < numslots; i++)
        if (slots[i] == fighter) return i;
    return 0;
}

static void SEL_Move(int pl, int bit)
{
    int old = SEL_Slot(cursor[pl]), next = old, cell = old % 7;
    int pages = (numslots + 6) / 7, page = old / 7;
    if (bit & BT_LEFT) next--;
    else if (bit & BT_RIGHT) next++;
    else if (bit & BT_DOWN)
        next = cell < 4 ? page * 7 + 4 + MIN(cell, 2) : ((page + 1) % pages) * 7 + cell - 4;
    else if (bit & BT_UP)
        next = cell >= 4 ? page * 7 + cell - 4 : ((page + pages - 1) % pages) * 7 + 4 + MIN(cell, 2);
    else return;
    int dir = bit & (BT_LEFT | BT_UP) ? -1 : 1;
    // A final clay page may be partially filled; skip its empty cells.
    for (int n = 0; n < pages * 7; n++, next += dir) {
        next = (next + pages * 7) % (pages * 7);
        if (next < numslots && slots[next] >= 0) {
            cursor[pl] = slots[next];
            rosterpage = next / 7;
            return;
        }
    }
}

void SEL_Reset(void)
{
    cursor[0]=cursor[1]=0;chosen[0]=chosen[1]=false;
    joined[0]=true;joined[1]=false;seltime=launch=rosterpage=0;
    teamcount[0]=teamcount[1]=0;
}

void SEL_Start(void)
{
    if (!selgra) {
        selgra = W_LoadGra("select");
        for (int i = 0; i < 512; i++)
            palcache[i] = -1;
        for (int i = 0; i < numroster; i++) // the arcade's default pick
            if (!strcasecmp(roster[i]->name, "SAURON"))
                cursor[0] = cursor[1] = i;
    }
    P_RefreshRoster(M_ParmValue("-creatures", "data/creatures"));
    numslots = 7;
    for (int j = 0; j < 7; j++) {
        slots[j] = -1;
        for (int i = 0; i < numroster; i++)
            if (roster[i]->codeindex == X_U8(0x6aca6+j)) slots[j] = i;
    }
    for (int i = 0; i < numroster; i++)
        if (roster[i]->codeindex < 0 || roster[i]->codeindex >= 7) slots[numslots++] = i;
    for (int pl = 0; pl < 2; pl++) cursor[pl] = CLAMP(cursor[pl], 0, numroster - 1);
    joined[0] = true; joined[1] = gamemode == GM_TRAINING;
    teamcount[0] = teamcount[1] = 0;
    endurance = false; C_Abandon();
    chosen[0] = chosen[1] = false;
    G_ClearInput();
    seltime = launch = 0;
    rosterpage = SEL_Slot(cursor[0]) / 7;
    gamestate = GS_SELECT;
}

static void SEL_DrawArt(int frame, int x, int y, float sx, float sy)
{
    patch_t *p = W_Frame(selgra, frame);
    if (!p)
        return;
    if (palcache[frame] < 0)
        palcache[frame] = W_FramePalette(selgra, frame);
    V_DrawPatchScaled(x + (int)lroundf(p->x * sx), y + (int)lroundf(p->y * sy),
                      p, &selgra->palettes[palcache[frame]], 0, sx, sy);
}

bool SEL_Responder(const event_t *ev)
{
    int pl, bit;
    if (ev->type == ev_keydown && (ev->key == KEY_ESCAPE || ev->key == PADKEY(0, PK_BACK))) {
        M_StartMenu(); return true;
    }
    if (ev->type == ev_keydown && ev->key == 'c') { M_Create(); return true; }
    if (ev->type == ev_keydown && ev->key == '\t') {
        pl = gamemode == GM_TRAINING && chosen[0] ? 1 : 0;
        if (!chosen[pl]) {
            int pages = (numslots + 6) / 7;
            int next = ((SEL_Slot(cursor[pl]) / 7 + 1) % pages) * 7;
            // Select the first occupied tile on the next page.
            for (int i = 0; i < numslots; i++) {
                int slot = (next + i) % numslots;
                if (slots[slot] >= 0) { cursor[pl] = slots[slot]; rosterpage = slot / 7; break; }
            }
        }
        return true;
    }
    if (ev->type != ev_keydown || !G_Binding(ev->key, &pl, &bit))
        return false;
    if (gamemode == GM_TRAINING && pl == 0 && chosen[0]) pl = 1;
    if (!joined[pl]) {
        if (bit & (BT_ATTACKS | BT_START)) {
            joined[pl] = true;
            S_StartSound(sfx_select, -1, 0.8f);
        }
        return true;
    }
    if (chosen[pl])
        return true;
    SEL_Move(pl, bit);
    if ((bit & (BT_ATTACKS | BT_START)) && P_Playable(roster[cursor[pl]])) {
        if (gamemode == GM_ENDURANCE) {
            teams[pl][teamcount[pl]++] = roster[cursor[pl]];
            chosen[pl] = teamcount[pl] == 4;
        } else chosen[pl] = true;
        S_StartSound(sfx_select, -1, 0.8f);
    }
    return true;
}

void SEL_Ticker(void)
{
    seltime++;
    // a lone player fights the CPU, which picks as soon as the player has
    if (chosen[0] && !joined[1] && !chosen[1]) {
        int playable[MAXROSTER], n = 0;
        for (int i = 0; i < numroster; i++)
            if (P_Playable(roster[i]))
                playable[n++] = i;
        if (!n) return;
        cursor[1] = playable[M_Random() % n];
        chosen[1] = true;
        if (gamemode == GM_ENDURANCE) {
            for (int i = 0; i < 4; i++) teams[1][i] = roster[playable[M_Random() % n]];
            teamcount[1] = 4;
        }
    }
    if (chosen[0] && chosen[1] && ++launch == LAUNCH_DELAY) {
        bool cpu[2] = {false, !joined[1] || gamemode == GM_TRAINING};
        if (gamemode == GM_ENDURANCE) { E_Start(teams, cpu); return; }
        if (gamemode == GM_ARCADE && !joined[1] && C_Start(roster[cursor[0]])) return;
        C_Abandon();
        G_InitNew(roster[cursor[0]], roster[cursor[1]], cpu, &stages[M_Random() % 7]);
    }
}

static void SEL_Box(int x, int y, int w, int h, pixel_t c)
{
    V_FillRect(x, y, w, 1, c);
    V_FillRect(x, y + h - 1, w, 1, c);
    V_FillRect(x, y, 1, h, c);
    V_FillRect(x + w - 1, y, 1, h, c);
}

// A creature's stand pose, scaled to fit a w x h box, feet on its bottom edge.
void M_DrawCreature(const fighterdef_t *d, int x, int y, int w, int h, bool mirror)
{
    const state_t *st = &d->states[d->anims[ANIM_STAND].first];
    patch_t *p = d->photoport ? W_Frame(d->photoport, 0) : P_StatePatch(st);
    if (!p)
        return;
    V_FillRect(x, y, w, h, 0xff101418);
    float sc = MIN((float)(w - 4) / p->w, (float)(h - 4) / p->h);
    bool flip = d->facesleft == !mirror; // face the middle of the screen
    int px = (int)(x + w / 2 + (flip ? -1 : 1) * (p->x - p->w / 2.f) * sc);
    V_DrawPatchScaled(px, y + h - 2, p, (d->photoport ? &d->photoport->palettes[0] : &d->base->palettes[d->palette]), flip ? V_FLIP : 0, sc, sc);
}

// Display the breathing pose on the podium's top surface, facing inward.
static void SEL_Preview(int pl)
{
    const fighterdef_t *d = roster[cursor[pl]];
    const animinfo_t *a = &d->anims[ANIM_STAND];
    const state_t *st = &d->states[a->first + seltime / 6 % a->count];
    const patch_t *p = P_StatePatch(st);
    if (!p) return;
    int height = p->h;
    for (int i = 0; i < a->count; i++) {
        const patch_t *pose = P_StatePatch(&d->states[a->first + i]);
        if (pose) height = MAX(height, pose->h);
    }
    float sc = 94.f / height;
    bool flip = (d->facesleft == (pl == 0)) != st->hflip;
    int cx = (pl ? 252 : 84);
    // The stand hotspot is the foot position. Centring the whole bitmap
    // includes long tails and pushes the feet off the podium toward P2.
    int x = cx;
    int y = PREVIEW_FEET - (int)lroundf((p->h - p->y) * sc);
    V_DrawPatchScaled(x, y, p, &d->base->palettes[d->palette], flip ? V_FLIP : 0, sc, sc);
    float labelscale = MIN(0.65f, 132.f / V_TextWidth(FONT_BIG, d->name));
    int labelx = pl ? 294 - (int)(V_TextWidth(FONT_BIG, d->name) * labelscale) : 26;
    V_DrawTextScaled(FONT_BIG, labelx, 102, d->name, 0xffffe080, labelscale, 0.9f);
}

static void SEL_Tile(int cell, int fighter)
{
    int x = TileX(cell), y = TileY(cell);
    int w = (int)lroundf(37 * TEMPLE_SX), h = (int)lroundf(24 * TEMPLE_SY);
    int selected = -1;
    for (int pl = 0; pl < 2; pl++)
        if (cursor[pl] == fighter && (joined[pl] || chosen[pl])) selected = pl;
    if (rosterpage == 0) {
        if (selected < 0) return; // unselected heads are carved into the temple
        const patch_t *p = W_Frame(selgra, headframes[cell]);
        const palette_t *pal = &selgra->palettes[headpalettes[cell]];
        // Arcade head hotspots encode their absolute position in the temple.
        V_DrawPatch(0, 0, p, pal, 0);
    } else {
        // Clay close-ups replace the stone heads on their own temple page.
        const fighterdef_t *d = roster[fighter];
        const patch_t *p = d->photoport ? W_Frame(d->photoport, 0)
                                       : P_StatePatch(&d->states[d->anims[ANIM_STAND].first]);
        V_FillRect(x, y, w, h, selected < 0 ? 0xff383c40 : selected ? 0xff807018 : 0xff801810);
        if (p) {
            float sc = MIN((w - 4.f) / p->w, (h - 2.f) / p->h);
            const palette_t *pal = d->photoport ? &d->photoport->palettes[0] : &d->base->palettes[d->palette];
            V_DrawPatchScaled(x + w / 2 + (int)lroundf((p->x - p->w / 2.f) * sc),
                              y + h - 1 - (int)lroundf((p->h - p->y) * sc), p, pal, 0, sc, sc);
        }
        SEL_Box(x - 1, y - 1, w + 2, h + 2, 0xff74787c);
    }
    for (int pl = 0; pl < 2; pl++) {
        if (cursor[pl] != fighter || (!joined[pl] && !chosen[pl])) continue;
        pixel_t color = pl ? 0xffffed10 : 0xffff2010;
        int inset = selected == 1 && pl == 0 ? 2 : 0;
        SEL_Box(x - 2 + inset, y - 2 + inset, w + 4 - inset * 2, h + 4 - inset * 2, color);
        SEL_Box(x - 1 + inset, y - 1 + inset, w + 2 - inset * 2, h + 2 - inset * 2, color);
    }
}

void SEL_Drawer(void)
{
    V_Clear(0xff08030b);
    SEL_DrawArt(BACKDROP, 0, 0, 1, 1);
    const char *heading = gamemode == GM_ENDURANCE ? "SELECT FOUR FIGHTERS"
        : gamemode == GM_TRAINING && chosen[0] ? "SELECT OPPONENT" : "START TO SELECT";
    V_DrawTextScaled(FONT_BIG, 168 - (int)(V_TextWidth(FONT_BIG, heading) * 0.375f),
                     3, heading, 0xffffe080, 0.75f, 0.85f);

    if (rosterpage) {
        // Cover all seven reliefs, including unused cells of a final page.
        for (int cell = 0; cell < 7; cell++) {
            int x = TileX(cell), y = TileY(cell);
            V_FillRect(x - 1, y - 1, 48, 36, 0xff383c40);
            SEL_Box(x - 1, y - 1, 48, 36, 0xff74787c);
        }
    }
    for (int cell = 0; cell < 7; cell++) {
        int slot = rosterpage * 7 + cell;
        if (slot < numslots && slots[slot] >= 0) SEL_Tile(cell, slots[slot]);
    }
    for (int pl = 0; pl < 2; pl++) {
        int cx = (pl ? 252 : 84);
        const patch_t *podium = W_Frame(selgra, 105 + pl);
        SEL_DrawArt(105 + pl, cx - podium->w / 2, PODIUM_TOP, 1, 1);
        if (joined[pl] || chosen[pl]) SEL_Preview(pl);
    }

    const pixel_t green = 0xff00d000, cyan = 0xff20c8d8;
    if (gamemode == GM_ENDURANCE) {
        char team[40];
        snprintf(team, sizeof team, "TEAM 1 %d/4   TEAM 2 %d/4", teamcount[0], teamcount[1]);
        V_DrawTextColor(FONT_SMALL, 15, 220, team, green);
    } else if (!joined[1]) {
#ifdef CLAY_IOS
        V_DrawTextColor(FONT_SMALL, 15, 220, "SELECT: CONFIRM", cyan);
#else
        V_DrawTextColor(FONT_SMALL, 15, 220, "ENTER: CONFIRM", cyan);
#endif
    }
    if (!joined[1] && !chosen[1]) {
        if ((seltime / 30) % 2 == 0) V_DrawTextColor(FONT_SMALL, 205, 220, "P2: PRESS START", green);
    } else if (chosen[0] && chosen[1]) V_DrawTextCentered(FONT_BIG, 168, 150, "FIGHT!");

    char footer[72];
#ifdef CLAY_IOS
    snprintf(footer, sizeof footer, "%s %d/%d  -  LEFT/RIGHT: MORE FIGHTERS",
             rosterpage ? "CLAY" : "ORIGINAL", rosterpage + 1, (numslots + 6) / 7);
#else
    snprintf(footer, sizeof footer, "%s %d/%d  TAB: %s  C: CREATE",
             rosterpage ? "CLAY" : "ORIGINAL", rosterpage + 1, (numslots + 6) / 7,
             rosterpage ? "NEXT PAGE" : "CLAY FIGHTERS");
#endif
    V_FillRect(0, VIEWHEIGHT - 9, VIEWWIDTH, 9, 0xff141218);
    V_DrawTextCentered(FONT_SMALL, 168, VIEWHEIGHT - 7, footer);
}
