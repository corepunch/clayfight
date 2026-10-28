// c_main.c -- original-data Urth globe, conquest and final battle.
#include <stdio.h>
#include "game/game.h"

campaign_t campaign;
// PRAGE.EXE character indices: Sauron, Blizzard, Talon, Vertigo, Armadon,
// Diablo, Chaos. Home arenas and globe pins are explicitly associated with
// opponents so a victory conquers the land the map actually announced.
static const int home[7] = {0, 4, 2, 3, 6, 1, 5};
static const int pins[7][2] = {{65, 69}, {114, 38}, {134, 64}, {144, 110}, {88, 113}, {48, 52}, {97, 83}};

void C_Abandon(void) { campaign.active = false; }

bool C_Start(fighterdef_t *hero)
{
    campaign = (campaign_t){.active = true, .hero = hero, .credits = options.credits, .health = MAX_HEALTH, .enemyhealth = MAX_HEALTH};
    for (int i = 0; i < numroster; i++) {
        int id = roster[i]->codeindex;
        if (id >= 0 && id < 7 && P_Playable(roster[i])) campaign.opponents[id] = roster[i];
    }
    for (int i = 0; i < 7; i++) {
        if (!campaign.opponents[i]) { campaign.active = false; return false; }
        // For an original fighter the mirror confrontation is last.
        campaign.order[i] = (i + (hero->codeindex >= 0 ? hero->codeindex + 1 : 0)) % 7;
    }
    G_ClearInput();
    gamestate = GS_MAP;
    return true;
}

void C_SetupFinal(void)
{
    if (!campaign.active || !campaign.finalbattle) return;
    level.time = -1; // continuous final battle: no clock and no round resets
    level.fighters[0].health = campaign.health;
    level.fighters[0].shownhealth = campaign.health;
    level.fighters[1].health = campaign.enemyhealth;
    level.fighters[1].shownhealth = campaign.enemyhealth;
}

void C_StartFight(void)
{
    int id = campaign.finalbattle ? campaign.order[campaign.finalindex] : campaign.order[campaign.conquered];
    bool cpu[2] = {false, true};
    G_InitNew(campaign.hero, campaign.opponents[id], cpu, &stages[campaign.finalbattle ? 7 : home[id]]);
    C_SetupFinal();
}

void C_MatchOver(bool won)
{
    campaign.tics = 0;
    G_ClearInput();
    if (!won) {
        campaign.enemyhealth = campaign.finalbattle ? MAX(1, level.fighters[1].health) : MAX_HEALTH;
        gamestate = GS_CONTINUE;
        return;
    }
    if (campaign.finalbattle) {
        campaign.health = MAX(1, level.fighters[0].health);
        campaign.enemyhealth = MAX_HEALTH;
        if (++campaign.finalindex == 7) { gamestate = GS_ENDING; return; }
    } else {
        campaign.conquered++;
        if (campaign.conquered == 7) {
            campaign.finalbattle = true;
            campaign.health = MAX_HEALTH;
        }
    }
    gamestate = GS_MAP;
}

bool C_Responder(const event_t *ev)
{
    if (ev->type != ev_keydown) return false;
    if (ev->key == KEY_ESCAPE || ev->key == PADKEY(0, PK_BACK)) {
        C_Abandon(); M_StartMenu(); return true;
    }
    int pl, bit;
    if (!G_Binding(ev->key, &pl, &bit) || !(bit & (BT_START | BT_ATTACKS))) return false;
    if (gamestate == GS_MAP) C_StartFight();
    else if (gamestate == GS_CONTINUE && campaign.credits > 0) {
        campaign.credits--;
        campaign.health = MAX_HEALTH;
        C_StartFight();
    } else { C_Abandon(); M_StartMenu(); }
    return true;
}

void C_Ticker(void)
{
    campaign.tics++;
    if (gamestate == GS_MAP && campaign.tics >= TICRATE * 6) C_StartFight();
    if (gamestate == GS_CONTINUE && campaign.tics >= TICRATE * (campaign.finalbattle ? 15 : 10)) {
        campaign.defeated = true;
        campaign.tics = 0;
        gamestate = GS_ENDING;
    }
}

void C_Drawer(void)
{
    if (gamestate == GS_ENDING) {
        M_Background(campaign.defeated ? "GAME OVER" : "MASTER OF THE NEW URTH");
        M_DrawCreature(campaign.hero, 110, 44, 100, 100, false);
        V_DrawTextCentered(FONT_BIG, 160, 150, campaign.hero->name);
        V_DrawTextCentered(FONT_SMALL, 160, 176, campaign.defeated ? "YOUR CONQUEST ENDS HERE" : "SEVEN LANDS. SEVEN FINAL RIVALS. VICTORY!");
        return;
    }
    if (gamestate == GS_CONTINUE) {
        M_Background("CONTINUE?");
        char text[64];
        int left = MAX(0, (campaign.finalbattle ? 15 : 10) - campaign.tics / TICRATE);
        snprintf(text, sizeof text, "%d", left);
        V_DrawTextCentered(FONT_BIG, 160, 75, text);
        snprintf(text, sizeof text, "GAME CREDITS: %d", campaign.credits);
        V_DrawTextCentered(FONT_SMALL, 160, 111, text);
        V_DrawTextCentered(FONT_SMALL, 160, 135, campaign.credits ? "PRESS START TO TRY THIS RIVAL AGAIN" : "NO CREDITS REMAIN");
        V_DrawTextCentered(FONT_SMALL, 160, 157, "YOUR CONQUERED LANDS ARE KEPT");
        return;
    }
    V_Clear(0xff07080c);
    V_DrawTextCentered(FONT_BIG, 160, 13, campaign.finalbattle ? "WORLD DOMINATION" : "CONQUERED LANDS");
    // Original animated globe and original continent patches, with their
    // shared hotspot retained. Marker positions are rebuilt for this view.
    int frame = 8 + campaign.tics / 5 % 12;
    gra_t *g = W_LoadGra("S04GLOBM.GRA");
    int layers[] = {frame, 23 + campaign.tics / 12 % 4, 37, 44};
    for (int i = 0; i < 4; i++) {
        patch_t *p = W_Frame(g, layers[i]);
        V_DrawPatch(104, 108, p, &g->palettes[W_GuessPalette(p, g)], 0);
    }
    int current = campaign.order[campaign.finalbattle ? campaign.finalindex : campaign.conquered];
    for (int i = 0; i < 7; i++) {
        int id = campaign.order[i];
        bool done = i < campaign.conquered;
        int x = pins[id][0] + 15, y = pins[id][1] + 31;
        pixel_t color = done ? 0xff75b64b : 0xff926c38;
        if (id == current && (campaign.tics & 16)) color = 0xffffe0a0;
        V_FillRect(x - 2, y - 2, 5, 5, 0xff000000);
        V_FillRect(x - 1, y - 1, 3, 3, color);
        V_DrawText(FONT_SMALL, 204, 43 + i * 18, campaign.opponents[id]->name);
        if (done) V_DrawText(FONT_SMALL, 191, 43 + i * 18, "+");
        if (id == current) V_DrawText(FONT_SMALL, 295, 43 + i * 18, "<");
    }
    char text[80];
    if (campaign.finalbattle) snprintf(text, sizeof text, "FINAL BATTLE  %d / 7", campaign.finalindex + 1);
    else snprintf(text, sizeof text, "LANDS CONQUERED: %d / 7", campaign.conquered);
    V_DrawTextCentered(FONT_SMALL, 160, 167, text);
    snprintf(text, sizeof text, "%s  /  %s", campaign.opponents[current]->name,
             campaign.finalbattle ? "DINOSAUR GRAVEYARD" : stages[home[current]].name);
    V_DrawTextCentered(FONT_SMALL, 160, 179, text);
#ifdef CLAY_IOS
    V_DrawTextCentered(FONT_SMALL, 160, VIEWHEIGHT - 10, "SELECT: FIGHT    BACK: MAIN MENU");
#else
    V_DrawTextCentered(FONT_SMALL, 160, VIEWHEIGHT - 10, "PRESS START TO FIGHT    ESC: MAIN MENU");
#endif
}
