// Endurance: four selected fighters per team; the survivor keeps its health.
#include "game/game.h"
bool endurance;
static fighterdef_t *team[2][4];
static int member[2], health[2], result;
static bool teamcpu[2];
static void E_Fight(void)
{
    G_InitNew(team[0][member[0]], team[1][member[1]], teamcpu, &stages[0]);
    for (int p = 0; p < 2; p++) {
        level.fighters[p].health = health[p];
        level.fighters[p].shownhealth = health[p];
    }
}
void E_Start(fighterdef_t *teams[2][4], const bool cpu[2])
{
    C_Abandon(); endurance = true; result = -1;
    for (int p = 0; p < 2; p++) {
        member[p] = 0; health[p] = MAX_HEALTH; teamcpu[p] = cpu[p];
        for (int i = 0; i < 4; i++) team[p][i] = teams[p][i];
    }
    E_Fight();
}
void E_MatchOver(int winner)
{
    for (int p = 0; p < 2; p++) {
        if (winner == p) health[p] = MAX(1, level.fighters[p].health);
        else { member[p]++; health[p] = MAX_HEALTH; }
    }
    if (member[0] < 4 && member[1] < 4) { E_Fight(); return; }
    result = member[0] == 4 && member[1] == 4 ? 2 : member[0] < 4 ? 0 : 1;
    endurance = false; G_ClearInput(); gamestate = GS_RESULTS;
}
void E_Drawer(void)
{
    M_Background("ENDURANCE");
    V_DrawTextCentered(FONT_BIG, 160, 67, result == 2 ? "NO WINNERS" : result == 0 ? "LEFT PLAYER WINS" : "RIGHT PLAYER WINS");
    if (result < 2) {
        M_DrawCreature(team[result][member[result]], 118, 86, 84, 73, result == 1);
        V_DrawTextCentered(FONT_SMALL, 160, 166, team[result][member[result]]->name);
    }
    V_DrawTextCentered(FONT_SMALL, 160, 178, "PRESS START TO RETURN");
}
