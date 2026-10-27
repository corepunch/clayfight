// p_mobj.c -- fighters: spawning, state machine, physics (cf. p_mobj.c)
//
// Two animation drivers sit behind P_SetAnim / P_StartMove:
//  - creature files (default): Doom-style state chains from p_creature.c;
//  - -script: Primal Rage's own bytecode run by p_script.c, kept as a
//    reference to check converted creature files against.
// Everything else (control, physics, hits) is shared.

#include <math.h>
#include <string.h>

#include "game/game.h"

bool p_scripted;

#define DOWN_TICS 40

static void P_ScriptThink(fighter_t *f);
static bool P_ScriptAnim(fighter_t *f, animnum_t anim);
static bool P_ScriptMove(fighter_t *f, int slot, animnum_t next);

void P_SpawnFighter(fighter_t *f, fighterdef_t *def, int player, float x, int palette)
{
    memset(f, 0, sizeof *f);
    f->player = player;
    f->def = def;
    f->pal = &def->base->palettes[CLAMP(palette, 0, def->base->numpalettes - 1)];
    f->x = x;
    f->facing = player == 0 ? 1 : -1;
    f->health = MAX_HEALTH;
    f->shownhealth = MAX_HEALTH;
    f->move = -1;
    f->curbox = -1;
    P_SetAnim(f, ANIM_STAND);
}

bool P_InAnim(const fighter_t *f, animnum_t anim)
{
    return f->state && f->state->anim == (int)anim;
}

bool P_IsAirborne(const fighter_t *f)
{
    return f->z > 0 || f->momz > 0;
}

int P_SpriteLift(const fighter_t *f)
{
    // Ground poses have foot hotspots; the original air poses often have
    // torso hotspots instead. Our z is always measured at the feet. Keep
    // those poses above that plane, including during takeoff and landing.
    if (f->state->kind != K_AIR)
        return 0;
    const patch_t *p = P_StatePatch(f->state);
    return p && p->hasbox ? MAX(0, p->by1 + 1) : 0;
}

void P_Voice(fighter_t *f, int which, float vol)
{
    S_StartVoice(f->def->prefix, which, f->x, vol);
}

// Action functions (cf. the A_* functions).
void A_Swish(fighter_t *f, const state_t *st)
{
    (void)st;
    S_StartSound(sfx_swish, f->x, 0.5f);
    if (++f->grunts % 3 == 0) // grunt on every third attack
        P_Voice(f, 1, 0.8f);
}

void A_Roar(fighter_t *f, const state_t *st)
{
    (void)st;
    P_Voice(f, 0, 0.9f);
}

void A_Thud(fighter_t *f, const state_t *st)
{
    (void)st;
    S_StartSound(sfx_thud, f->x, 0.8f);
}

// --- creature-file animation ---------------------------------------------------

static float P_StateTics(const fighter_t *f, const state_t *st)
{
    const animinfo_t *a = &f->def->anims[st->anim];
    if (st->stunshares > 0) // a reaction spread over the hit's stun
        return st->stunshares * (a->stun > 0 && f->linkstun > 0 ? (float)f->linkstun / a->stun : 2);
    return st->tics;
}

// Like Doom's P_SetMobjState. carry keeps the overshoot of the frame timer
// so fractional durations add up.
static void P_EnterState(fighter_t *f, int index, bool carry)
{
    const state_t *st = &f->def->states[index];
    f->state = st;
    f->tics = (carry ? f->tics : 0) + P_StateTics(f, st);
    if (st->dx && st->kind != K_AIR) // root motion; jumps use our physics
        f->x += st->dx * f->facing;
}

bool P_SetAnim(fighter_t *f, animnum_t anim)
{
    if (p_scripted)
        return P_ScriptAnim(f, anim);
    if (anim < 0 || anim >= f->def->numanims || !f->def->anims[anim].count)
        return false;
    const animinfo_t *a = &f->def->anims[anim];
    f->move = -1;
    f->attack = NULL;
    f->attacklive = false;
    f->curbox = -1;
    f->attackhit = false;
    f->animnext = a->next;
    f->wandertimer = 0;
    f->downtimer = anim == ANIM_DOWN ? DOWN_TICS : 0;
    P_EnterState(f, a->first, false);
    return true;
}

// Start the move in `slot` (see game.h); `next` follows when it ends.
bool P_StartMove(fighter_t *f, int slot, animnum_t next)
{
    if (p_scripted)
        return P_ScriptMove(f, slot, next);
    const movedef_t *mv = &f->def->moves[slot];
    if (!mv->valid || !P_SetAnim(f, mv->anim))
        return false;
    f->move = slot;
    f->attack = &mv->attack;
    f->movetic = 0;
    f->animnext = next;
    A_Swish(f, NULL);
    return true;
}

// The breathing idle: drift through the wandering frames in a random
// direction, re-rolled every few frames (Primal Rage's native 0x38294).
static int P_Wander(fighter_t *f, const animinfo_t *a)
{
    int n = a->count - a->wanderfrom;
    if (--f->wandertimer < 0) {
        f->wanderdir = P_Random() & 1 ? 1 : -1; // the frame drives the hurt box: simulation RNG
        f->wandertimer = n / 3 * 3;
    }
    int i = f->state->step - a->wanderfrom + f->wanderdir;
    i = (i % n + n) % n;
    return a->first + a->wanderfrom + i;
}

static void P_AnimateCreature(fighter_t *f)
{
    f->tics -= 1;
    for (int guard = 0; f->tics <= PS_EPSILON && guard < 32; guard++) {
        const state_t *st = f->state;
        const animinfo_t *a = &f->def->anims[st->anim];
        int next = a->wanderfrom >= 0 && st->step >= a->wanderfrom ? P_Wander(f, a) : st->next;
        if (next == st - f->def->states) { // held: stay without re-entering
            f->tics += MAX(P_StateTics(f, st), 1);
            continue;
        }
        if (next >= 0) {
            P_EnterState(f, next, true);
            continue;
        }
        if (st->kind == K_AIR && P_IsAirborne(f)) { // air move over: hang on until landing
            f->tics = 1;
            break;
        }
        animnum_t n = f->animnext;
        if (!P_SetAnim(f, n != ANIM_NONE ? n : ANIM_STAND))
            P_SetAnim(f, ANIM_STAND);
        break;
    }

    // which of the move's attack boxes is out
    f->attacklive = false;
    if (f->move >= 0) {
        const movedef_t *mv = &f->def->moves[f->move];
        f->movetic += 1;
        if (!mv->numboxes) {
            f->attacklive = f->state->active; // derived from the silhouette
        } else {
            for (int i = 0; i < mv->numboxes; i++) {
                if (f->movetic >= mv->boxes[i].tic && f->movetic < mv->boxes[i].tic + mv->boxes[i].tics) {
                    f->attacklive = true;
                    f->curbox = i;
                }
            }
        }
    }

    if (P_InAnim(f, ANIM_DOWN) && --f->downtimer <= 0 && !f->dead)
        P_SetAnim(f, ANIM_GETUP);
}

// --- script mode (-script): Primal Rage's bytecode ------------------------------

// Which of the game's 62 core rows plays each engine animation.
static const signed char scriptrows[NUMSTDANIMS] = {
    [ANIM_STAND] = ROW_STANCE, [ANIM_WALKF] = ROW_WALKF, [ANIM_WALKB] = ROW_WALKB,
    [ANIM_CROUCH] = ROW_CROUCH, [ANIM_JUMP] = ROW_JUMP, [ANIM_LAND] = ROW_LAND,
    [ANIM_BLOCK] = ROW_BLOCK, [ANIM_CROUCHBLOCK] = ROW_CROUCHBLOCK, [ANIM_HIT] = ROW_HIT,
    [ANIM_CROUCHHIT] = ROW_CROUCHHIT, [ANIM_HITHARD] = ROW_HITHARD, [ANIM_FALL] = ROW_FALL,
    [ANIM_DOWN] = ROW_LYING, [ANIM_GETUP] = ROW_LAND, [ANIM_DIZZY] = ROW_DIZZY, [ANIM_WIN] = ROW_WIN,
};

static void P_ScriptFrame(fighter_t *f)
{
    f->pstate.sprite = f->anim.sprite;
    f->pstate.frame = f->anim.frame;
    f->pstate.hflip = f->anim.hflip;
    f->pstate.step = f->anim.step;
}

static void P_ScriptMotion(fighter_t *f)
{
    if (f->anim.movex && f->pstate.kind != K_AIR)
        f->x += f->anim.movex * ARCADE_XSCALE * f->facing; // positive = forward
    if (f->anim.flipfacing)
        f->facing = -f->facing;
}

// Open the move's attack boxes whose step has come up.
static void P_ScriptBoxes(fighter_t *f)
{
    for (int i = 0; i < f->smove.numboxes; i++) {
        if (f->smove.boxes[i].step == f->anim.step) {
            f->boxtimer[i] = f->smove.boxes[i].ticks;
            f->curbox = i;
        }
    }
}

static void P_ScriptStart(fighter_t *f, int row, float duration)
{
    memset(f->boxtimer, 0, sizeof f->boxtimer);
    f->curbox = -1;
    f->attackhit = false;
    f->attacklive = false;
    f->state = &f->pstate;
    f->anim.linkstun = f->linkstun;
    PS_Start(&f->anim, PS_RowScript(f->def->codeindex, row), duration);
    P_ScriptFrame(f);
    P_ScriptMotion(f);
}

static bool P_ScriptAnim(fighter_t *f, animnum_t anim)
{
    if (anim < 0 || anim >= NUMSTDANIMS)
        return false;
    int row = scriptrows[anim];
    f->smove.valid = false;
    f->attack = NULL;
    f->pstate = (state_t){.anim = anim, .kind = f->def->anims[anim].kind};
    f->animnext = f->def->anims[anim].next;
    f->scripthold = f->def->anims[anim].hold || anim == ANIM_CROUCH || anim == ANIM_DOWN || anim == ANIM_JUMP;
    f->downtimer = anim == ANIM_DOWN ? DOWN_TICS : 0;
    P_ScriptStart(f, row, PS_RowDuration(row));
    return true;
}

static bool P_ScriptMove(fighter_t *f, int slot, animnum_t next)
{
    if (!PS_Move(f->def->codeindex, slot, &f->smove))
        return false;
    f->smoveattack = (attackinfo_t){
        .damage = f->smove.damage,
        .level = f->smove.hitclass == 4 || f->smove.hitclass == 5 ? LOW : HIGH,
        .push = 1.5f + f->smove.strength,
        .knockdown = f->smove.strength >= 3,
    };
    bool air = slot / 4 == MG_JUMP || slot / 4 == MG_AIR;
    f->pstate = (state_t){.anim = air ? ANIM_AIRMOVE : ANIM_MOVE, .kind = air ? K_AIR : K_ATTACK};
    f->animnext = next;
    f->scripthold = air;
    P_ScriptStart(f, f->smove.row, 2); // every move starts at 2 tics per frame
    f->attack = &f->smoveattack;
    P_ScriptBoxes(f);
    A_Swish(f, NULL);
    return true;
}

static void P_ScriptThink(fighter_t *f)
{
    bool advanced = PS_Tick(&f->anim);
    P_ScriptFrame(f);
    P_ScriptMotion(f);
    for (int i = 0; i < f->smove.numboxes; i++)
        if (f->boxtimer[i] > 0)
            f->boxtimer[i]--;
    if (advanced)
        P_ScriptBoxes(f);
    f->attacklive = false;
    for (int i = 0; i < f->smove.numboxes; i++) {
        if (f->smove.valid && f->boxtimer[i] > 0) {
            f->attacklive = true;
            f->curbox = i; // the latest box still out
        }
    }

    if (P_InAnim(f, ANIM_DOWN)) {
        if (--f->downtimer <= 0 && !f->dead)
            P_SetAnim(f, ANIM_GETUP);
    } else if (f->anim.done && !f->scripthold) {
        animnum_t n = f->animnext != ANIM_NONE ? f->animnext : (animnum_t)f->pstate.anim;
        P_SetAnim(f, n < NUMSTDANIMS ? n : ANIM_STAND);
    }
}

void P_AnimTick(fighter_t *f)
{
    if (p_scripted)
        P_ScriptThink(f);
    else
        P_AnimateCreature(f);
}

// --- physics --------------------------------------------------------------------

static void P_Landed(fighter_t *f)
{
    f->airattackused = false;
    if (f->state->kind == K_DOWN) {
        S_StartSound(sfx_thud, f->x, 0.8f);
        if (P_InAnim(f, ANIM_FALL))
            P_SetAnim(f, ANIM_DOWN);
    } else if (f->state->kind == K_AIR) {
        f->momx = 0;
        P_SetAnim(f, ANIM_LAND);
    }
}

void P_FighterThinker(fighter_t *f)
{
    P_AnimTick(f);
    if (f->dead && P_InAnim(f, ANIM_GETUP)) // KO'd: stay down
        P_SetAnim(f, ANIM_DOWN);

    // move
    f->x += f->momx;
    if (P_IsAirborne(f)) {
        f->z += f->momz;
        f->momz -= GRAVITY;
        if (f->z <= 0) {
            f->z = 0;
            f->momz = 0;
            P_Landed(f);
        }
    } else if (f->state->kind != K_FREE) {
        f->momx *= 0.82f; // friction while not walking
        if (fabsf(f->momx) < 0.05f)
            f->momx = 0;
    }

    // stay inside the arena and on screen
    float minx = MAX(16.f, level.camx + 14);
    float maxx = MIN(ARENA_WIDTH - 16.f, level.camx + VIEWWIDTH - 14);
    if (f->x < minx) {
        f->x = minx;
        f->momx = MAX(f->momx, 0);
    }
    if (f->x > maxx) {
        f->x = maxx;
        f->momx = MIN(f->momx, 0);
    }

    if (f->stun > 0)
        f->stun--;
    if (f->flash > 0)
        f->flash--;
    if (f->shownhealth > f->health)
        f->shownhealth = MAX((float)f->health, f->shownhealth - 0.5f);
}
