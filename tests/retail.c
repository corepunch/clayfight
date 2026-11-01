// Retail select navigation and airborne art/collision regression checks.
#include <assert.h>
#include <math.h>
#include <stdio.h>

#include "game/game.h"

static void press(int key)
{
    if (key == '\t') {
        // Go through the game event queue, not just the menu responder.
        event_t keydown={ev_keydown, '\t'};
        D_PostEvent(&keydown);
        responder_t responders[] = {SEL_Responder};
        D_ProcessEvents(responders, 1);
        return;
    }
    event_t e = {ev_keydown, key};
    assert(SEL_Responder(&e));
}

static void composition_checks(void)
{
    assert(SCREENWIDTH == 672 && SCREENHEIGHT == 480);
    gra_t *g = W_LoadGra("select");
    const patch_t *bg = W_Frame(g, 136);
    const palette_t *pal = &g->palettes[14];
    int left = (VIEWWIDTH - bg->w) / 2, top = (VIEWHEIGHT - bg->h) / 2;
    V_Clear(0xff000000);
    V_DrawPatch(left,top,bg,pal,0);
    // Both complete vertical edges must survive, including the sky and floor.
    for (int y = 10; y < bg->h - 10; y++) {
        for (int side = 0; side < 2; side++) {
            int x = side ? bg->w - 1 : 0, value = bg->pixels[y * bg->w + x];
            if (!value) continue;
            for (int dy = 0; dy < 2; dy++) for (int dx = 0; dx < 2; dx++)
                assert(screen[((top + y) * 2 + dy) * SCREENWIDTH + (left + x) * 2 + dx] == pal->colors[value]);
        }
    }
    SEL_Drawer();
    I_SaveScreenshot(screen, "build/retail-composition.bmp");
    // Both original podiums are visible at exactly 2x, unobscured by UI.
    for (int pl = 0; pl < 2; pl++) {
        const patch_t *p = W_Frame(g, 105 + pl);
        int px = left + (pl ? 252 : 84) - p->w / 2, py = 200;
        const palette_t *podium_pal=&g->palettes[W_FramePalette(g,105+pl)];
        for (int y = 5; y < 19; y++) for (int x = 0; x < p->w; x++) {
            int value = p->pixels[y * p->w + x];
            if (!value) continue;
            for (int dy = 0; dy < 2; dy++) for (int dx = 0; dx < 2; dx++)
                assert(screen[((py + y) * 2 + dy) * SCREENWIDTH + (px + x) * 2 + dx] == podium_pal->colors[value]);
        }
    }
}

static void launch(void)
{
    for (int i = 0; i < 50; i++) SEL_Ticker();
    assert(gamestate == GS_FIGHT);
}

static void shot(const char *path)
{
    SEL_Drawer();
    I_SaveScreenshot(screen, path);
}

static void select_checks(void)
{
    SEL_Reset();
    gamemode = GM_TUG;
    SEL_Start();
    press('s'); // Chaos fits within the temple edges being checked below.
    composition_checks();
    press('w');
    press('o');
    press('s'); press('d'); press('d'); // P1: Diablo, lower-right
    press(KEY_RIGHTARROW); // P2: Talon, second tile on the top row
    shot("build/retail-versus-podiums.bmp");
    press(KEY_ENTER); press('o'); launch();
    assert(match.defs[0]->codeindex == 5 && match.defs[1]->codeindex == 2);

    SEL_Start();
    for (int i = 0; i < 6; i++) press('a'); // Diablo -> Sauron
    press('s'); // the lower-left relief is Chaos, below Sauron
    shot("build/retail-chaos.bmp");
    press('d'); // Armadon, the lower middle relief in the native temple table
    press(KEY_ENTER); launch();
    assert(match.defs[0]->codeindex == X_U8(0x6aca6+5));

    SEL_Start();
    press('w'); press('d'); // Armadon -> Talon -> Vertigo
    press('o'); // P2 joins on its current original tile
    press(KEY_ENTER); press('o'); launch();
    assert(match.defs[0]->codeindex == X_U8(0x6aca6+2) && !match.cpu[1]);

    SEL_Start(); press('\t'); // first clay page
    shot("build/retail-clay.bmp");
    press(KEY_ENTER); launch();
    assert(match.defs[0] == roster[7] && match.defs[0]->codeindex < 0);

    // Fill the roster, including a final page with just one occupied tile.
    int saved = numroster;
    while (numroster < MAXROSTER) { roster[numroster] = roster[7]; numroster++; }
    SEL_Start();
    for (int i = 0; i < 8; i++) press('\t');
    shot("build/retail-last-page.bmp");
    press('d'); // skip empty cells and wrap back to Sauron
    press(KEY_ENTER); launch();
    assert(match.defs[0]->codeindex == 0);
    numroster = saved;
}

static void air_checks(void)
{
    bool cpu[2] = {false, false};
    G_InitNew(roster[1], roster[2], cpu, &stages[2]);
    fighter_t *f = &level.fighters[0];
    level.fighters[1].x = -10000;
    level.camx = 0;
    int checked = 0;
    for (int i = 0; i < numroster; i++) {
        P_SpawnFighter(f, roster[i], 0, 160, roster[i]->palette);
        bool seen[12000]={0};
        for (int s = 0; s < f->def->numstates; s++) {
            if (f->def->states[s].kind != K_AIR) continue;
            f->state = &f->def->states[s];
            if (f->state->frame>=0 && f->state->frame<12000) {
                if (seen[f->state->frame]) continue;
                seen[f->state->frame]=true;
            }
            if (!P_StatePatch(f->state)) continue;
            // Test takeoff, apex, and just before landing in both directions.
            const int heights[] = {0, 55, 1};
            for (int h = 0; h < 3; h++) {
                f->z = heights[h];
                f->facing = h & 1 ? -1 : 1;
                f->flash = 2; // solid white exposes every rendered body pixel
                box_t hurt;
                assert(P_HurtBox(f, &hurt) && hurt.z0 >= f->z);
                R_RenderFight();
                int bottom = -1;
                for (int y = 0; y < SCREENHEIGHT; y++)
                    for (int x = 0; x < SCREENWIDTH; x++)
                        if (screen[y * SCREENWIDTH + x] == 0xffffffff) bottom = y;
                assert(bottom >= 0);
                assert(bottom < (FLOOR_Y - heights[h]) * PIXELS_PER_UNIT);
                checked++;
            }
        }
    }
    // Reproduce the reported Blizzard/Talon scene just above the floor.
    G_InitNew(roster[1], roster[2], cpu, &stages[2]);
    f = &level.fighters[1];
    P_SetAnim(f, ANIM_JUMP);
    f->state = &f->def->states[f->def->anims[ANIM_JUMP].first + f->def->anims[ANIM_JUMP].count - 1];
    f->z = 8;
    R_RenderFight(); ST_Drawer(match.wins);
    I_SaveScreenshot(screen, "build/retail-jump.bmp");
    printf("air: %d rendered takeoff/apex/landing poses and hurt boxes stay above their foot plane\n", checked);
}

void CF_RetailChecks(void)
{
    options.sound = false;
    assert(numroster >= 12);
    assert(roster[3]->palette == 12); // Vertigo's player object in PRAGE.EXE
    for (int i = 0; i < 7; i++) assert(roster[i]->palette == i*4);
    gra_t *select = W_LoadGra("select");
    assert(W_FramePalette(select, 91) == 4 && W_FramePalette(select, 130) == 13);
    assert(W_FramePalette(select, 149) == 18 && W_FramePalette(select, 136) == 14);
    for (int i = 0; i < select->numframes; i++) {
        const patch_t *p = W_Frame(select, i);
        if (p) assert(p->maxval <= select->palettes[W_FramePalette(select, i)].count);
    }
    select_checks();
    air_checks();

    puts("retail: 672x480, complete 2x temple, podiums, Tab, navigation, clay pages, P2 and palettes passed");
}


#ifndef CLAY_COMBINED
int main(int argc,char **argv) {
    myargc=argc;myargv=argv;I_InitGraphics(true);W_Init("data/primrage");
    PS_Init();V_Init();P_LoadRoster("data/creatures");
    CF_RetailChecks();I_Shutdown();return 0;
}
#endif
