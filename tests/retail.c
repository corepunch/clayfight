// Retail select navigation and airborne art/collision regression checks.
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include "game/game.h"

static void press(int key)
{
    event_t e = {ev_keydown, key};
    assert(SEL_Responder(&e));
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
    gamemode = GM_TUG;
    SEL_Start();
    press('s'); // the lower-left relief is Chaos, below Sauron
    shot("build/retail-chaos.bmp");
    press('d'); // Vertigo, the lower middle relief
    press(KEY_ENTER); launch();
    assert(match.defs[0]->codeindex == 3);

    SEL_Start();
    press('w'); press('d'); // Vertigo -> Diablo -> Armadon
    press('o'); // P2 joins on its current original tile
    press(KEY_ENTER); press('o'); launch();
    assert(match.defs[0]->codeindex == 4 && !match.cpu[1]);

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
        for (int s = 0; s < f->def->numstates; s++) {
            if (f->def->states[s].kind != K_AIR) continue;
            f->state = &f->def->states[s];
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

int main(int argc, char **argv)
{
    myargc = argc; myargv = argv;
    I_InitGraphics(true); W_Init("primalrage"); V_Init(); P_LoadRoster("creatures");
    options.sound = false;
    assert(numroster >= 12);
    assert(roster[3]->palette == 1); // Vertigo's player object in PRAGE.EXE
    for (int i = 0; i < 7; i++) if (i != 3) assert(roster[i]->palette == 3);
    gra_t *select = W_LoadGra("S04SELMO.GRA");
    assert(W_FramePalette(select, 91) == 4 && W_FramePalette(select, 130) == 13);
    assert(W_FramePalette(select, 149) == 18 && W_FramePalette(select, 136) == 14);
    for (int i = 0; i < select->numframes; i++) {
        const patch_t *p = W_Frame(select, i);
        if (p) assert(p->maxval <= select->palettes[W_FramePalette(select, i)].count);
    }
    select_checks();
    air_checks();
    I_Shutdown();
    puts("retail: temple navigation, clay pages, full roster, P2 and original palettes passed");
    return 0;
}
