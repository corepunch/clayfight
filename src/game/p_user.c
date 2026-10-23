// p_user.c -- turn ticcmds into fighter actions (cf. p_user.c)

#include <math.h>

#include "game/game.h"

// Button order inside each move group: HQ HF LQ LF. Fierce wins over quick.
static int P_ButtonIndex(int pressed)
{
    return pressed & BT_HF ? 1 : pressed & BT_LF ? 3 : pressed & BT_HQ ? 0 : 2;
}

// Try the move for this group and button, falling back to the plain
// standing (or jumping) version when a creature doesn't have it.
static bool P_TryMove(fighter_t *f, int group, int button, animnum_t next)
{
    if (P_StartMove(f, group * 4 + button, next))
        return true;
    int fallback = group == MG_CLOSE || group == MG_POWER ? MG_STAND : group == MG_AIR ? MG_JUMP : -1;
    return fallback >= 0 && P_StartMove(f, fallback * 4 + button, next);
}

// +1 = towards where the fighter faces, -1 = away.
int P_ForwardInput(const fighter_t *f, int buttons)
{
    int right = (buttons & BT_RIGHT) != 0, left = (buttons & BT_LEFT) != 0;
    return (right - left) * f->facing;
}

void P_PlayerThink(fighter_t *f, const fighter_t *opp)
{
    int buttons = f->cmd.buttons;
    int pressed = buttons & ~f->oldbuttons;
    f->oldbuttons = buttons;
    if (f->dead)
        return;

    int fwd = P_ForwardInput(f, buttons);
    animkind_t kind = f->state->kind;
    // holding away from the opponent means "guard", like most fighters
    f->blocking = fwd < 0 && !P_IsAirborne(f);

    if (kind == K_FREE && !P_IsAirborne(f)) {
        // always face the opponent while in control on the ground
        int want = opp->x >= f->x ? 1 : -1;
        if (want != f->facing && fabsf(opp->x - f->x) > 4)
            f->facing = want;

        bool crouching = buttons & BT_DOWN;
        if (pressed & BT_ATTACKS) {
            bool close = fabsf(opp->x - f->x) < f->def->reach + opp->def->reach + 8;
            int group = crouching ? MG_CROUCH : close ? MG_CLOSE : MG_STAND;
            f->momx = 0;
            if (P_TryMove(f, group, P_ButtonIndex(pressed), crouching ? ANIM_CROUCH : ANIM_STAND))
                return;
        }
        if (buttons & BT_UP) {
            f->momz = f->def->jumpspeed;
            f->momx = fwd * 2.2f * f->facing;
            P_SetAnim(f, ANIM_JUMP);
            return;
        }
        if (crouching) {
            f->momx = 0;
            if (!P_InAnim(f, ANIM_CROUCH))
                P_SetAnim(f, ANIM_CROUCH);
            return;
        }
        animnum_t want_anim = fwd > 0 ? ANIM_WALKF : fwd < 0 ? ANIM_WALKB : ANIM_STAND;
        f->momx = fwd > 0 ? f->def->walkspeed * f->facing : fwd < 0 ? -f->def->backspeed * f->facing : 0;
        if (!P_InAnim(f, want_anim))
            P_SetAnim(f, want_anim);
        return;
    }

    // one attack per jump: straight up uses the jump group, across the air group
    if (kind == K_AIR && P_InAnim(f, ANIM_JUMP) && !f->airattackused && (pressed & BT_ATTACKS)) {
        f->airattackused = true;
        P_TryMove(f, fabsf(f->momx) > 0.5f ? MG_AIR : MG_JUMP, P_ButtonIndex(pressed), ANIM_JUMP);
        return;
    }

    // leave the guard pose once blockstun runs out
    if (kind == K_HIT && f->stun == 0 && (P_InAnim(f, ANIM_BLOCK) || P_InAnim(f, ANIM_CROUCHBLOCK)))
        P_SetAnim(f, P_InAnim(f, ANIM_CROUCHBLOCK) ? ANIM_CROUCH : ANIM_STAND);
}
