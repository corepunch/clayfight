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

static void P_DamageFighter(fighter_t *vic, const fighter_t *att, const attackinfo_t *atk)
{
    vic->health = MAX(0, vic->health - atk->damage);
    vic->flash = 4;
    level.hitstop = HITSTOP;
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
    bool crouch = (vic->cmd.buttons & BT_DOWN) && vic->state->kind == K_FREE;
    // the reaction's length follows the hit's stun
    vic->linkstun = p_scripted ? att->smove.stun : att->move >= 0 ? att->def->moves[att->move].stun : 0;
    if (!(crouch && P_SetAnim(vic, ANIM_CROUCHHIT)))
        P_SetAnim(vic, ANIM_HIT);
    vic->stun = HITSTUN;
    vic->momx = att->facing * atk->push;
}

// Called every tic for the attacker. Returns true if something connected.
bool P_CheckAttack(fighter_t *att, fighter_t *vic)
{
    box_t hit, hurt;
    if (att->attackhit || vic->dead || vic->state->kind == K_DOWN)
        return false;
    if (!P_AttackBox(att, &hit) || !P_HurtBox(vic, &hurt) || !P_BoxesTouch(&hit, &hurt))
        return false;

    att->attackhit = true;
    const attackinfo_t *atk = att->attack;
    float sx = att->facing > 0 ? MAX(hit.x0, hurt.x0) : MIN(hit.x1, hurt.x1);
    float sz = (MAX(hit.z0, hurt.z0) + MIN(hit.z1, hurt.z1)) / 2;

    if (P_CanBlock(vic, atk)) {
        bool crouch = vic->cmd.buttons & BT_DOWN;
        if (!(crouch && P_SetAnim(vic, ANIM_CROUCHBLOCK)))
            P_SetAnim(vic, ANIM_BLOCK);
        vic->stun = BLOCKSTUN;
        vic->momx = att->facing * atk->push * 0.8f;
        vic->health = MAX(1, vic->health - atk->damage / 6); // chip damage
        level.hitstop = 3;
        P_SpawnSpark(sx, sz, SPARK_BLOCK);
        S_StartSound(sfx_block, sx, 0.8f);
        return true;
    }

    P_DamageFighter(vic, att, atk);
    P_SpawnSpark(sx, sz, atk->damage >= 10 ? SPARK_BIG : SPARK_HIT);
    return true;
}
