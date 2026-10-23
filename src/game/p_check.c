// p_check.c -- -checkanims: play every animation and move from the creature
// files and through Primal Rage's own script interpreter, tic by tic, and
// report where they differ. This is how the converter (tools/pr2creature.py)
// is held to the original.

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "game/game.h"

#define MAXTICS 400

static bool verbose;

typedef struct {
    gra_t *sprite;
    int frame;
    float x;
    bool live;
    box_t box;
    uint32_t ip; // script mode: where the interpreter is
} sample_t;

// Record what one animation shows each tic until it hands over to another.
static int P_Record(fighterdef_t *def, bool script, int anim, int slot, sample_t *out)
{
    static fighter_t f;
    p_scripted = script;
    P_SpawnFighter(&f, def, 0, 200, def->palette);
    f.linkstun = 20; // a medium hit, for stun-scaled reactions
    if (slot >= 0) {
        if (!P_StartMove(&f, slot, ANIM_STAND))
            return 0;
    } else if (!P_SetAnim(&f, anim)) {
        return 0;
    }
    int start = f.state->anim, n = 0;
    for (; n < MAXTICS; n++) {
        if (f.state->anim != start || (slot >= 0 && !f.attack))
            break;
        sample_t *s = &out[n];
        s->sprite = f.state->sprite;
        s->frame = f.state->frame;
        s->x = f.x;
        s->live = P_AttackBox(&f, &s->box);
        s->ip = f.anim.ip;
        P_AnimTick(&f);
    }
    return n;
}

// Compare two recordings over their common length.
static int P_Compare(const char *who, const char *what, const sample_t *a, int na, const sample_t *b, int nb, bool air)
{
    int n = MIN(na, nb), bad = 0, first = -1;
    for (int t = 0; t < n; t++) {
        bool diff = a[t].sprite != b[t].sprite || a[t].frame != b[t].frame || fabsf(a[t].x - b[t].x) > 1.5f
                    || a[t].live != b[t].live
                    || (a[t].live && (fabsf(a[t].box.x0 - b[t].box.x0) > 3 || fabsf(a[t].box.z1 - b[t].box.z1) > 3));
        if (diff && first < 0)
            first = t;
        bad += diff;
    }
    if (bad || (!air && abs(na - nb) > 2)) {
        I_Log("  %-8s %-12s %3d/%3d tics differ (first at %d), lengths %d vs %d", who, what, bad, n, first, na, nb);
        if (verbose && first >= 0) {
            int back = getenv("CHECK_BACK") ? atoi(getenv("CHECK_BACK")) : 2;
            for (int t = MAX(0, first - back); t < MIN(n, first + 6); t++)
                I_Log("      t%-3d file %-13s #%-3d x%6.1f live%d box %4.0f..%-4.0f z%4.0f..%-4.0f | %-13s #%-3d x%6.1f live%d box %4.0f..%-4.0f z%4.0f..%-4.0f ip %x", t,
                      a[t].sprite->name, a[t].frame, a[t].x, a[t].live, a[t].box.x0, a[t].box.x1, a[t].box.z0, a[t].box.z1,
                      b[t].sprite->name, b[t].frame, b[t].x, b[t].live, b[t].box.x0, b[t].box.x1, b[t].box.z0, b[t].box.z1, b[t].ip);
        }
        return 1;
    }
    return 0;
}

int P_CheckAnims(void)
{
    static sample_t a[MAXTICS], b[MAXTICS];
    int checked = 0, failed = 0;
    bool was = p_scripted;
    verbose = getenv("CHECK_VERBOSE") != NULL;
    for (int r = 0; r < numroster; r++) {
        fighterdef_t *def = roster[r];
        if (def->codeindex < 0)
            continue;
        for (int anim = 0; anim < NUMSTDANIMS; anim++) {
            if (!def->anims[anim].count || def->anims[anim].wanderfrom >= 0)
                continue; // the breathing idles are random by design
            int na = P_Record(def, false, anim, -1, a), nb = P_Record(def, true, anim, -1, b);
            checked++;
            failed += P_Compare(def->name, stdanimnames[anim], a, na, b, nb, anim == ANIM_JUMP);
        }
        for (int m = 0; m < NUMMOVES; m++) {
            if (!def->moves[m].valid)
                continue;
            char what[32];
            snprintf(what, sizeof what, "move %d", m);
            int na = P_Record(def, false, -1, m, a), nb = P_Record(def, true, -1, m, b);
            checked++;
            failed += P_Compare(def->name, what, a, na, b, nb, m / 4 == MG_JUMP || m / 4 == MG_AIR);
        }
    }
    p_scripted = was;
    I_Log("P_CheckAnims: %d animations and moves checked, %d differ", checked, failed);
    return failed;
}
