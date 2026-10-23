// p_inter.c -- attacks connecting: damage, blocking, knockdown (cf. p_inter.c)

#include "game/game.h"

#define HITSTOP 5 // freeze frames on impact
#define BLOCKSTUN 14
#define HITSTUN 18

static bool P_CanBlock(const fighter_t *vic, const attackinfo_t *atk)
{
    if (!vic->blocking)
        return false;
    animkind_t k = vic->state->kind;
    bool guarding = k == K_FREE || (k == K_HIT && (P_InAnim(vic, ANIM_BLOCK) || P_InAnim(vic, ANIM_CROUCHBLOCK)));
    if (!guarding)
        return false;
    bool crouch = vic->cmd.buttons & BT_DOWN;
    return atk->level != LOW || crouch; // lows must be blocked low
}

static void P_KnockDown(fighter_t *vic, const fighter_t *att, float up)
{
    P_SetAnim(vic, ANIM_FALL);
    vic->momz = up;
    vic->z = MAX(vic->z, 1);
    vic->momx = att->facing * 2.5f;
    vic->stun = 0;
}

static void P_DamageFighter(fighter_t *vic, const fighter_t *att, const attackinfo_t *atk, const hit_t *h)
{
    vic->health = MAX(0, vic->health - atk->damage);
    vic->flash = 4;
    level.hitstop = HITSTOP; // on a trade both sides share one freeze
    S_StartSound(atk->damage >= 10 ? sfx_hithard : sfx_hit, vic->x, 0.9f);

    if (vic->health == 0) {
        vic->dead = true;
        P_Voice(vic, 2, 1);
        P_KnockDown(vic, att, 4.5f);
        return;
    }
    if (atk->knockdown || P_IsAirborne(vic)) {
        P_Voice(vic, 3, 0.7f);
        P_KnockDown(vic, att, 3);
        return;
    }
    // the reaction's length follows the hit's stun
    vic->linkstun = h->stun;
    if (!(h->crouch && P_SetAnim(vic, ANIM_CROUCHHIT)))
        P_SetAnim(vic, ANIM_HIT);
    vic->stun = HITSTUN;
    vic->momx = att->facing * atk->push;
}

// Hits are resolved in two steps so that a trade (both attacks landing on
// the same tic) is symmetric: P_AttackLands only looks, for both fighters,
// and P_ApplyHit then changes them. Everything the second step needs is
// captured in hit_t, because applying the first hit puts its victim into a
// reaction, which clears the attack that victim was making.
bool P_AttackLands(const fighter_t *att, const fighter_t *vic, hit_t *out)
{
    box_t hit, hurt;
    if (att->attackhit || vic->dead || vic->state->kind == K_DOWN)
        return false;
    if (!P_AttackBox(att, &hit) || !P_HurtBox(vic, &hurt) || !P_BoxesTouch(&hit, &hurt))
        return false;
    out->atk = att->attack;
    out->stun = p_scripted ? att->smove.stun : att->move >= 0 ? att->def->moves[att->move].stun : 0;
    out->blocked = P_CanBlock(vic, out->atk);
    out->crouch = (vic->cmd.buttons & BT_DOWN) && (out->blocked || vic->state->kind == K_FREE);
    out->x = att->facing > 0 ? MAX(hit.x0, hurt.x0) : MIN(hit.x1, hurt.x1);
    out->z = (MAX(hit.z0, hurt.z0) + MIN(hit.z1, hurt.z1)) / 2;
    return true;
}

void P_ApplyHit(fighter_t *att, fighter_t *vic, const hit_t *h)
{
    const attackinfo_t *atk = h->atk;
    att->attackhit = true;

    if (h->blocked) {
        if (!(h->crouch && P_SetAnim(vic, ANIM_CROUCHBLOCK)))
            P_SetAnim(vic, ANIM_BLOCK);
        vic->stun = BLOCKSTUN;
        vic->momx = att->facing * atk->push * 0.8f;
        vic->health = MAX(1, vic->health - atk->damage / 6); // chip damage
        level.hitstop = MAX(level.hitstop, 3);
        P_SpawnSpark(h->x, h->z, SPARK_BLOCK);
        S_StartSound(sfx_block, h->x, 0.8f);
        return;
    }

    P_DamageFighter(vic, att, atk, h);
    P_SpawnSpark(h->x, h->z, atk->damage >= 10 ? SPARK_BIG : SPARK_HIT);
}
