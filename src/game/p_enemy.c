// p_enemy.c -- weighted, committed joystick macros through the human rules.
#include <math.h>
#include "game/game.h"

typedef struct {
    int spacing; // offset from body contact distance
    int jump, crouch, power; // chances out of 256 at a decision boundary
} cpustyle_t;

// Port-authored tendencies, not a reconstruction of the original AI program.
// The clay roster uses the balanced profile rather than a character-name test.
static const cpustyle_t styles[] = {
    {8, 20, 55, 80},   // Sauron: heavy close pressure
    {14, 42, 65, 55},  // Blizzard: mixed pressure
    {0, 85, 85, 38},   // Talon: mobile rushdown
    {34, 28, 50, 45},  // Vertigo: reach and distance
    {18, 30, 90, 100}, // Armadon: crouching power
    {28, 65, 35, 65},  // Diablo: distance and jumps
    {10, 60, 80, 50},  // Chaos: mobile low pressure
};
static const cpustyle_t balanced = {12, 40, 65, 50};

static int P_CpuAttack(const cpustyle_t *style, bool punish)
{
    int low = P_Random() & 1;
    if (P_Random() < style->power + (punish ? 55 : 0))
        return low ? BT_LQ | BT_LF : BT_HQ | BT_HF;
    bool fierce = P_Random() < (punish ? 190 : 90);
    return low ? (fierce ? BT_LF : BT_LQ) : (fierce ? BT_HF : BT_HQ);
}

void P_CpuCmd(fighter_t *f, const fighter_t *opp, ticcmd_t *cmd)
{
    cmd->buttons = 0;
    if (f->dead || opp->dead || level.phase != PH_FIGHT || level.hitstop > 0)
        return;
    int difficulty = CLAMP(options.difficulty, 1, 3);
    int reaction = 22 - difficulty * 5;
    int toward = opp->x > f->x ? BT_RIGHT : BT_LEFT;
    int away = toward == BT_RIGHT ? BT_LEFT : BT_RIGHT;
    float dist = fabsf(opp->x - f->x);
    int c = f->def->codeindex;
    const cpustyle_t *style = c >= 0 && c < 7 ? &styles[c] : &balanced;
    float contact = f->def->reach + opp->def->reach + 8;

    if (f->dizzy) {
        // Alternate at a human-sized cadence, never every simulation tick.
        cmd->buttons = (level.leveltime / reaction) & 1 ? BT_LEFT : BT_RIGHT;
        return;
    }
    if (f->aiwait > 0)
        f->aiwait--;
    if (f->state->kind == K_HIT || f->state->kind == K_DOWN || f->dizzypending) {
        f->aihold = f->aiplan = 0;
        return;
    }
    if (f->aihold > 0) {
        f->aihold--;
        // Macros store logical toward/away so a side switch never reverses guard.
        int plan = f->aiplan;
        cmd->buttons = (plan & ~(BT_LEFT | BT_RIGHT)) |
            ((plan & BT_RIGHT) ? toward : (plan & BT_LEFT) ? away : 0);
        return;
    }
    if (f->aiwait > 0)
        return;
    f->aiwait = reaction + (P_Random() & 3);

    if (P_IsAirborne(f)) {
        if (!f->airattackused && dist < contact + 35 && f->momz < 3)
            cmd->buttons = P_CpuAttack(style, false);
        return;
    }
    if (f->state->kind != K_FREE)
        return;

    bool threat = opp->attack && !opp->attackhit && dist < contact + 45;
    if (threat && P_Random() < 90 + difficulty * 40) {
        // Decide once, commit for a reaction interval. No fresh guard roll or
        // high/low correction on every tick of the opponent's attack.
        f->aiplan = BT_LEFT | (opp->attack->level == LOW ? BT_DOWN : 0);
    } else if (dist > contact + style->spacing + 12) {
        f->aiplan = BT_RIGHT;
        if (dist < contact + 90 && P_Random() < style->jump)
            cmd->buttons = BT_UP | toward;
    } else if (dist < contact + 20) {
        bool punish = opp->dizzy || (opp->state->kind == K_ATTACK && opp->attackhit);
        if (punish || P_Random() < 170 + difficulty * 15) {
            cmd->buttons = P_CpuAttack(style, punish);
            if (P_Random() < style->crouch)
                cmd->buttons |= BT_DOWN;
            f->aiwait += reaction; // release buttons; recover before the next decision
            return;
        }
        f->aiplan = BT_LEFT;
    } else {
        f->aiplan = P_Random() < 100 ? BT_RIGHT : BT_LEFT;
    }
    if (cmd->buttons) { // takeoff is one pulse, not a held auto-jump
        f->aiplan = 0;
        return;
    }
    f->aihold = reaction - 1;
    cmd->buttons = (f->aiplan & BT_DOWN) | ((f->aiplan & BT_RIGHT) ? toward : away);
}
