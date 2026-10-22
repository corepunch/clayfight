// st_stuff.c -- fight status bar: health, names, clock, round wins (cf. st_stuff.c)

#include <math.h>
#include <stdio.h>

#include "pr_def.h"
#include "p_local.h"
#include "st_stuff.h"
#include "v_video.h"

#define BAR_W 128
#define BAR_H 7
#define BAR_Y 8

void ST_Drawer(const int wins[2])
{
    for (int i = 0; i < 2; i++) {
        const fighter_t *f = &level.fighters[i];
        bool left = i == 0;
        int x = left ? 8 : SCREENWIDTH - 8 - BAR_W;
        V_FillRect(x - 1, BAR_Y - 1, BAR_W + 2, BAR_H + 2, 0xff000000);
        V_FillRect(x, BAR_Y, BAR_W, BAR_H, 0xff601a1a);
        int shown = (int)lroundf(f->shownhealth * BAR_W / MAX_HEALTH);
        int real = f->health * BAR_W / MAX_HEALTH;
        V_FillRect(left ? x : x + BAR_W - shown, BAR_Y, shown, BAR_H, 0xffffe0e0);
        V_FillRect(left ? x : x + BAR_W - real, BAR_Y, real, BAR_H, f->health > 30 ? 0xfff0c820 : 0xfff03020);
        V_FillRect(left ? x : x + BAR_W - real, BAR_Y, real, 2, 0xfffff0a0);

        const char *name = f->def->name;
        V_DrawText(FONT_SMALL, left ? x : x + BAR_W - V_TextWidth(FONT_SMALL, name), BAR_Y + BAR_H + 3, name);

        for (int k = 0; k < ROUNDS_TO_WIN; k++) { // round win markers
            int mx = left ? x + BAR_W - 6 - k * 8 : x + 1 + k * 8;
            V_FillRect(mx, BAR_Y + BAR_H + 3, 5, 5, 0xff000000);
            if (wins[i] > k)
                V_FillRect(mx + 1, BAR_Y + BAR_H + 4, 3, 3, 0xffffd030);
        }
    }
    char clock[8];
    snprintf(clock, sizeof clock, "%02d", (level.time + TICRATE - 1) / TICRATE);
    V_DrawTextCentered(FONT_BIG, SCREENWIDTH / 2, 5, clock);
}

// --- centre-screen announcements (cf. hu_stuff.c) ---------------------------

void HU_Drawer(void)
{
    int cx = SCREENWIDTH / 2, t = level.phasetime;
    char msg[48];
    if (level.phase == PH_INTRO) {
        if (t < 60) {
            snprintf(msg, sizeof msg, "ROUND %d", level.round);
            V_DrawTextCentered(FONT_BIG, cx, 70, msg);
        } else if (t & 8) {
            V_DrawTextCentered(FONT_BIG, cx, 70, "FIGHT!");
        }
    } else if (level.phase == PH_KO) {
        if (t < 60)
            V_DrawTextCentered(FONT_BIG, cx, 70, level.time == 0 ? "TIME" : level.winner == 2 ? "DRAW" : "KO!");
        else if (level.winner < 2) {
            snprintf(msg, sizeof msg, "%s WINS", level.fighters[level.winner].def->name);
            V_DrawTextCentered(FONT_BIG, cx, 60, msg);
        } else {
            V_DrawTextCentered(FONT_BIG, cx, 60, "NO WINNERS");
        }
    }
}
