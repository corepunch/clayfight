// r_main.c -- draw the fight scene (cf. r_main.c / r_things.c)

#include <math.h>

#include "game/game.h"

bool r_showboxes;

static void R_DrawShadow(const fighter_t *f, int camx)
{
    int cx = (int)lroundf(f->x) - camx;
    if(f->def->codeindex>=0){
        patch_t *p=W_ArcadeShadow(f->def->codeindex,f->state->frame);
        int flags=((f->def->facesleft?f->facing>0:f->facing<0)!=f->state->hflip)?V_FLIP:0;
        uint32_t desc=X_U32(0x6d5e0+f->def->codeindex*4);
        float scale=X_U16(desc+12)/4096.f;
        V_DrawPatchScaled(cx,FLOOR_Y,p,W_ArcadeStagePalette((int)(level.stage-stages),0x100),flags,scale,scale);
        return;
    }
    // Photo fighters have no authored arcade shadow bank; their existing
    // ground-contact ellipse follows the photographed silhouette's width.
    float shrink = MAX(0.4f, 1 - f->z / 160);
    int rx = (int)lroundf(f->def->width * 0.9f * shrink);
    int ry = MAX(2, (int)lroundf(5 * shrink));
    for (int y = -ry; y <= ry; y++) {
        int half = (int)lroundf(rx * sqrtf(1 - (float)(y * y) / (ry * ry)));
        V_FillRect(cx - half, FLOOR_Y + y, half * 2, 1, 0xff101010);
    }
}

static void R_DrawFighter(const fighter_t *f, int camx)
{
    patch_t *p = P_StatePatch(f->state);
    int flags = 0;
    if ((f->def->facesleft ? f->facing > 0 : f->facing < 0) != f->state->hflip)
        flags |= V_FLIP;
    if (f->flash & 2)
        flags |= V_FLASH;
    V_DrawPatch((int)lroundf(f->x) - camx, FLOOR_Y - (int)lroundf(f->z) - P_SpriteLift(f), p, f->pal, flags);
}

static void R_Outline(const box_t *b, int camx, pixel_t color)
{
    int x0 = (int)b->x0 - camx, x1 = (int)b->x1 - camx;
    int y0 = FLOOR_Y - (int)b->z1, y1 = FLOOR_Y - (int)b->z0;
    V_FillRect(x0, y0, x1 - x0 + 1, 1, color);
    V_FillRect(x0, y1, x1 - x0 + 1, 1, color);
    V_FillRect(x0, y0, 1, y1 - y0 + 1, color);
    V_FillRect(x1, y0, 1, y1 - y0 + 1, color);
}

void R_RenderFight(void)
{
    int camx = (int)lroundf(level.camx);
    R_DrawStage(level.stage, camx);

    const fighter_t *a = &level.fighters[0], *b = &level.fighters[1];
    R_DrawShadow(a, camx);
    R_DrawShadow(b, camx);
    // the attacker goes on top so the striking limb overlaps the victim
    if (a->state->kind == K_ATTACK && b->state->kind != K_ATTACK) {
        const fighter_t *t = a;
        a = b;
        b = t;
    }
    R_DrawFighter(a, camx);
    R_DrawFighter(b, camx);

    for (int i = 0; i < level.numparticles; i++) {
        const particle_t *p = &level.particles[i];
        V_DrawPatch((int)lroundf(p->x)-camx,FLOOR_Y-(int)lroundf(p->z),
                    W_Frame(W_LoadGra("arcade"),p->animation.frameid),
                    W_ArcadeImpactPalette(p->blocked),0);
    }

    R_DrawStageForeground(level.stage, camx);

    if (r_showboxes) {
        for (int i = 0; i < 2; i++) {
            const fighter_t *f = &level.fighters[i];
            box_t box;
            if (P_HurtBox(f, &box))
                R_Outline(&box, camx, 0xff00ff00);
            if (P_AttackBox(f, &box))
                R_Outline(&box, camx, 0xffff0000);
            V_FillRect((int)f->x - camx - 1, FLOOR_Y - 1, 3, 3, 0xffffffff);
        }
    }
}
