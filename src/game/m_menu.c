// Original PC menu vocabulary and art, with a Clayfight creation entry.
#include <stdio.h>
#include <string.h>
#include "game/game.h"

options_t options = {2, 60, 2, 5, true};
static int pagetic, page, cursor;
static const char *const mainitems[] = {"START", "CREATE FIGHTER", "OPTIONS", "CONTROLS"};

void M_LoadOptions(void)
{
    FILE *f = fopen("build/options.cfg", "r");
    int sound;
    if (f) {
        if (fscanf(f, "%d %d %d %d %d", &options.rounds, &options.seconds,
                   &options.difficulty, &options.credits, &sound) == 5)
            options.sound = sound != 0;
        fclose(f);
    }
    options.rounds = CLAMP(options.rounds, 1, 4);
    options.seconds = CLAMP(options.seconds, 30, 99);
    options.difficulty = CLAMP(options.difficulty, 1, 3);
    options.credits = CLAMP(options.credits, 1, 10);
}

void M_SaveOptions(void)
{
    FILE *f = fopen("build/options.cfg", "w");
    if (f) {
        fprintf(f, "%d %d %d %d %d\n", options.rounds, options.seconds,
                options.difficulty, options.credits, options.sound);
        fclose(f);
    }
}

// Menus use original patches, drawn by their top-left corner. Cache palettes
// because the archive stores no explicit frame -> palette table.
void M_DrawArt(const char *file, int frame, int x, int y, float scale, int flags)
{
    static struct { gra_t *g; int frame, pal; } cache[256];
    static int count;
    gra_t *g = W_LoadGra(file);
    patch_t *p = W_Frame(g, frame);
    if (!p) return;
    int pal = -1;
    for (int i = 0; i < count; i++)
        if (cache[i].g == g && cache[i].frame == frame) pal = cache[i].pal;
    if (pal < 0) {
        pal = W_GuessPalette(p, g);
        if (count < 256) {
            cache[count].g = g; cache[count].frame = frame; cache[count++].pal = pal;
        }
    }
    V_DrawPatchScaled(x + p->x * scale, y + p->y * scale, p, &g->palettes[pal], flags, scale, scale);
}

void M_Background(const char *heading)
{
    V_Clear(0xff070609);
    M_DrawArt("S04SELMO.GRA", 136, 24, 26, 1, V_FADE);
    V_FillRect(16, 43, 288, 175, 0xff100d10);
    V_FillRect(16, 42, 288, 1, 0xff806143);
    V_FillRect(16, 217, 288, 1, 0xff806143);
    V_DrawTextCentered(FONT_BIG, 160, 21, heading);
    V_DrawTextCentered(FONT_SMALL, 160, 230, "ARROWS / WASD: MOVE   ENTER: SELECT   ESC: BACK");
}

void M_StartTitle(void)
{
    pagetic = 0;
    gamestate = GS_TITLE;
    G_ClearInput();
}

void M_StartMenu(void)
{
    page = cursor = 0;
    gamestate = GS_MENU;
    paused = false;
    G_ClearInput();
}

void M_OpenOptions(void)
{
    M_StartMenu(); page = 1;
}

bool M_Responder(const event_t *ev)
{
    if (ev->type != ev_keydown) return false;
    if (ev->key == KEY_ESCAPE || ev->key == PADKEY(0, PK_BACK)) {
        if (page) M_StartMenu(); else M_StartTitle();
        return true;
    }
    int pl, bit;
    if (!G_Binding(ev->key, &pl, &bit)) return false;
    if (gamestate == GS_TITLE) {
        if (bit & (BT_START | BT_ATTACKS)) M_StartMenu();
        return true;
    }
    int n = page == 1 ? 7 : page == 2 ? 1 : 4;
    if (bit & BT_UP) cursor = (cursor + n - 1) % n;
    if (bit & BT_DOWN) cursor = (cursor + 1) % n;
    if (page == 1 && (bit & (BT_LEFT | BT_RIGHT | BT_START | BT_ATTACKS))) {
        int delta = bit & BT_LEFT ? -1 : 1;
        switch (cursor) {
        case 0: options.rounds = CLAMP(options.rounds + delta, 1, 4); break;
        case 1: options.seconds = CLAMP(options.seconds + delta * 10, 30, 99); break;
        case 2: options.difficulty = CLAMP(options.difficulty + delta, 1, 3); break;
        case 3: options.credits = CLAMP(options.credits + delta, 1, 10); break;
        case 4: options.sound = !options.sound; break;
        case 5: options = (options_t){2, 60, 2, 5, true}; break;
        case 6: M_StartMenu(); break;
        }
        M_SaveOptions();
    } else if (bit & (BT_START | BT_ATTACKS)) {
        if (page == 2) M_StartMenu();
        else switch (cursor) {
        case 0: SEL_Start(); break;
        case 1: M_Create(); break;
        case 2: page = 1; cursor = 0; break;
        case 3: page = 2; cursor = 0; break;
        }
    }
    return true;
}

void M_Ticker(void) { pagetic++; }

void M_Drawer(void)
{
    if (gamestate == GS_TITLE) {
        V_Clear(0xff000000);
        // Original title slab and layered PRIMAL / RAGE lettering.
        gra_t *g = W_LoadGra("S04TITLE.GRA");
        int slab = MIN(39 + pagetic / 6, 45);
        patch_t *p = W_Frame(g, slab);
        M_DrawArt("S04TITLE.GRA", slab, 160 - p->w / 2, 104 - p->h / 2, 1, 0);
        if (pagetic > 50) M_DrawArt("S04TITLE.GRA", 37, 95, 83, 1, 0);
        if (pagetic > 80) M_DrawArt("S04TITLE.GRA", 19, 76, 107, 1, 0);
        if (pagetic & 32) V_DrawTextCentered(FONT_SMALL, 160, 198, "PRESS START");
        V_DrawTextCentered(FONT_SMALL, 160, 226, "CLAYFIGHT  /  PRIMAL RAGE + YOUR CLAY FIGHTERS");
        return;
    }
    M_Background(page == 1 ? "CONFIG OPTIONS" : page == 2 ? "CONTROLS" : "MAIN MENU");
    if (page == 2) {
        const char *lines[] = {"LEFT PLAYER: WASD", "HI QUICK / FIERCE: R / T", "LO QUICK / FIERCE: F / G", "RIGHT PLAYER: ARROWS",
            "HI QUICK / FIERCE: O / P", "LO QUICK / FIERCE: L / ;", "P2 JOINS WITH AN ATTACK", "ESC: PAUSE   Q: LEAVE PAUSED FIGHT", "GAMEPADS: D-PAD + FOUR FACE BUTTONS"};
        for (int i = 0; i < 9; i++) V_DrawTextCentered(FONT_SMALL, 160, 60 + 16 * i, lines[i]);
        return;
    }
    char labels[7][64];
    if (page == 1) {
        snprintf(labels[0], 64, "NUMBER OF ROUNDS    %d", options.rounds * 2 - 1);
        snprintf(labels[1], 64, "ROUND TIME          %d", options.seconds);
        snprintf(labels[2], 64, "DIFFICULTY          %s", options.difficulty == 1 ? "EASY" : options.difficulty == 2 ? "NORMAL" : "HARD");
        snprintf(labels[3], 64, "GAME CREDITS        %d", options.credits);
        snprintf(labels[4], 64, "GAME SAMPLES        %s", options.sound ? "ON" : "OFF");
        snprintf(labels[5], 64, "RESTORE DEFAULTS"); snprintf(labels[6], 64, "BACK");
    }
    int n = page == 1 ? 7 : 4, y = page == 1 ? 61 : 75, step = page == 1 ? 21 : 31;
    for (int i = 0; i < n; i++) {
        if (i == cursor) {
            V_FillRect(30, y - 5, 260, page == 1 ? 18 : 24, 0xff37241b);
            V_DrawText(FONT_SMALL, 37, y, ">");
        }
        V_DrawTextCentered(page == 1 ? FONT_SMALL : FONT_BIG, 160, y, page == 1 ? labels[i] : mainitems[i]);
        y += step;
    }
}
