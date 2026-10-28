// Original PC menu vocabulary and original art, with a Clayfight creation entry.
#include <stdio.h>
#include <string.h>
#include "game/game.h"

options_t options = {2, 60, 2, 5, true, 8};
enum { MENU_MAIN, MENU_CONFIG, MENU_CONTROLS, MENU_START, MENU_OPTIONS, MENU_VOLUME, MENU_SOUND };
static int pagetic, page, cursor;
static const char *const mainitems[] = {"START", "CREATE FIGHTER", "OPTIONS"};
static const char *const startitems[] = {"ARCADE", "TRAINING", "TUG OF WAR", "ENDURANCE", "BACK"};
static const char *const optionitems[] = {"CONFIG OPTIONS", "ADJUST VOLUME", "SOUND TEST", "MODIFY CONTROLS", "BACK"};
static const char *const sounditems[] = {"HIT", "HEAVY HIT", "BLOCK", "SWISH", "THUD", "CROWD", "SELECT"};

void M_LoadOptions(void)
{
    FILE *f = fopen("build/options.cfg", "r");
    int sound;
    if (f) {
        int n = fscanf(f, "%d %d %d %d %d %d", &options.rounds, &options.seconds,
                       &options.difficulty, &options.credits, &sound, &options.volume);
        if (n >= 5) options.sound = sound != 0;
        fclose(f);
    }
    options.rounds = CLAMP(options.rounds, 1, 4);
    options.seconds = CLAMP(options.seconds, 30, 99);
    options.difficulty = CLAMP(options.difficulty, 1, 3);
    options.credits = CLAMP(options.credits, 1, 10);
    options.volume = CLAMP(options.volume, 0, 10);
}
void M_SaveOptions(void)
{
    FILE *f = fopen("build/options.cfg", "w");
    if (f) {
        fprintf(f, "%d %d %d %d %d %d\n", options.rounds, options.seconds,
                options.difficulty, options.credits, options.sound, options.volume);
        fclose(f);
    }
}

// Menus use original patches, drawn by their top-left corner. Cache the
// recorded palette mapping (or the fallback for unmapped artwork).
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
        pal = W_FramePalette(g, frame);
        if (count < 256) {
            cache[count].g = g; cache[count].frame = frame; cache[count++].pal = pal;
        }
    }
    V_DrawPatchScaled(x + p->x * scale, y + p->y * scale, p, &g->palettes[pal], flags, scale, scale);
}
void M_Background(const char *heading)
{
    V_Clear(0xff070609);
    M_DrawArt("S04SELMO.GRA", 136, 24, 15, 1, V_FADE);
    V_FillRect(24, 35, 272, 145, 0xff100d10);
    gra_t *slabs = W_LoadGra("S04SLABS.GRA");
    patch_t *border = W_Frame(slabs, 37);
    V_DrawPatchScaled(24 + border->x * 2, 34 + border->y * 0.8f, border,
                     &slabs->palettes[W_GuessPalette(border, slabs)], 0, 2, 0.8f);
    V_FillRect(40, 45, 240, 123, 0xff100d10);
    V_DrawTextCentered(FONT_BIG, 160, 14, heading);
#ifdef CLAY_IOS
    V_DrawTextCentered(FONT_SMALL, 160, VIEWHEIGHT - 10, "D-PAD: MOVE   SELECT: CONFIRM   BACK: RETURN");
#else
    V_DrawTextCentered(FONT_SMALL, 160, VIEWHEIGHT - 10, "ARROWS / WASD: MOVE   ENTER: SELECT   ESC: BACK");
#endif
}
void M_StartTitle(void)
{
    pagetic = 0; gamestate = GS_TITLE; G_ClearInput();
}
void M_StartMenu(void)
{
    page = MENU_MAIN; cursor = 0; gamestate = GS_MENU; paused = false; G_ClearInput();
}
void M_OpenOptions(void) { M_StartMenu(); page = MENU_OPTIONS; }
void M_OpenStart(void) { M_StartMenu(); page = MENU_START; }
static void M_Back(void)
{
    if (page == MENU_MAIN) M_StartTitle();
    else if (page == MENU_START || page == MENU_OPTIONS) M_StartMenu();
    else { page = MENU_OPTIONS; cursor = 0; }
}
bool M_Responder(const event_t *ev)
{
    if (ev->type != ev_keydown) return false;
    if (gamestate == GS_RESULTS) { M_StartMenu(); return true; }
    if (ev->key == KEY_ESCAPE || ev->key == PADKEY(0, PK_BACK)) { M_Back(); return true; }
    int pl, bit;
    if (!G_Binding(ev->key, &pl, &bit)) return false;
    if (gamestate == GS_TITLE) {
        if (bit & (BT_START | BT_ATTACKS)) M_StartMenu();
        return true;
    }
    int n = page == MENU_CONFIG ? 7 : page == MENU_CONTROLS || page == MENU_VOLUME ? 1 :
            page == MENU_MAIN ? 3 : page == MENU_SOUND ? NUMSFX : 5;
    if (bit & BT_UP) cursor = (cursor + n - 1) % n;
    if (bit & BT_DOWN) cursor = (cursor + 1) % n;
    bool accept = (bit & (BT_START | BT_ATTACKS)) != 0;
    int delta = bit & BT_LEFT ? -1 : 1;
    if (page == MENU_CONFIG && (bit & (BT_LEFT | BT_RIGHT) || accept)) {
        switch (cursor) {
        case 0: options.rounds = CLAMP(options.rounds + delta, 1, 4); break;
        case 1: options.seconds = CLAMP(options.seconds + delta * 10, 30, 99); break;
        case 2: options.difficulty = CLAMP(options.difficulty + delta, 1, 3); break;
        case 3: options.credits = CLAMP(options.credits + delta, 1, 10); break;
        case 4: options.sound = !options.sound; break;
        case 5: options = (options_t){2, 60, 2, 5, true, 8}; break;
        case 6: M_Back(); break;
        }
        M_SaveOptions();
    } else if (page == MENU_VOLUME) {
        if (bit & (BT_LEFT | BT_RIGHT)) { options.volume = CLAMP(options.volume + delta, 0, 10); M_SaveOptions(); }
        if (accept) M_Back();
    } else if (page == MENU_SOUND) {
        if (bit & (BT_LEFT | BT_RIGHT)) cursor = (cursor + NUMSFX + delta) % NUMSFX;
        if (accept) S_StartSound((sfxenum_t)cursor, -1, 1);
    } else if (accept) {
        if (page == MENU_CONTROLS) M_Back();
        else if (page == MENU_START) {
            if (cursor == 4) M_Back();
            else { gamemode = (gamemode_t)cursor; SEL_Start(); }
        } else if (page == MENU_OPTIONS) {
            int destinations[] = {MENU_CONFIG, MENU_VOLUME, MENU_SOUND, MENU_CONTROLS};
            if (cursor == 4) M_Back(); else { page = destinations[cursor]; cursor = 0; }
        } else switch (cursor) {
        case 0: M_OpenStart(); break;
        case 1: M_Create(); break;
        case 2: M_OpenOptions(); break;
        }
    }
    return true;
}
void M_Ticker(void) { pagetic++; }
void M_Drawer(void)
{
    if (gamestate == GS_TITLE) {
        V_Clear(0xff000000);
        gra_t *g = W_LoadGra("S04TITLE.GRA");
        int slab = MIN(39 + pagetic / 6, 45);
        patch_t *p = W_Frame(g, slab);
        M_DrawArt("S04TITLE.GRA", slab, 160 - p->w / 2, 90 - p->h / 2, 1, 0);
        if (pagetic > 50) M_DrawArt("S04TITLE.GRA", 37, 95, 69, 1, 0);
        if (pagetic > 80) M_DrawArt("S04TITLE.GRA", 19, 76, 93, 1, 0);
        if (pagetic & 32) V_DrawTextCentered(FONT_SMALL, 160, 172, "PRESS START");
        V_DrawTextCentered(FONT_SMALL, 160, VIEWHEIGHT - 10, "CLAYFIGHT  /  PRIMAL RAGE + YOUR CLAY FIGHTERS");
        return;
    }
    const char *headings[] = {"MAIN MENU", "CONFIG OPTIONS", "MODIFY CONTROLS", "START MENU", "OPTIONS MENU", "ADJUST VOLUME", "SOUND TEST"};
    M_Background(headings[page]);
    if (page == MENU_CONTROLS) {
#ifdef CLAY_IOS
        const char *lines[] = {"D-PAD: MOVE / JUMP / CROUCH", "HQ: HIGH QUICK", "HF: HIGH FIERCE", "LQ: LOW QUICK", "LF: LOW FIERCE", "SELECT: CONFIRM CHOICE", "BACK: RETURN / PAUSE", "MENU: MAIN MENU", "HOLD TWO BUTTONS TO MOVE AND ATTACK"};
#else
        const char *lines[] = {"LEFT PLAYER: WASD", "HI QUICK / FIERCE: R / T", "LO QUICK / FIERCE: F / G", "RIGHT PLAYER: ARROWS",
            "HI QUICK / FIERCE: O / P", "LO QUICK / FIERCE: L / ;", "P2 JOINS WITH AN ATTACK", "ESC: PAUSE   Q: LEAVE PAUSED FIGHT", "GAMEPADS: D-PAD + FOUR FACE BUTTONS"};
#endif
        for (int i = 0; i < 9; i++) V_DrawTextCentered(FONT_SMALL, 160, 47 + 15 * i, lines[i]);
        return;
    }
    if (page == MENU_VOLUME) {
        V_DrawTextCentered(FONT_BIG, 160, 85, options.volume ? "GAME SAMPLES" : "MUTE");
        V_FillRect(65, 119, 190, 10, 0xff302821);
        V_FillRect(65, 119, options.volume * 19, 10, 0xffd59a46);
        V_DrawTextCentered(FONT_SMALL, 160, 151, "LEFT / RIGHT TO ADJUST VOLUME");
        V_DrawTextCentered(FONT_SMALL, 160, 175, "ENTER: SAVE AND EXIT"); return;
    }
    if (page == MENU_SOUND) {
        V_DrawTextCentered(FONT_BIG, 160, 80, sounditems[cursor]);
        V_DrawTextCentered(FONT_SMALL, 160, 119, "UP / DOWN: SELECT SAMPLE");
        V_DrawTextCentered(FONT_SMALL, 160, 143, "ENTER: PLAY");
        V_DrawTextCentered(FONT_SMALL, 160, 177, options.sound ? "ESC: OPTIONS MENU" : "GAME SAMPLES ARE MUTED IN CONFIG OPTIONS"); return;
    }
    char labels[7][64];
    if (page == MENU_CONFIG) {
        snprintf(labels[0], 64, "NUMBER OF ROUNDS    %d", options.rounds * 2 - 1);
        snprintf(labels[1], 64, "ROUND TIME          %d", options.seconds);
        snprintf(labels[2], 64, "DIFFICULTY          %s", options.difficulty == 1 ? "EASY" : options.difficulty == 2 ? "NORMAL" : "HARD");
        snprintf(labels[3], 64, "GAME CREDITS        %d", options.credits);
        snprintf(labels[4], 64, "GAME SAMPLES        %s", options.sound ? "ON" : "OFF");
        snprintf(labels[5], 64, "RESTORE DEFAULTS"); snprintf(labels[6], 64, "BACK");
    }
    int n = page == MENU_CONFIG ? 7 : page == MENU_MAIN ? 3 : 5;
    int y = page == MENU_CONFIG ? 45 : page == MENU_MAIN ? 66 : 49;
    int spacing = page == MENU_CONFIG ? 19 : page == MENU_MAIN ? 33 : 26;
    for (int i = 0; i < n; i++) {
        if (i == cursor) {
            V_FillRect(46, y - 4, 228, page == MENU_CONFIG ? 17 : 21, 0xff37241b);
            V_DrawText(FONT_SMALL, 49, y, ">");
        }
        const char *label = page == MENU_CONFIG ? labels[i] : page == MENU_START ? startitems[i] : page == MENU_OPTIONS ? optionitems[i] : mainitems[i];
        V_DrawTextCentered(page == MENU_CONFIG ? FONT_SMALL : FONT_BIG, 160, y, label);
        y += spacing;
    }
}
