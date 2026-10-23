// info.c -- fighter state tables: linking and measuring (cf. info.c)

#include <string.h>
#include <strings.h>

#include "game/game.h"

const char *const stdanimnames[NUMSTDANIMS] = {
    "stand", "walkf", "walkb", "crouch", "jump", "land", "block", "crouchblock",
    "hit", "crouchhit", "hithard", "fall", "down", "getup", "dizzy", "win",
};

// What each standard animation is to the play code, and what follows it when
// the creature file doesn't say (the logic Primal Rage keeps in native code).
static const struct {
    animkind_t kind;
    animnum_t next;
} stdanims[NUMSTDANIMS] = {
    [ANIM_STAND] = {K_FREE, ANIM_NONE},
    [ANIM_WALKF] = {K_FREE, ANIM_NONE},
    [ANIM_WALKB] = {K_FREE, ANIM_NONE},
    [ANIM_CROUCH] = {K_FREE, ANIM_NONE},
    [ANIM_JUMP] = {K_AIR, ANIM_NONE},
    [ANIM_LAND] = {K_FREE, ANIM_STAND},
    [ANIM_BLOCK] = {K_HIT, ANIM_NONE},
    [ANIM_CROUCHBLOCK] = {K_HIT, ANIM_NONE},
    [ANIM_HIT] = {K_HIT, ANIM_STAND},
    [ANIM_CROUCHHIT] = {K_HIT, ANIM_CROUCH},
    [ANIM_HITHARD] = {K_HIT, ANIM_STAND},
    [ANIM_FALL] = {K_DOWN, ANIM_DOWN},
    [ANIM_DOWN] = {K_DOWN, ANIM_NONE},
    [ANIM_GETUP] = {K_DOWN, ANIM_STAND},
    [ANIM_DIZZY] = {K_HIT, ANIM_STAND},
    [ANIM_WIN] = {K_SCRIPT, ANIM_NONE},
};

bool P_Playable(const fighterdef_t *def)
{
    return def && def->numstates > 0 && def->anims[ANIM_STAND].count > 0;
}

int P_FindAnim(const fighterdef_t *def, const char *name)
{
    for (int i = 0; i < def->numanims; i++)
        if (!strcasecmp(def->anims[i].name, name))
            return i;
    return -1;
}

patch_t *P_StatePatch(const state_t *st)
{
    return W_Frame(st->sprite, st->frame);
}

// Forward reach per row. Art that faces left has "forward" = -x.
void P_PatchProfile(patch_t *p, bool facesleft)
{
    if (p->profile)
        return;
    p->profile = I_Malloc(p->h * sizeof *p->profile);
    p->front = -32768;
    for (int y = 0; y < p->h; y++) {
        p->profile[y] = -32768;
        for (int k = 0; k < p->w; k++) {
            int x = facesleft ? k : p->w - 1 - k;
            if (p->pixels[y * p->w + x]) {
                p->profile[y] = (int16_t)(facesleft ? p->x - x : x - p->x);
                break;
            }
        }
        p->front = MAX(p->front, p->profile[y]);
    }
}

static int StateFront(fighterdef_t *def, int index)
{
    patch_t *p = P_StatePatch(&def->states[index]);
    if (!p)
        return -999;
    P_PatchProfile(p, def->facesleft);
    return p->front;
}

// Moves without authored boxes attack with whatever sticks out in front of
// the idle pose: mark the frames reaching at least 70% of the way from idle
// reach to the move's peak reach. This needs no data at all, which is what
// photographed creatures need.
static void P_DeriveActiveFrames(fighterdef_t *def, const animinfo_t *a)
{
    int peak = -999, at = 0;
    for (int i = 0; i < a->count; i++) {
        int f = StateFront(def, a->first + i);
        if (f > peak) {
            peak = f;
            at = i;
        }
    }
    int thresh = def->reach + (peak - def->reach) * 7 / 10;
    int s = at, e = at;
    while (s > 0 && StateFront(def, a->first + s - 1) >= thresh)
        s--;
    while (e < a->count - 1 && StateFront(def, a->first + e + 1) >= thresh)
        e++;
    for (int i = s; i <= e; i++)
        def->states[a->first + i].active = true;
}

void P_FinishFighter(fighterdef_t *def)
{
    if (!P_Playable(def))
        I_Error("%s has no stand animation", def->name);

    // kinds and default successors; move animations take their kind from the
    // moves that use them
    for (int i = 0; i < def->numanims; i++) {
        animinfo_t *a = &def->anims[i];
        if (i < NUMSTDANIMS) {
            a->kind = stdanims[i].kind;
            if (a->next == ANIM_NONE && !a->hold && !a->loop)
                a->next = stdanims[i].next;
        } else {
            a->kind = K_ATTACK;
        }
    }
    for (int m = 0; m < NUMMOVES; m++) {
        const movedef_t *mv = &def->moves[m];
        if (mv->valid && (m / 4 == MG_JUMP || m / 4 == MG_AIR))
            def->anims[mv->anim].kind = K_AIR;
    }

    // link states: fall through, then hold / loop / end-of-animation (-1)
    for (int i = 0; i < def->numanims; i++) {
        animinfo_t *a = &def->anims[i];
        for (int k = 0; k < a->count; k++) {
            state_t *st = &def->states[a->first + k];
            st->anim = i;
            st->step = k;
            st->kind = a->kind;
            if (k < a->count - 1)
                st->next = a->first + k + 1;
            else if (a->hold)
                st->next = a->first + k;
            else if (a->next == ANIM_NONE && (i < NUMSTDANIMS || a->loop))
                st->next = a->first + CLAMP(a->loopfrom, 0, a->count - 1);
            else
                st->next = -1; // the play code decides what comes next
        }
    }

    def->reach = StateFront(def, def->anims[ANIM_STAND].first);
    for (int m = 0; m < NUMMOVES; m++)
        if (def->moves[m].valid && !def->moves[m].numboxes)
            P_DeriveActiveFrames(def, &def->anims[def->moves[m].anim]);
}
