// r_local.h -- renderer: arenas and the fight scene (cf. r_local.h)

#ifndef __R_LOCAL__
#define __R_LOCAL__

#include "pr_def.h"
#include "w_gra.h"

#define HORIZON_Y 128
#define MAXPROPS 3

typedef struct stage_s {
    const char *file;
    const char *name;
    // built by R_PrepareStage
    bool prepared;
    patch_t *sky;
    const palette_t *skypal;
    int numprops;
    patch_t *props[MAXPROPS];
    const palette_t *proppal[MAXPROPS];
    float propx[MAXPROPS];
    pixel_t floortop, floorbottom, floorline;
} stage_t;

extern stage_t stages[];
extern int numstages;
extern bool r_showboxes;

void R_PrepareStage(stage_t *stage);
void R_DrawStage(const stage_t *stage, int camx);
void R_RenderFight(void);

#endif
