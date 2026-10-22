// g_game.h -- game flow, bindings and ticcmds (cf. g_game.h)

#ifndef __G_GAME__
#define __G_GAME__

#include "pr_def.h"
#include "d_event.h"
#include "info.h"
#include "r_local.h"

typedef enum {
    ga_nothing,
    ga_matchover, // back to character select
} gameaction_t;

typedef struct {
    fighterdef_t *defs[2];
    bool cpu[2];
    stage_t *stage;
    int palettes[2];
    int wins[2];
    int round;
} match_t;

extern gamestate_t gamestate;
extern gameaction_t gameaction;
extern bool paused;
extern match_t match;

bool G_Responder(const event_t *ev);
bool G_Binding(int key, int *player, int *bit); // key -> player + BT_ bit
void G_InitNew(fighterdef_t *p1, fighterdef_t *p2, const bool cpu[2], stage_t *stage);
void G_Ticker(void);

#endif
