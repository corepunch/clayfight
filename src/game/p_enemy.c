// p_enemy.c -- CPU opponent (cf. p_enemy.c)
//
// Instead of steering the fighter directly, the AI fills in a ticcmd just as
// a player's controls would, so CPU and humans share every rule.

#include <math.h>

#include "game/game.h"

static const int attacks[] = {BT_HQ, BT_HQ, BT_LQ, BT_HF, BT_LF};

void P_CpuCmd(fighter_t *f, const fighter_t *opp, ticcmd_t *cmd)
{
    cmd->buttons = 0;
    if (f->dead || opp->dead)
        return;

    int toward = opp->x > f->x ? BT_RIGHT : BT_LEFT;
    int away = toward == BT_RIGHT ? BT_LEFT : BT_RIGHT;
    float dist = fabsf(opp->x - f->x);
    bool close = dist < f->def->reach + opp->def->reach + 16; // fronts nearly touching

    // guard against an incoming attack (sometimes too late)
    if (opp->state->kind == K_ATTACK && dist < 140 && P_Random() < 60 + options.difficulty * 40) {
        cmd->buttons = away;
        if (opp->attack && opp->attack->level == LOW)
            cmd->buttons |= BT_DOWN;
        return;
    }
    if (f->aihold > 0) {
        f->aihold--;
        cmd->buttons = f->aiplan;
        return;
    }
    if (f->state->kind != K_FREE)
        return;
    if (f->aiwait > 0) {
        f->aiwait--;
        return;
    }

    int r = P_Random();
    if (close) {
        if (r < 120) {
            cmd->buttons = attacks[P_Random() % 5] | (P_Random() < 60 ? BT_DOWN : 0);
            f->aiwait = (10 + (P_Random() & 15)) * (4 - options.difficulty) / 2;
        } else if (r < 170) {
            f->aiplan = away;
            f->aihold = 12 + (P_Random() & 15);
        } else if (r < 185) {
            cmd->buttons = BT_UP | away;
        } else {
            f->aiwait = 6;
        }
    } else if (r < 200) {
        f->aiplan = toward;
        f->aihold = 10 + (P_Random() & 31);
    } else if (r < 215) {
        cmd->buttons = BT_UP | toward;
    } else {
        f->aiwait = 15;
    }
}
