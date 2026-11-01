// p_map.c -- body/attack boxes and fighter collision (cf. p_map.c)
//
// Boxes come straight from the sprites (see info.c): the hurt box is the body
// around the hotspot, the attack box is whatever part of an active attack
// frame reaches past the fighter's idle pose.

#include <math.h>

#include "game/game.h"

// Facing-relative forward range + hotspot-relative rows -> world box.
static void ToWorld(const fighter_t *f, float fwd0, float fwd1, float y0, float y1, box_t *out)
{
    float a = f->x + f->facing * fwd0, b = f->x + f->facing * fwd1;
    out->x0 = MIN(a, b);
    out->x1 = MAX(a, b);
    float z = f->z + P_SpriteLift(f);
    out->z0 = z - y1;
    out->z1 = z - y0;
}

bool P_HurtBox(const fighter_t *f, box_t *out)
{
    patch_t *p = P_StatePatch(f->state);
    if (!p || !p->hasbox)
        return false;
    P_PatchProfile(p, f->def->facesleft);
    float rawback = f->def->facesleft ? -p->bx1 : p->bx0;
    float back = MAX(rawback, -f->def->width / 2.f); // ignore long tails
    ToWorld(f, back, p->front, p->by0, p->by1, out);
    return true;
}

// An authored box in art pixels around the hotspot (art unmirrored, y down).
static void P_ArtBox(const fighter_t *f, float x0, float y0, float x1, float y1, box_t *out)
{
    if (f->def->facesleft)
        ToWorld(f, -x1, -x0, y0, y1, out);
    else
        ToWorld(f, x0, x1, y0, y1, out);
}

bool P_AttackBox(const fighter_t *f, box_t *out)
{
    if (!f->attack || !f->attacklive || (f->curbox < 0 && p_scripted && f->def->codeindex >= 0))
        return false;
    if (p_scripted && f->def->codeindex >= 0) {
        // Native 26.6 coordinates: box bytes shift by 8, anchors by 6
        // (68020 0x249d2..0x24a50). Thus one box unit is four pixels.
        const psbox_t *b = &f->smove.boxes[f->curbox];
        int ax = f->anim.ax, ay = f->anim.ay;
        if (f->def->codeindex == 5 || f->def->codeindex == 6) {
            // Native 0x27048: Diablo/Chaos use adjusted combat anchors.
            ax += (127 - ax) / 8;
            ay += (95 - ay) / 8;
        }
        float x0 = -ax + b->x * 4, y0 = -ay + b->y * 4;
        P_ArtBox(f, x0, y0, x0 + b->w * 4, y0 + b->h * 4, out);
        return true;
    }
    const movedef_t *mv = &f->def->moves[f->move];
    if (mv->numboxes) {
        const hitbox_t *b = &mv->boxes[f->curbox];
        P_ArtBox(f, b->x0, b->y0, b->x1, b->y1, out);
        return true;
    }
    // no authored boxes: whatever sticks out in front of the idle pose
    const state_t *st = f->state;
    patch_t *p = P_StatePatch(st);
    if (!p || !p->hasbox)
        return false;
    P_PatchProfile(p, f->def->facesleft);
    int limit = MIN(p->front - 6, f->def->reach - 4);
    int y0 = -1, y1 = -1;
    for (int y = 0; y < p->h; y++) {
        if (p->profile[y] > limit) {
            if (y0 < 0)
                y0 = y;
            y1 = y;
        }
    }
    if (y0 < 0)
        return false;
    ToWorld(f, MIN(limit, p->front - 18), p->front, y0 - p->y, y1 - p->y, out);
    return true;
}

bool P_BoxesTouch(const box_t *a, const box_t *b)
{
    return a->x0 <= b->x1 && b->x0 <= a->x1 && a->z0 <= b->z1 && b->z0 <= a->z1;
}

// Keep the two bodies from overlapping (push boxes).
void P_SeparateFighters(fighter_t *a, fighter_t *b)
{
    if (a->dead || b->dead)
        return;
    // hotspots sit at the feet, so keep the two fronts (snout, fists) apart too
    float mindist = MAX((a->def->width + b->def->width) / 2.f, a->def->reach + b->def->reach - 12.f);
    float dx = b->x - a->x, dist = fabsf(dx);
    if (dist >= mindist || fabsf(a->z - b->z) > 40)
        return;
    float push = (mindist - dist) / 2, dir = dx >= 0 ? 1 : -1;
    a->x -= dir * push;
    b->x += dir * push;
}
