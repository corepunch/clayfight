// info.h -- fighter definitions and state tables (cf. info.h)
//
// Doom drives every thing with a flat states[] array: each state names a
// sprite frame, how long to show it, an action to run on entry and the next
// state. Fighters work the same way, except that the tables are loaded from
// creature files at startup (p_creature.c) instead of being compiled in.
// See docs/creature-format.md.

#ifndef __INFO__
#define __INFO__

#include "pr_def.h"
#include "w_gra.h"

// What the play code lets a fighter do while in an animation.
typedef enum {
    K_FREE, // stand / walk / crouch: reads input every tic
    K_ATTACK,
    K_HIT, // hitstun or blockstun
    K_AIR,
    K_DOWN, // knocked down: can't be hit
    K_SCRIPT, // victory and other non-interactive sequences
} animkind_t;

// The animations the play code asks for by name. Creature files may define
// more (each move's animation); those get indices from NUMSTDANIMS up.
typedef enum {
    ANIM_NONE = -1,
    ANIM_STAND,
    ANIM_WALKF,
    ANIM_WALKB,
    ANIM_CROUCH,
    ANIM_JUMP,
    ANIM_LAND,
    ANIM_BLOCK,
    ANIM_CROUCHBLOCK,
    ANIM_HIT,
    ANIM_CROUCHHIT,
    ANIM_HITHARD,
    ANIM_FALL,
    ANIM_DOWN,
    ANIM_GETUP,
    ANIM_DIZZY,
    ANIM_WIN,
    NUMSTDANIMS,
    // used only by the script mode (p_script.c) to label running moves
    ANIM_MOVE = NUMSTDANIMS,
    ANIM_AIRMOVE,
} animnum_t;

#define MAXANIMS 64

// Moves: 6 groups x 4 buttons (the same numbering as Primal Rage's move
// table): stand, close, crouch, jump, power, air -- each HQ HF LQ LF.
enum { MG_STAND, MG_CLOSE, MG_CROUCH, MG_JUMP, MG_POWER, MG_AIR, NUMMOVEGROUPS };
#define NUMMOVES (NUMMOVEGROUPS * 4)
#define MAXBOXES 32

typedef enum { HIGH, LOW } attacklevel_t;

typedef struct {
    int damage;
    attacklevel_t level;
    float push;
    bool knockdown;
} attackinfo_t;

typedef struct state_s {
    gra_t *sprite;
    int frame;
    float tics;
    float dx; // root motion on entry, pixels forward
    float stunshares; // > 0: lasts this many shares of the attacker's stun (see animinfo_t.stun)
    bool hflip; // drawn mirrored
    int next; // index into the fighter's states; -1 = the animation ends here
    int anim;
    int step; // frame number within the animation
    animkind_t kind;
    bool active; // derived from the silhouette: attack frames for box-less moves
} state_t;

typedef struct {
    char name[24];
    int first, count; // states
    animkind_t kind;
    int next; // animation that follows; ANIM_NONE = loop
    bool hold; // stay on the last frame
    bool loop; // explicitly loops (overrides the standard successor)
    int loopfrom; // when looping, restart at this frame
    int wanderfrom; // >= 0: frames from here on drift randomly (breathing idle)
    int stun; // > 0: an "Ns" frame lasts N * (attacker's stun / stun) tics
} animinfo_t;

typedef struct {
    float tic; // tics after the move starts
    int tics; // how long it stays live
    int x0, y0, x1, y1; // pixels around the hotspot, art unmirrored, y down
} hitbox_t;

typedef struct {
    bool valid;
    int anim;
    attackinfo_t attack;
    int stun;
    int numboxes; // 0: derive the attack area from the sprites
    hitbox_t boxes[MAXBOXES];
} movedef_t;

// fighterdef_t: one per creature file (cf. mobjinfo_t)
typedef struct {
    char name[32];
    char prefix[8]; // art files S04<prefix>*.GRA, voice S04<prefix>SD.GRA
    int codeindex; // character number in PRAGE.EXE (-script mode), -1 none
    int portrait, label; // S04SELMO.GRA frames: 5-frame portrait, name plate
    int palette, altpalette; // sub-palettes of the base art file
    bool facesleft; // which way the art faces
    float walkspeed, backspeed, jumpspeed;
    int width; // body width for pushing
    gra_t *base;
    state_t *states;
    int numstates;
    animinfo_t anims[MAXANIMS];
    int numanims;
    movedef_t moves[NUMMOVES];
    int reach; // forward reach of the standing pose
} fighterdef_t;

#define MAXROSTER 16
extern fighterdef_t *roster[MAXROSTER];
extern int numroster;

// p_creature.c
void P_LoadRoster(const char *dir);
fighterdef_t *P_LoadCreature(const char *path);

// info.c
bool P_Playable(const fighterdef_t *def);
int P_FindAnim(const fighterdef_t *def, const char *name);
void P_FinishFighter(fighterdef_t *def); // link states, measure, derive attack frames
patch_t *P_StatePatch(const state_t *st);
void P_PatchProfile(patch_t *p, bool facesleft);

extern const char *const stdanimnames[NUMSTDANIMS];

#endif
