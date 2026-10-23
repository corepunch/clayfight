// g_game.c -- game flow, input bindings and ticcmds (cf. g_game.c)

#include <SDL_scancode.h>

#include "game/game.h"

gamestate_t gamestate = GS_TITLE;
gameaction_t gameaction;
bool paused;
match_t match;

static const struct {
    int key, player, bit;
} bindings[] = {
    // left player: WASD to move, R T (high) / F G (low) to attack
    {SDL_SCANCODE_W, 0, BT_UP}, {SDL_SCANCODE_S, 0, BT_DOWN},
    {SDL_SCANCODE_A, 0, BT_LEFT}, {SDL_SCANCODE_D, 0, BT_RIGHT},
    {SDL_SCANCODE_R, 0, BT_HQ}, {SDL_SCANCODE_T, 0, BT_HF},
    {SDL_SCANCODE_F, 0, BT_LQ}, {SDL_SCANCODE_G, 0, BT_LF},
    {SDL_SCANCODE_RETURN, 0, BT_START}, {SDL_SCANCODE_SPACE, 0, BT_START},
    // right player: arrows to move, O P (high) / L ; (low) to attack
    {SDL_SCANCODE_UP, 1, BT_UP}, {SDL_SCANCODE_DOWN, 1, BT_DOWN},
    {SDL_SCANCODE_LEFT, 1, BT_LEFT}, {SDL_SCANCODE_RIGHT, 1, BT_RIGHT},
    {SDL_SCANCODE_O, 1, BT_HQ}, {SDL_SCANCODE_P, 1, BT_HF},
    {SDL_SCANCODE_L, 1, BT_LQ}, {SDL_SCANCODE_SEMICOLON, 1, BT_LF},
    {SDL_SCANCODE_BACKSLASH, 1, BT_START},
};

static const int padbits[] = {
    [PK_UP] = BT_UP, [PK_DOWN] = BT_DOWN, [PK_LEFT] = BT_LEFT, [PK_RIGHT] = BT_RIGHT,
    [PK_B0] = BT_HQ, [PK_B1] = BT_HF, [PK_B2] = BT_LQ, [PK_B3] = BT_LF, [PK_START] = BT_START,
};

static int held[MAXPLAYERS]; // buttons currently down (gamekeydown)

bool G_Binding(int key, int *player, int *bit)
{
    if (key >= KEY_PAD) {
        int pad = (key - KEY_PAD) / 16, k = (key - KEY_PAD) % 16;
        if (pad >= MAXPLAYERS || k >= (int)(sizeof padbits / sizeof padbits[0]) || !padbits[k])
            return false;
        *player = pad;
        *bit = padbits[k];
        return true;
    }
    for (size_t i = 0; i < sizeof bindings / sizeof bindings[0]; i++) {
        if (bindings[i].key == key) {
            *player = bindings[i].player;
            *bit = bindings[i].bit;
            return true;
        }
    }
    return false;
}

bool G_Responder(const event_t *ev)
{
    if (ev->type == ev_keydown && gamestate == GS_FIGHT) {
        if (ev->key == SDL_SCANCODE_GRAVE) { // debug: show hit boxes
            r_showboxes = !r_showboxes;
            return true;
        }
        if (ev->key == SDL_SCANCODE_ESCAPE || ev->key == PADKEY(0, PK_BACK)) {
            paused = !paused;
            return true;
        }
    }
    int player, bit;
    if (!G_Binding(ev->key, &player, &bit))
        return false;
    if (ev->type == ev_keydown)
        held[player] |= bit;
    else
        held[player] &= ~bit;
    return false; // let menus see it too
}

// G_BuildTiccmd: snapshot the controls for this tic.
static void G_BuildTiccmd(int player, ticcmd_t *cmd)
{
    cmd->buttons = held[player];
}

void G_InitNew(fighterdef_t *p1, fighterdef_t *p2, const bool cpu[2], stage_t *stage)
{
    R_PrepareStage(stage);
    match = (match_t){
        .defs = {p1, p2},
        .cpu = {cpu[0], cpu[1]},
        .stage = stage,
        .palettes = {p1->palette, p1 == p2 ? p2->altpalette : p2->palette},
        .round = 1,
    };
    M_ClearRandom();
    P_SetupRound(match.defs, match.palettes, stage, 1);
    paused = false;
    gamestate = GS_FIGHT;
}

void G_Ticker(void)
{
    if (gamestate != GS_FIGHT || paused)
        return;
    fighter_t *a = &level.fighters[0], *b = &level.fighters[1];
    if (match.cpu[0])
        P_CpuCmd(a, b, &a->cmd);
    else
        G_BuildTiccmd(0, &a->cmd);
    if (match.cpu[1])
        P_CpuCmd(b, a, &b->cmd);
    else
        G_BuildTiccmd(1, &b->cmd);
    P_Ticker();

    if (level.phase != PH_OVER)
        return;
    if (level.winner < 2)
        match.wins[level.winner]++;
    I_Log("round %d: winner %d, health %d/%d, %d s left", match.round, level.winner,
          level.fighters[0].health, level.fighters[1].health, level.time / TICRATE);
    if (match.wins[0] >= ROUNDS_TO_WIN || match.wins[1] >= ROUNDS_TO_WIN || match.round >= 5) {
        gameaction = ga_matchover;
        return;
    }
    P_SetupRound(match.defs, match.palettes, match.stage, ++match.round);
}
