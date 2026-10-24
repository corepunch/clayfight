// p_tick.c -- the fight simulation, one tic at a time (cf. p_tick.c, p_setup.c)

#include <string.h>

#include "game/game.h"

level_t level;

// P_SetupLevel: spawn both fighters for a new round.
void P_SetupRound(fighterdef_t *defs[2], const int palettes[2], stage_t *stage, int round)
{
    memset(&level, 0, sizeof level);
    P_SpawnFighter(&level.fighters[0], defs[0], 0, ARENA_WIDTH / 2 - 70, palettes[0]);
    P_SpawnFighter(&level.fighters[1], defs[1], 1, ARENA_WIDTH / 2 + 70, palettes[1]);
    level.stage = stage;
    level.camx = (ARENA_WIDTH - VIEWWIDTH) / 2;
    level.time = gamemode == GM_TRAINING ? -1 : options.seconds * TICRATE;
    if (gamemode == GM_TUG) {
        for (int i = 0; i < 2; i++) level.fighters[i].health = level.fighters[i].shownhealth = MAX_HEALTH / 2;
    }
    level.phase = PH_INTRO;
    level.winner = -1;
    level.round = round;
}

static void P_SetPhase(phase_t ph)
{
    level.phase = ph;
    level.phasetime = 0;
}

static void P_MoveCamera(const fighter_t *a, const fighter_t *b)
{
    float mid = (a->x + b->x) / 2;
    float target = CLAMP(mid - VIEWWIDTH / 2, 0, ARENA_WIDTH - VIEWWIDTH);
    level.camx += (target - level.camx) * 0.2f;
}

void P_Ticker(void)
{
    fighter_t *a = &level.fighters[0], *b = &level.fighters[1];
    level.phasetime++;

    if (level.hitstop > 0) { // impact freeze: nothing moves
        level.hitstop--;
        return;
    }
    level.leveltime++;

    bool controlled = level.phase == PH_FIGHT;
    if (!controlled)
        a->cmd.buttons = b->cmd.buttons = 0;

    P_PlayerThink(a, b);
    P_PlayerThink(b, a);
    P_FighterThinker(a);
    P_FighterThinker(b);
    P_SeparateFighters(a, b);
    if (controlled) { // look first, then apply: a trade hits both ways
        int olda = a->health, oldb = b->health;
        hit_t ab, ba;
        bool hitb = P_AttackLands(a, b, &ab), hita = P_AttackLands(b, a, &ba);
        if (hitb)
            P_ApplyHit(a, b, &ab);
        if (hita)
            P_ApplyHit(b, a, &ba);
        if (gamemode == GM_TUG) {
            int share = CLAMP(olda - (olda - a->health) + (oldb - b->health), 0, MAX_HEALTH);
            a->health = share; b->health = MAX_HEALTH - share;
            a->dead = a->health == 0; b->dead = b->health == 0;
        } else if (gamemode == GM_TRAINING) {
            a->health = b->health = MAX_HEALTH;
            a->dead = b->dead = false;
        }
    }
    P_RunParticles();
    P_MoveCamera(a, b);

    switch (level.phase) {
    case PH_INTRO:
        if (level.phasetime > 110)
            P_SetPhase(PH_FIGHT);
        break;
    case PH_FIGHT:
        if (level.time > 0)
            level.time--;
        if (a->dead || b->dead || level.time == 0) {
            if (a->dead && b->dead)
                level.winner = 2;
            else if (a->dead)
                level.winner = 1;
            else if (b->dead)
                level.winner = 0;
            else
                level.winner = a->health == b->health ? 2 : a->health > b->health ? 0 : 1;
            S_StartSound(sfx_crowd, -1, 0.8f);
            P_SetPhase(PH_KO);
        }
        break;
    case PH_KO:
        if (level.phasetime == 60 && level.winner < 2) {
            fighter_t *w = &level.fighters[level.winner];
            P_SetAnim(w, ANIM_WIN);
            P_Voice(w, 0, 1);
        }
        if (level.phasetime > 240)
            P_SetPhase(PH_OVER);
        break;
    case PH_OVER:
        break;
    }
}
