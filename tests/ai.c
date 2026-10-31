// Runtime integration and reset checks. Original-byte parity is tested separately.
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "game/game.h"

static void input_channels(void)
{
    const bool cpu[] = {true, true};
    G_InitNew(roster[0], roster[2], cpu, &stages[0]);
    fighter_t *f = &level.fighters[0], *opp = &level.fighters[1];
    // An original edge can repeat even when its held bit did not change.
    f->oldbuttons = BT_HQ;
    f->cmd = (ticcmd_t){.buttons = BT_HQ, .pressed = BT_HQ, .explicit_edges = true};
    P_PlayerThink(f, opp);
    assert(f->attack && f->move >= 0);
    P_SetAnim(f, ANIM_STAND);
    f->oldbuttons = 0;
    f->cmd = (ticcmd_t){.buttons = BT_HQ, .explicit_edges = true};
    P_PlayerThink(f, opp);
    assert(!f->attack); // held without edge must not manufacture an attack
    f->cmd = (ticcmd_t){.pressed = BT_HQ, .explicit_edges = true};
    P_PlayerThink(f, opp);
    assert(f->attack); // edge without held must still work
    assert(PA_Input(0x30000 | 0x101, false) == 0x1111);
    assert(PA_Input(0x30000 | 0x101, true) == 0x2121);
    // Script mode must expose the move ID to the same CPU observer.
    assert(PS_Init());
    p_scripted = true;
    assert(P_StartMove(f, 3, ANIM_STAND) && f->move == 3);
    assert(P_SetAnim(f, ANIM_STAND) && f->move == -1);
    p_scripted = false;
    puts("ai: original edge/held channels, facing and script move IDs passed");
}

static void lifecycle(void)
{
    const bool cpu[] = {true, true};
    G_InitNew(roster[0], roster[2], cpu, &stages[0]);
    fighter_t *f = &level.fighters[0], *opp = &level.fighters[1];
    prai_t zero = {0};
    P_CpuCmd(f, opp, &f->cmd);
    assert(!memcmp(&f->ai, &zero, sizeof zero)); // intro consumes no AI tics
    level.phase = PH_FIGHT;
    G_Ticker();
    assert(f->ai.decisions == 1 && f->ai.macro);
    prai_t saved = f->ai;
    level.hitstop = 3;
    G_Ticker();
    assert(!memcmp(&f->ai, &saved, sizeof saved));
    level.hitstop = 0;
    paused = true;
    G_Ticker();
    assert(!memcmp(&f->ai, &saved, sizeof saved));
    paused = false;
    gamemode = GM_TRAINING;
    saved = opp->ai;
    G_Ticker();
    assert(!memcmp(&opp->ai, &saved, sizeof saved));
    gamemode = GM_ARCADE;
    P_SetupRound(match.defs, match.palettes, &stages[0], 2);
    assert(!memcmp(&level.fighters[0].ai, &zero, sizeof zero));
    assert(!memcmp(&level.fighters[1].ai, &zero, sizeof zero));
    puts("ai: intro, pause, hitstop, training and round reset passed");
}

static void matches(void)
{
    const bool cpu[] = {true, true};
    unsigned decisions[7] = {0}, attacks = 0;
    for (int c = 0; c < 7; c++) {
        for (int o = 0; o < 7; o++) {
            for (int difficulty = 1; difficulty <= 3; difficulty++) {
                options.difficulty = difficulty;
                G_InitNew(roster[c], roster[o], cpu, &stages[0]);
                level.phase = PH_FIGHT;
                for (int t = 0; t < 480 && level.phase == PH_FIGHT; t++) {
                    G_Ticker();
                    for (int p = 0; p < 2; p++) {
                        fighter_t *f = &level.fighters[p];
                        assert(f->cmd.explicit_edges);
                        assert(f->ai.profile < 3);
                        assert(f->ai.context < 139);
                        attacks += f->attack != NULL;
                    }
                }
                assert(level.fighters[0].ai.decisions && level.fighters[1].ai.decisions);
                decisions[c] += level.fighters[0].ai.decisions;
            }
        }
    }
    for (int c = 0; c < 7; c++) assert(decisions[c] > 21);
    assert(attacks > 1000);
    // A photographed fighter inherits an explicitly documented original style.
    assert(numroster > 7);
    G_InitNew(roster[7], roster[0], cpu, &stages[0]);
    level.phase = PH_FIGHT;
    G_Ticker();
    assert(level.fighters[0].ai.decisions == 1);
    puts("ai: 49 matchups x 3 difficulties, attacks and photographed-fighter fallback passed");
}

int main(int argc, char **argv)
{
    myargc = argc; myargv = argv;
    I_InitGraphics(true); W_Init("primalrage"); V_Init(); P_LoadRoster("creatures");
    assert(X_Init("primalrage/PRAGE.EXE") && PA_Init());
    options.sound = false;
    input_channels();
    lifecycle();
    matches();
    I_Shutdown();
    puts("PASS: original AI runtime integration");
    return 0;
}
