// p_local.h -- play simulation: fighters, the level, and the p_*.c modules (cf. p_local.h)

#ifndef __P_LOCAL__
#define __P_LOCAL__

#include "pr_def.h"
#include "info.h"
#include "p_script.h"

// fighter_t (cf. mobj_t + player_t)
typedef struct fighter_s {
    int player;
    fighterdef_t *def;
    const palette_t *pal;
    float x, z; // z: height above the floor
    float momx, momz;
    int facing; // +1 right, -1 left
    const state_t *state;
    float tics; // left on the current frame
    int health;
    float shownhealth; // HUD drains towards health
    int stun; // tics of hit/block stun left
    int flash;
    bool attackhit; // current attack already connected
    bool airattackused;
    bool blocking;
    bool dead;
    int grunts;
    ticcmd_t cmd;
    int oldbuttons;
    // CPU brain (p_enemy.c)
    int aiwait, aihold, aiplan;
    // the move in progress
    int move; // slot in def->moves, -1 none
    const attackinfo_t *attack; // NULL when not attacking
    bool attacklive; // an attack box is out this tic
    int curbox; // which of the move's boxes, -1 none
    float movetic; // tics since the move started
    int linkstun; // stun of the hit that started this reaction
    animnum_t animnext; // what follows the current animation
    int wanderdir, wandertimer; // breathing idle
    int downtimer;
    // Primal Rage script mode (p_script.c, -script): the running animation
    // script, the state it shows, and the move table entry being performed
    psanim_t anim;
    state_t pstate;
    psmove_t smove;
    attackinfo_t smoveattack;
    int boxtimer[5]; // tics left on each of the move's attack boxes
    bool scripthold; // keep showing the last frame when the script ends
} fighter_t;

typedef enum { PH_INTRO, PH_FIGHT, PH_KO, PH_OVER } phase_t;

typedef struct {
    float x, z, vx, vz;
    int life;
    pixel_t color;
} particle_t;

#define MAXPARTICLES 256

struct stage_s;

typedef struct {
    fighter_t fighters[2];
    particle_t particles[MAXPARTICLES];
    int numparticles;
    struct stage_s *stage;
    float camx;
    int leveltime;
    int time; // tics left on the round clock
    phase_t phase;
    int phasetime;
    int hitstop; // freeze frames after an impact
    int winner; // 0/1, 2 = draw, -1 = undecided
    int round;
} level_t;

extern level_t level;

// p_tick.c
void P_SetupRound(fighterdef_t *defs[2], const int palettes[2], struct stage_s *stage, int round);
void P_Ticker(void);

// p_mobj.c
extern bool p_scripted; // -script: run PRAGE.EXE's bytecode instead of creature files
void P_SpawnFighter(fighter_t *f, fighterdef_t *def, int player, float x, int palette);
bool P_SetAnim(fighter_t *f, animnum_t anim);
bool P_StartMove(fighter_t *f, int slot, animnum_t next);
bool P_InAnim(const fighter_t *f, animnum_t anim);
bool P_IsAirborne(const fighter_t *f);
void P_FighterThinker(fighter_t *f);
void P_AnimTick(fighter_t *f); // advance the animation only

// p_check.c
int P_CheckAnims(void); // -checkanims: creature files vs PRAGE.EXE's scripts
void P_Voice(fighter_t *f, int which, float vol);
void A_Swish(fighter_t *f, const state_t *st);
void A_Roar(fighter_t *f, const state_t *st);
void A_Thud(fighter_t *f, const state_t *st);

// p_user.c
void P_PlayerThink(fighter_t *f, const fighter_t *opp);
int P_ForwardInput(const fighter_t *f, int buttons);

// p_map.c
typedef struct {
    float x0, x1, z0, z1;
} box_t;
bool P_HurtBox(const fighter_t *f, box_t *out);
bool P_AttackBox(const fighter_t *f, box_t *out);
bool P_BoxesTouch(const box_t *a, const box_t *b);
void P_SeparateFighters(fighter_t *a, fighter_t *b);

// p_inter.c
bool P_CheckAttack(fighter_t *att, fighter_t *vic);

// p_enemy.c
void P_CpuCmd(fighter_t *f, const fighter_t *opp, ticcmd_t *cmd);

// p_effect.c
typedef enum { SPARK_HIT, SPARK_BIG, SPARK_BLOCK } sparkkind_t;
void P_SpawnSpark(float x, float z, sparkkind_t kind);
void P_RunParticles(void);

#endif
