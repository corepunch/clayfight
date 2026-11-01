// d_main.c -- startup and the main loop (cf. d_main.c: D_DoomMain, D_DoomLoop, D_Display)
//
// Command line (Doom style):
//   -data <dir>         Primal Rage 2.3 arcade ROM chips (default: data/primrage)
//   -fight <p1> <p2>    skip menus straight into a fight, e.g. -fight sauron sauron
//   -cpu                both fighters CPU-controlled (attract mode)
//   -stage <n>          arena index for -fight
//   -warp select        start on the select screen
//   -shots <prefix>     headless: save <prefix>NNNNN.bmp every -every tics,
//   -every <n>            and quit after -tics tics
//   -tics <n>
//   -nosound
//   -sheet NAME         with -shots: draw every frame of a creature's art (hotspots marked)
//   -checkanims         validate arcade animations, frames and palettes and quit
//   -boxes              draw hurt (green) and attack (red) boxes
//   -creatures <dir>    creature files to load, also one folder down (default: creatures)
//   Retail fighters always run arcade bytecode; photos use authored states.

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

#include "game/game.h"

#define MAXTICS 8 // never simulate more than this per frame

static const char *shotprefix;
static int shotevery = 60, maxtics;

void D_Display(void)
{
    switch (gamestate) {
    case GS_MENU:
    case GS_TITLE:
        M_Drawer();
        break;
    case GS_SELECT:
        SEL_Drawer();
        break;
    case GS_RESULTS:
        E_Drawer(); break;
    case GS_MAP: case GS_CONTINUE: case GS_ENDING:
        C_Drawer(); break;
    case GS_CREATE:
        M_CreateDrawer(); break;
    case GS_FIGHT:
        R_RenderFight();
        ST_Drawer(match.wins);
        HU_Drawer();
        if (paused)
            V_DrawTextCentered(FONT_BIG, VIEWWIDTH / 2, 80, "PAUSED");
        break;
    }
    I_FinishUpdate(screen);
}

void D_RunTic(void)
{
    I_StartTic();
    responder_t list[2] = {G_Responder, NULL};
    int n = 1;
    if (gamestate == GS_TITLE || gamestate == GS_MENU || gamestate == GS_RESULTS)
        list[n++] = M_Responder;
    else if (gamestate == GS_SELECT)
        list[n++] = SEL_Responder;
    else if (gamestate == GS_CREATE)
        list[n++] = M_CreateResponder;
    else if (gamestate == GS_MAP || gamestate == GS_CONTINUE || gamestate == GS_ENDING)
        list[n++] = C_Responder;
    D_ProcessEvents(list, n);

    switch (gamestate) {
    case GS_MENU:
    case GS_TITLE:
        M_Ticker();
        break;
    case GS_SELECT:
        SEL_Ticker();
        break;
    case GS_RESULTS: break;
    case GS_MAP: case GS_CONTINUE: case GS_ENDING:
        C_Ticker(); break;
    case GS_CREATE:
        M_CreateTicker(); break;
    case GS_FIGHT:
        G_Ticker();
        break;
    }
    if (gameaction == ga_matchover) {
        gameaction = ga_nothing;
        if (endurance) E_MatchOver(match.wins[0] == match.wins[1] ? 2 : match.wins[0] > match.wins[1] ? 0 : 1);
        else if (campaign.active) C_MatchOver(match.wins[0] >= (campaign.finalbattle ? 1 : options.rounds));
        else SEL_Start();
    }
}

// -sheet: every frame of a creature's art on one screen, hotspots marked.
static void D_DrawSheet(const fighterdef_t *def)
{
    V_Clear(0xff283040);
    gra_t *g = def->base;
    int n = g->numframes, cols = 1;
    while (cols * cols * 1.6 < n)
        cols++;
    int rows = (n + cols - 1) / cols, cw = VIEWWIDTH / cols, ch = (VIEWHEIGHT - 10) / rows;
    for (int i = 0; i < n; i++) {
        patch_t *p = W_Frame(g, i);
        if (!p)
            continue;
        float sc = MIN(1.f, MIN((float)(cw - 4) / p->w, (float)(ch - 4) / p->h));
        int x = (i % cols) * cw + cw / 2 + (int)((p->x - p->w / 2.f) * sc), y = 10 + (i / cols + 1) * ch - 2;
        V_DrawPatchScaled(x, y, p, &g->palettes[def->palette], 0, sc, sc);
        V_FillRect(x - 2, y, 5, 1, 0xff00ff00);
        V_FillRect(x, y - 2, 1, 5, 0xff00ff00);
    }
    V_DrawText(FONT_SMALL, 2, 2, def->name);
}

static fighterdef_t *D_FindFighter(const char *name)
{
    for (int i = 0; i < numroster; i++)
        if (!strcasecmp(roster[i]->name, name) || !strcasecmp(roster[i]->prefix, name))
            return roster[i];
    I_Error("unknown fighter '%s'", name);
}

// Headless run for testing: fixed number of tics, periodic screenshots.
static void D_RunHeadless(void)
{
    char path[512];
    for (int tic = 0; tic < maxtics; tic++) {
        D_RunTic();
        if (tic % shotevery == 0) {
            D_Display();
            snprintf(path, sizeof path, "%s%05d.bmp", shotprefix, tic);
            I_SaveScreenshot(screen, path);
        }
    }
}

static void D_DoomLoop(void)
{
    int lasttic = I_GetTime();
    while (!I_QuitRequested()) {
        int now = I_GetTime();
        int n = MIN(now - lasttic, MAXTICS);
        lasttic = now;
        while (n-- > 0)
            D_RunTic();
        D_Display(); // presents with vsync, which paces the loop
        if (now == I_GetTime())
            I_Sleep(1);
    }
}

int D_InitGame(int argc, char **argv)
{
    myargc = argc;
    myargv = argv;
    shotprefix = M_ParmValue("-shots", NULL);
    bool headless = shotprefix || M_CheckParm("-checkanims");
    if (shotprefix) {
        shotevery = atoi(M_ParmValue("-every", "60"));
        maxtics = atoi(M_ParmValue("-tics", "600"));
    }

    r_showboxes = M_CheckParm("-boxes") != 0;
    I_InitGraphics(headless);
    W_Init(M_ParmValue("-data", "data/primrage"));
    if (!PS_Init()) I_Error("Arcade script table is unavailable");
    p_scripted = true;
    V_Init();
    M_LoadOptions();
    P_LoadRoster(M_ParmValue("-creatures", "data/creatures"));
    SEL_Reset();
    if (M_CheckParm("-checkanims")) {
        int failed = P_CheckAnims(); I_Shutdown(); return failed ? 1 : 0;
    }
    if (!M_CheckParm("-nosound") && !headless)
        S_Init();

    int p = M_CheckParm("-fight");
    if (p && p + 2 < argc) {
        bool cpu[2] = {M_CheckParm("-cpu") != 0, true};
        int s = atoi(M_ParmValue("-stage", "0")) % numstages;
        G_InitNew(D_FindFighter(argv[p + 1]), D_FindFighter(argv[p + 2]), cpu, &stages[s]);
    } else if (!strcasecmp(M_ParmValue("-warp", ""), "select")) {
        SEL_Start();
    } else if (!strcasecmp(M_ParmValue("-warp", ""), "map")) {
        if (!C_Start(D_FindFighter(M_ParmValue("-hero", "boxer")))) I_Error("Conquest needs all seven original fighters");
    } else if (!strcasecmp(M_ParmValue("-warp", ""), "menu")) {
        M_StartMenu();
    } else if (!strcasecmp(M_ParmValue("-warp", ""), "start")) {
        M_OpenStart();
    } else if (!strcasecmp(M_ParmValue("-warp", ""), "options")) {
        M_OpenOptions();
    } else {
        M_StartTitle();
    }

    return -1; // initialized; native hosts drive ticks and display themselves
}

#if !defined(CLAY_IOS) || defined(CLAY_HOST)
int clay_cli_main(int argc, char **argv)
{
    int result = D_InitGame(argc, argv);
    if (result >= 0) return result;
    const char *sheet = M_ParmValue("-sheet", NULL);
    if (sheet && shotprefix) {
        char path[600];
        D_DrawSheet(D_FindFighter(sheet));
        snprintf(path, sizeof path, "%ssheet.bmp", shotprefix);
        I_SaveScreenshot(screen, path);
    } else if (shotprefix)
        D_RunHeadless();
    else
        D_DoomLoop();

    I_Shutdown();
    return 0;
}

#endif
