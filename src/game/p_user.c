// p_user.c -- turn human and CPU ticcmds into the same fighter actions.
#include <math.h>
#include "game/game.h"

static int P_ButtonIndex(int buttons)
{
    return buttons & BT_HF ? 1 : buttons & BT_LF ? 3 : buttons & BT_HQ ? 0 : 2;
}

bool P_MoveIsAir(int slot)
{
    return (slot >= MG_JUMP * 4 && slot < MG_JUMP * 4 + 4) || slot >= MP_JUMP;
}

// ROM rows 19/20, 37/38, 25/26, 31/32 are the four power pairs.
int P_InputMove(const fighter_t *f, const fighter_t *opp, int buttons)
{
    bool air = P_IsAirborne(f);
    bool crouch = !air && (buttons & BT_DOWN);
    bool close = fabsf(opp->x - f->x) < f->def->reach + opp->def->reach + 8;
    int high = BT_HQ | BT_HF, low = BT_LQ | BT_LF;
    if ((buttons & high) == high || (buttons & low) == low) {
        int base = air ? MP_JUMP : crouch ? MP_CROUCH : close ? MP_CLOSE : MP_STAND;
        return base + ((buttons & high) == high ? 0 : 1);
    }
    return (air ? MG_JUMP : crouch ? MG_CROUCH : close ? MG_CLOSE : MG_STAND) * 4 + P_ButtonIndex(buttons);
}

static bool P_TryMove(fighter_t *f, const fighter_t *opp, int buttons, animnum_t next)
{
    int slot = P_InputMove(f, opp, buttons);
    if (P_StartMove(f, slot, next))
        return true;
    // Photo creatures need only standing attacks; prefer the authored fierce
    // variant if they have no power pair, then the standing variant.
    int button = P_ButtonIndex(buttons);
    int group = P_IsAirborne(f) ? MG_JUMP : (buttons & BT_DOWN) ? MG_CROUCH : MG_STAND;
    if (slot >= MP_STAND && P_StartMove(f, group * 4 + button, next))
        return true;
    return !P_IsAirborne(f) && P_StartMove(f, MG_STAND * 4 + button, next);
}

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
    f->blocking = false;
    if (f->dead)
        return;

    if (f->dizzypending && f->state->kind == K_FREE && !P_IsAirborne(f)) {
        f->dizzypending = false;
        f->dizzy = true;
        f->mashdir = 0;
        f->momx = 0;
        if (!P_SetAnim(f, ANIM_DIZZY))
            P_SetAnim(f, ANIM_HIT);
    }
    if (f->dizzy) {
        int dir = !!(buttons & BT_RIGHT) - !!(buttons & BT_LEFT);
        if (dir && f->mashdir && dir != f->mashdir)
            f->brain = MAX(0, f->brain - 8);
        if (dir)
            f->mashdir = dir;
        if (f->brain > 0)
            return;
        f->dizzy = false;
        P_SetAnim(f, ANIM_STAND);
    }

    animkind_t kind = f->state->kind;
    if (kind == K_FREE && !P_IsAirborne(f)) {
        int want = opp->x >= f->x ? 1 : -1;
        if (fabsf(opp->x - f->x) > 4)
            f->facing = want;
    }
    // Guard follows the opponent's current side, including cross-ups. Attack
    // buttons and up cancel guard; airborne fighters cannot block.
    int away = opp->x >= f->x ? BT_LEFT : BT_RIGHT;
    int toward = away == BT_LEFT ? BT_RIGHT : BT_LEFT;
    f->blocking = (buttons & away) && !(buttons & (toward | BT_UP | BT_ATTACKS)) && !P_IsAirborne(f);
    int fwd = P_ForwardInput(f, buttons);

    if (kind == K_FREE && !P_IsAirborne(f)) {
        bool crouching = buttons & BT_DOWN;
        if (pressed & BT_ATTACKS) {
            f->momx = 0;
            if (P_TryMove(f, opp, buttons, crouching ? ANIM_CROUCH : ANIM_STAND))
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

    if (kind == K_AIR && P_InAnim(f, ANIM_JUMP) && !f->airattackused && (pressed & BT_ATTACKS)) {
        f->airattackused = true;
        P_TryMove(f, opp, buttons, ANIM_JUMP);
        return;
    }
    if (kind == K_HIT && f->stun == 0 && (P_InAnim(f, ANIM_BLOCK) || P_InAnim(f, ANIM_CROUCHBLOCK)))
        P_SetAnim(f, P_InAnim(f, ANIM_CROUCHBLOCK) ? ANIM_CROUCH : ANIM_STAND);
}
