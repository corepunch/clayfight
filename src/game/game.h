// game.h -- the fighting game: fighter definitions and state tables, the
// play simulation, Primal Rage's script interpreter, the fight renderer,
// HUD, menus and game flow (cf. Doom's info.h, p_local.h, r_local.h,
// g_game.h, st_stuff.h, wi_stuff.h, m_menu.h, s_sound.h).

#ifndef __GAME__
#define __GAME__

#include "game/resources.h"

// --- game rules (cf. doomdef.h) --------------------------------------------

#define MAXPLAYERS 2

typedef enum {
    GS_TITLE,
    GS_SELECT,
    GS_FIGHT,
    GS_MENU,
    GS_CREATE,
    GS_MAP,
    GS_CONTINUE,
    GS_ENDING,
    GS_RESULTS,
} gamestate_t;

// ticcmd_t.buttons bits. Primal Rage's four buttons are HI/LO x QUICK/FIERCE.
enum {
    BT_UP = 1 << 0,
    BT_DOWN = 1 << 1,
    BT_LEFT = 1 << 2,
    BT_RIGHT = 1 << 3,
    BT_HQ = 1 << 4, // high quick
    BT_HF = 1 << 5, // high fierce
    BT_LQ = 1 << 6, // low quick
    BT_LF = 1 << 7, // low fierce
    BT_START = 1 << 8,
    BT_ATTACKS = BT_HQ | BT_HF | BT_LQ | BT_LF,
};

typedef struct {
    int buttons;
} ticcmd_t;

// Arena geometry, in world pixels. z grows upward from the floor (0).
int R_ArenaWidth(void);
#define ARENA_WIDTH R_ArenaWidth()
#define FLOOR_Y (VIEWHEIGHT - 18) // screen row of the floor line
#define GRAVITY 0.593f

#define ROUND_TIME 60 // seconds
#define ROUNDS_TO_WIN 2
#define MAX_HEALTH 100
#define MAX_BRAIN 100
#define BRAIN_RECOVERY_DELAY TICRATE

// --- info.c: fighter definitions and state tables (cf. info.h) -------------
//
// Doom drives every thing with a flat states[] array: each state names a
// sprite frame, how long to show it, an action to run on entry and the next
// state. Fighters work the same way, except that the tables are loaded from
// creature files at startup (p_creature.c) instead of being compiled in.
// See docs/creature-format.md.

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

// First 16 slots: stand, close, crouch, jump, each HQ HF LQ LF.
// Slots 16..23: high/low power pairs for stand, close, crouch, jump.
// MG_POWER/MG_AIR retain the old file aliases for these last eight slots.
enum { MG_STAND, MG_CLOSE, MG_CROUCH, MG_JUMP, MG_POWER, MG_AIR, NUMMOVEGROUPS };
#define NUMMOVES (NUMMOVEGROUPS * 4)
#define MP_STAND 16
#define MP_CLOSE 18
#define MP_CROUCH 20
#define MP_JUMP 22
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
    char prefix[8]; // original character identifier or custom photo prefix
    int codeindex; // arcade character index; -1 for custom fighters
    int portrait, label; // virtual selection bank slots: portrait and label
    gra_t *photoport; // photographed close-up, separate from the animation scale
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

#define MAXROSTER 64
extern fighterdef_t *roster[MAXROSTER];
extern int numroster;

// p_creature.c
void P_LoadRoster(const char *dir);
void P_RefreshRoster(const char *dir); // append newly published creatures
fighterdef_t *P_LoadCreature(const char *path);
bool P_RegisterCreature(fighterdef_t *def, const char *path); // already validated, atomically published

// info.c
bool P_Playable(const fighterdef_t *def);
int P_FindAnim(const fighterdef_t *def, const char *name);
void P_FinishFighter(fighterdef_t *def); // link states, measure, derive attack frames
patch_t *P_StatePatch(const state_t *st);
void P_PatchProfile(patch_t *p, bool facesleft);

extern const char *const stdanimnames[NUMSTDANIMS];

// --- p_script.c: Primal Rage's own animation scripts and move tables -------
//
// Run directly from the big-endian arcade CPU ROM (see w_exe.c).
// An animation is a stream of 16-bit words. A word below 0x8000 shows a
// "frame id" (bit 15 of a frame word mirrors it); anything else is an
// opcode: number = (w >> 8) & 0x1F, operand mode = w & 0x6000, selector =
// low byte (opcode 0x1F: the real opcode is the low byte and the selector is
// the next word). Frame ids address RLE motion objects. Each script runs
// with a frame timer: every tic the
// timer drops by one; at zero the script advances to its next frame and the
// timer gains that frame's duration.
//
// The game code calls native routines from scripts (CALL/CALL_YIELD). Those
// are bound to C functions here by address -- the role of Doom's action
// pointers. Unknown routines are skipped and reported once.

#define PS_NUMROWS 62 // core animations per character
#define PS_NUMVARS 6
#define PS_EPSILON 1e-3f // frame timers are floats; fractional durations must not drift a tic

// Native arcade units; presentation applies the sole 2x enlargement.
#define ARCADE_XSCALE 1.f
#define ARCADE_YSCALE 1.f

// the 62 core rows, named from what they show (the game names none of them)
enum {
    ROW_STANCE = 0,
    ROW_CROUCH = 1,
    ROW_CROUCHED = 2,
    ROW_STANDUP = 3,
    ROW_JUMP = 4,
    ROW_WALKF = 7,
    ROW_WALKB = 10,
    ROW_JUMPMOVE = 12,
    ROW_LAND = 13,
    ROW_BLOCK = 38,
    ROW_CROUCHBLOCK = 40,
    ROW_HIT = 42,
    ROW_CROUCHHIT = 43,
    ROW_HITHARD = 44,
    ROW_FALL = 46,
    ROW_LYING = 52,
    ROW_GETUP = 53,
    ROW_DIZZY = 54,
    ROW_WIN = 58,
};

// A script in progress (the animation part of the game's object struct).
typedef struct {
    uint32_t ip;
    uint32_t cosmetic_random; // scenery has its own stream; never changes combat RNG
    float timer, duration;
    uint32_t timing; // per-frame tick table (indexed by v52), 0 = off
    uint32_t table; // MOVE_X/MOVE_Y data table
    int vars[PS_NUMVARS]; // v52 v53 v54 v55 slot v58
    int loopcount;
    int step; // frames advanced since the start (attack boxes key on it)
    int cyclelen; // range of the breathing index (v52)
    int breathtimer;
    bool done;
    // what to show
    int frameid;
    bool hflip;
    gra_t *sprite;
    int frame;
    int ax, ay; // per-frame anchor
    float movex, movey; // root motion accumulated this tic
    bool flipfacing; // FLIP_FACING executed this tic
    int linkstun; // stun of the attack that started a hit reaction (kept across PS_Start)
} psanim_t;

typedef struct {
    int step, ticks; // starts when `step` frames have played, lasts `ticks`
    int x, y, w, h; // in 4-pixel arcade units, combat-anchor-relative
} psbox_t;

typedef struct {
    bool valid;
    int row;
    int damage;
    int strength; // 1 light .. 3 heavy
    int stun; // A[1]: hit reactions stretch to this many tics
    int hitclass; // C[3] low three bits; C[2] contains separate flags
    int numboxes;
    psbox_t boxes[5];
} psmove_t;

bool PS_Init(void); // after X_Init; false if the tables don't check out
bool PS_Active(void);
uint32_t PS_RowScript(int character, int row);
float PS_RowDuration(int row);
bool PS_Move(int character, int id, psmove_t *out);
bool PS_FrameInfo(int frameid, gra_t **sprite, int *frame, int *ax, int *ay);

void PS_Start(psanim_t *a, uint32_t script, float duration);
void PS_StartScenery(psanim_t *a, uint32_t script, float duration, uint32_t seed);
// Advance one tic. Returns true when a new frame came up (a->step grew).
bool PS_Tick(psanim_t *a);

// --- r_main.c, r_stage.c: arenas and the fight scene (cf. r_local.h) --------
typedef struct stage_s {
    const char *name;
    bool prepared; // immutable ROM scenery expanded on first use
} stage_t;

extern stage_t stages[];
extern int numstages;
extern bool r_showboxes;

void R_PrepareStage(stage_t *stage);
void R_DrawStage(const stage_t *stage, int camx);
void R_DrawStageForeground(const stage_t *stage, int camx);
void R_RenderFight(void);

// --- p_*.c: play simulation: fighters and the level (cf. p_local.h) ---------
// fighter_t (cf. mobj_t + player_t)
typedef struct fighter_s {
    int player;
    fighterdef_t *def;
    const palette_t *pal;
    float x, z; // z: height above the floor
    float momx, momz;
    float airlift; // ROM MOVE_Y since takeoff: the air pivot's height above z
    int facing; // +1 right, -1 left
    const state_t *state;
    float tics; // left on the current frame
    int health;
    float shownhealth; // HUD drains towards health
    int stun; // tics of hit/block stun left
    int brain, braindelay; // accumulated brain damage, independent of hitstun
    bool dizzypending, dizzy;
    int mashdir; // last non-neutral left/right direction during dizziness
    int flash;
    bool attackhit; // current attack already connected
    bool airattackused;
    bool blocking;
    bool dead;
    int grunts;
    ticcmd_t cmd;
    int oldbuttons;
    // CPU brain (p_enemy.c)
    int aiwait, aihold, aiplan; // reaction cooldown, committed macro duration/input
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
    float x, z;
    bool blocked;
    psanim_t animation;
} particle_t;

#define MAXPARTICLES 256

typedef struct {
    fighter_t fighters[2];
    particle_t particles[MAXPARTICLES];
    int numparticles;
    stage_t *stage;
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
void P_SetupRound(fighterdef_t *defs[2], const int palettes[2], stage_t *stage, int round);
void P_Ticker(void);

// p_mobj.c
extern bool p_scripted; // original fighters run ROM scripts; photos use authored states
void P_SpawnFighter(fighter_t *f, fighterdef_t *def, int player, float x, int palette);
bool P_SetAnim(fighter_t *f, animnum_t anim);
bool P_StartMove(fighter_t *f, int slot, animnum_t next);
bool P_InAnim(const fighter_t *f, animnum_t anim);
bool P_IsAirborne(const fighter_t *f);
int P_SpriteLift(const fighter_t *f); // height of an air pose's pivot above the simulated feet
void P_FighterThinker(fighter_t *f);
void P_AnimTick(fighter_t *f); // advance the animation only

// p_check.c
int P_CheckAnims(void); // -checkanims: all supported arcade animations and move slots
void P_Voice(fighter_t *f, int which, float vol);
void A_Swish(fighter_t *f, const state_t *st);
void A_Roar(fighter_t *f, const state_t *st);
void A_Thud(fighter_t *f, const state_t *st);

// p_user.c
void P_PlayerThink(fighter_t *f, const fighter_t *opp);
int P_ForwardInput(const fighter_t *f, int buttons);
int P_InputMove(const fighter_t *f, const fighter_t *opp, int buttons);
bool P_MoveIsAir(int slot);

// p_map.c
typedef struct {
    float x0, x1, z0, z1;
} box_t;
bool P_HurtBox(const fighter_t *f, box_t *out);
bool P_AttackBox(const fighter_t *f, box_t *out);
bool P_BoxesTouch(const box_t *a, const box_t *b);
void P_SeparateFighters(fighter_t *a, fighter_t *b);

// p_inter.c
// What one attack landing on one victim this tic amounts to. Looked up for
// both fighters first, then applied, so trades are symmetric.
typedef struct {
    const attackinfo_t *atk;
    int stun; // the move's stun, for the victim's reaction length
    bool blocked;
    bool crouch; // the victim takes it crouching
    float x, z; // impact point, for sparks and sound
} hit_t;
bool P_AttackLands(const fighter_t *att, const fighter_t *vic, hit_t *out);
void P_ApplyHit(fighter_t *att, fighter_t *vic, const hit_t *h);

// p_enemy.c
void P_CpuCmd(fighter_t *f, const fighter_t *opp, ticcmd_t *cmd);

// p_effect.c
typedef enum { SPARK_HIT, SPARK_BIG, SPARK_BLOCK } sparkkind_t;
void P_SpawnSpark(float x, float z, sparkkind_t kind);
void P_RunParticles(void);

// --- s_sound.c: sound effects (cf. s_sound.h, sounds.h) --------------------
typedef enum {
    sfx_hit,
    sfx_hithard,
    sfx_block,
    sfx_swish,
    sfx_thud,
    sfx_crowd,
    sfx_select,
    NUMSFX
} sfxenum_t;

void S_Init(void);
// x: world x of the source (for stereo), or < 0 for centred
void S_StartSound(sfxenum_t id, float x, float vol);
// play sample `which` of a fighter's voice file (S04REXSD.GRA ...)
void S_StartVoice(const char *prefix, int which, float x, float vol);

// --- st_stuff.c: status bar and announcements (cf. st_stuff.h, hu_stuff.h) --
void ST_Drawer(const int wins[2]);
void HU_Drawer(void);

// --- g_game.c: game flow, bindings and ticcmds (cf. g_game.h) --------------
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
typedef enum { GM_ARCADE, GM_TRAINING, GM_TUG, GM_ENDURANCE } gamemode_t;
extern gamemode_t gamemode;
void E_Start(fighterdef_t *teams[2][4], const bool cpu[2]);
void E_MatchOver(int winner);
void E_Drawer(void);
extern bool endurance;

bool G_Responder(const event_t *ev);
bool G_Binding(int key, int *player, int *bit); // key -> player + BT_ bit
void G_InitNew(fighterdef_t *p1, fighterdef_t *p2, const bool cpu[2], stage_t *stage);
void G_Ticker(void);
void G_ClearInput(void);

// Single-player conquest followed by the seven-opponent final gauntlet.
typedef struct {
    bool active, finalbattle, defeated;
    fighterdef_t *hero, *opponents[7];
    int order[7], conquered, finalindex, credits, health, enemyhealth, tics;
} campaign_t;
extern campaign_t campaign;
bool C_Start(fighterdef_t *hero);
void C_Abandon(void);
void C_StartFight(void);
void C_MatchOver(bool won);
bool C_Responder(const event_t *ev);
void C_Ticker(void);
void C_Drawer(void);
void C_SetupFinal(void);

typedef struct {
    int rounds, seconds, difficulty, credits;
    bool sound;
    int volume;
} options_t;
extern options_t options;
void M_LoadOptions(void);
void M_SaveOptions(void);
void M_StartMenu(void);
void M_OpenOptions(void);
void M_OpenStart(void);
void M_DrawArt(const char *file, int frame, int x, int y, float scale, int flags);
void M_Background(const char *heading);
void M_DrawCreature(const fighterdef_t *d, int x, int y, int w, int h, bool mirror);
void M_Create(void);
void M_CreateTicker(void);
bool M_CreateResponder(const event_t *ev);
void M_CreateDrawer(void);

// d_main.c: shared initialization; desktop loop or native display link.
int D_InitGame(int argc, char **argv);
void D_RunTic(void);
void D_Display(void);

// --- sel_main.c: character select screen (cf. wi_stuff.h) ------------------
void SEL_Start(void);
void SEL_Reset(void);
bool SEL_Responder(const event_t *ev);
void SEL_Ticker(void);
void SEL_Drawer(void);

// --- m_menu.c: title screen (cf. m_menu.h) ---------------------------------
void M_StartTitle(void);
bool M_Responder(const event_t *ev); // returns true when start is pressed
void M_Ticker(void);
void M_Drawer(void);

#endif
