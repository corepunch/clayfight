// Validate each arcade animation/move using the ROM interpreter itself.
#include <stdio.h>
#include <math.h>
#include "game/game.h"
int P_CheckAnims(void)
{
    int checked=0,failed=0;
    for(int c=0;c<7;c++)for(int slot=0;slot<NUMSTDANIMS+NUMMOVES;slot++){
        fighter_t f;P_SpawnFighter(&f,roster[c],0,ARENA_WIDTH/2,roster[c]->palette);
        bool ok=slot<NUMSTDANIMS?P_SetAnim(&f,(animnum_t)slot):P_StartMove(&f,slot-NUMSTDANIMS,ANIM_STAND);
        if(!ok){I_Log("FAIL arcade %s animation %d did not start",roster[c]->name,slot);failed++;continue;}
        for(int tic=0;tic<400;tic++){
            patch_t *p=P_StatePatch(f.state);
            if(!p || !p->hasbox || p->maxval>f.pal->count || !isfinite(f.x)){
                I_Log("FAIL arcade %s animation %d tic %d object %d",roster[c]->name,slot,tic,f.state->frame);failed++;break;
            }
            P_AnimTick(&f);
        }
        checked++;
    }
    I_Log("arcade animations: %d checked, %d failed",checked,failed);return failed;
}
