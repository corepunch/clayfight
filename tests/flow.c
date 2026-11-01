// Exercise real menu input, round completion, conquest and the final gauntlet.
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "game/game.h"
static void press(responder_t respond, int key)
{
    event_t e = {ev_keydown, key}; respond(&e);
}
static void winround(int winner)
{
    level.phase = PH_OVER; level.winner = winner; G_Ticker();
}
static void finishmatch(int winner)
{
    int n = campaign.finalbattle ? 1 : options.rounds;
    for (int i = 0; i < n; i++) winround(winner);
    assert(gameaction == ga_matchover); gameaction = ga_nothing;
    C_MatchOver(match.wins[0] >= n);
}
void CF_FlowChecks(void)
{
    options.sound = false;
    assert(numroster >= 12);
    for (int i = 0; i < 7; i++) assert(roster[i]->codeindex == i);
    M_StartTitle(); press(M_Responder, KEY_ENTER); assert(gamestate == GS_MENU);
    press(M_Responder, KEY_DOWNARROW); press(M_Responder, KEY_ENTER); assert(gamestate == GS_CREATE);
    press(M_CreateResponder, KEY_ESCAPE); assert(gamestate == GS_MENU);
    press(M_Responder, KEY_ENTER); assert(gamestate == GS_MENU);
    press(M_Responder, KEY_ENTER); assert(gamestate == GS_SELECT);
    press(SEL_Responder, KEY_ENTER);
    for (int i = 0; i < 50; i++) SEL_Ticker();
    assert(gamestate == GS_MAP && campaign.active);
    assert(campaign.order[6] == 0); // mirror last for Sauron
    C_Abandon();
    fighterdef_t *hero = NULL;
    for (int i = 0; i < numroster; i++) if (!strcmp(roster[i]->name, "BOXER")) hero = roster[i];
    assert(hero && C_Start(hero));
    C_StartFight(); finishmatch(1); assert(gamestate == GS_CONTINUE);
    int credits = campaign.credits;
    press(C_Responder, KEY_ENTER); assert(gamestate == GS_FIGHT && campaign.credits == credits - 1);
    assert(campaign.conquered == 0);
    for (int i = 0; i < 7; i++) {
        if (i) C_StartFight();
        assert(match.defs[1] == campaign.opponents[campaign.order[i]]);
        finishmatch(0); assert(gamestate == GS_MAP && campaign.conquered == i + 1);
    }
    assert(campaign.finalbattle);
    C_StartFight(); assert(level.time == -1 && level.fighters[0].health == 100);
    level.fighters[1].health = 37;
    finishmatch(1); assert(gamestate == GS_CONTINUE && campaign.enemyhealth == 37);
    press(C_Responder, KEY_ENTER); assert(level.fighters[1].health == 37);
    for (int i = 0; i < 7; i++) {
        if (i) C_StartFight();
        level.fighters[0].health = 85 - i * 5;
        finishmatch(0);
        assert(campaign.health == 85 - i * 5);
        assert(gamestate == (i == 6 ? GS_ENDING : GS_MAP));
    }
    assert(!campaign.defeated);
    press(C_Responder, KEY_ENTER); assert(gamestate == GS_MENU && !campaign.active);
    assert(C_Start(hero)); C_StartFight(); finishmatch(1);
    for (int i = 0; i < 10 * TICRATE; i++) C_Ticker();
    assert(gamestate == GS_ENDING && campaign.defeated);
    // A fresh select must forget P2's join from the previous session.
    SEL_Start(); press(SEL_Responder, 'o'); press(SEL_Responder, 'o');
    press(SEL_Responder, KEY_ENTER);
    for (int i = 0; i < 50; i++) SEL_Ticker();
    assert(gamestate == GS_FIGHT && !match.cpu[1]);
    SEL_Start(); press(SEL_Responder, KEY_ENTER);
    for (int i = 0; i < 50; i++) SEL_Ticker();
    assert(gamestate == GS_MAP);
    // PC Start menu modes: choose a training dummy using P1 controls.
    M_OpenStart(); press(M_Responder, KEY_DOWNARROW); press(M_Responder, KEY_ENTER);
    assert(gamemode == GM_TRAINING && gamestate == GS_SELECT);
    press(SEL_Responder, KEY_ENTER); press(SEL_Responder, 'd'); press(SEL_Responder, KEY_ENTER);
    for (int i = 0; i < 50; i++) SEL_Ticker();
    assert(gamestate == GS_FIGHT && level.time == -1);
    level.phase = PH_FIGHT; level.fighters[0].health = level.fighters[1].health = 1;
    P_Ticker(); assert(level.fighters[0].health == MAX_HEALTH && level.fighters[1].health == MAX_HEALTH);
    for (int i = 0; i < 4000; i++) G_Ticker();
    assert(gamestate == GS_FIGHT && level.phase == PH_FIGHT && gameaction == ga_nothing);
    // Tug of War keeps a single shared pool over an actual CPU match.
    C_Abandon(); gamemode = GM_TUG;
    bool bothcpu[2] = {true, true}; G_InitNew(roster[0], roster[2], bothcpu, &stages[0]);
    assert(level.fighters[0].health == 50 && level.fighters[1].health == 50);
    for (int i = 0; i < 3000 && gameaction == ga_nothing; i++) {
        G_Ticker(); assert(level.fighters[0].health + level.fighters[1].health == MAX_HEALTH);
    }
    gameaction = ga_nothing;
    // Endurance selects four fighters per side and keeps the winner's health.
    gamemode = GM_ENDURANCE; SEL_Start();
    for (int i = 0; i < 4; i++) { press(SEL_Responder, KEY_ENTER); press(SEL_Responder, 'd'); }
    for (int i = 0; i < 50; i++) SEL_Ticker();
    assert(endurance && gamestate == GS_FIGHT && !campaign.active);
    for (int i = 0; i < 4; i++) {
        level.fighters[0].health = 80 - i * 10;
        winround(0); assert(gameaction == ga_matchover); gameaction = ga_nothing;
        E_MatchOver(0);
        if (i < 3) assert(endurance && level.fighters[0].health == 80 - i * 10);
    }
    assert(!endurance && gamestate == GS_RESULTS);
    E_Drawer(); press(M_Responder, KEY_ENTER); assert(gamestate == GS_MENU);
 puts("flow: menus, 7 lands, continues, 7 final rivals, victory, defeat, P2 reset, training, tug of war and endurance passed");
}


#ifndef CLAY_COMBINED
int main(int argc,char **argv) {
    myargc=argc;myargv=argv;I_InitGraphics(true);W_Init("data/primrage");
    PS_Init();V_Init();P_LoadRoster("data/creatures");
    CF_FlowChecks();I_Shutdown();return 0;
}
#endif
