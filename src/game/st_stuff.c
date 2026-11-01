// st_stuff.c -- fight status bar: health, names, clock, round wins (cf. st_stuff.c)

#include <math.h>
#include <stdio.h>

#include "game/game.h"

static void ST_Art(gra_t *g,int frame,int x,int y,int flags)
{
    const patch_t *p=W_Frame(g,frame);
    V_DrawPatch(x,y,p,&g->palettes[W_FramePalette(g,frame)],flags);
}

void ST_Drawer(const int wins[2])
{
    gra_t *g=W_LoadGra("hud");
    for(int i=0;i<2;i++){
        const fighter_t *f=&level.fighters[i];
        int damage=CLAMP((int)lroundf((MAX_HEALTH-f->shownhealth)*120.f/MAX_HEALTH),0,120);
        ST_Art(g,(i?HUD_HEALTH_RIGHT:HUD_HEALTH_LEFT)+damage,i?184:32,8,0);
        ST_Art(g,(i?HUD_STUN_RIGHT:HUD_STUN_LEFT)+CLAMP(f->brain*68/MAX_BRAIN,0,68),i?184:72,24,0);

        // Retail heart script 57052 holds frame 1767, then beats through
        // 1768/1769 at four ticks each. The beat interval shortens as HP falls.
        int interval=MAX(12,f->health*60/MAX_HEALTH),phase=level.leveltime%interval;
        int beat=phase<4?1:phase<8?2:0;
        if(f->health<=0)beat=3+MIN(6,level.phasetime/4);
        ST_Art(g,HUD_HEART+beat,i?332:4,0,i?V_FLIP:0);
        ST_Art(g,HUD_BRAIN,i?288:48,16,i?V_FLIP:0);
        int c=f->def->codeindex;
        if(c>=0 && c<7){
            const patch_t *p=W_Frame(g,HUD_NAME+c);
            int x=i?328-p->w:8;
            ST_Art(g,HUD_NAME+c,x+p->x,38+p->y,0);
        }else{
            int w=V_TextWidth(FONT_BIG,f->def->name);
            float scale=MIN(0.7f,126.f/MAX(1,w));
            V_DrawTextScaled(FONT_BIG,i?328-(int)lroundf(w*scale):8,38,
                             f->def->name,0xffffffff,scale,0.7f);
        }
        if(wins[i]>0){
            char n[12];snprintf(n,sizeof n,"%d",wins[i]);
            V_DrawText(FONT_SMALL,i?190:142,41,n);
        }
    }
    if(gamemode==GM_TRAINING)V_DrawTextCentered(FONT_SMALL,VIEWWIDTH/2,VIEWHEIGHT-7,"TRAINING  /  ESC THEN Q TO LEAVE");
    if(gamemode==GM_ENDURANCE)V_DrawTextCentered(FONT_SMALL,VIEWWIDTH/2,VIEWHEIGHT-7,"ENDURANCE  /  FOUR FIGHTERS PER TEAM");
    if(gamemode==GM_TUG)V_DrawTextCentered(FONT_SMALL,VIEWWIDTH/2,VIEWHEIGHT-7,"TUG OF WAR");
    char clock[12];
    if(level.time<0)V_DrawTextCentered(FONT_SMALL,VIEWWIDTH/2,12,"--");
    else{
        snprintf(clock,sizeof clock,"%02d",CLAMP((level.time+TICRATE-1)/TICRATE,0,99));
        V_DrawTextCentered(FONT_TIMER,VIEWWIDTH/2,8,clock);
    }
}

// --- centre-screen announcements (cf. hu_stuff.c) ---------------------------

void HU_Drawer(void)
{
    int cx = VIEWWIDTH / 2, t = level.phasetime;
    char msg[48];
    if (level.phase == PH_INTRO) {
        if (t < 60) {
            snprintf(msg, sizeof msg, "ROUND %d", level.round);
            V_DrawTextCentered(FONT_BIG, cx, 70, msg);
        } else if (t & 8) {
            V_DrawTextCentered(FONT_BIG, cx, 70, "FIGHT!");
        }
    } else if (level.phase == PH_KO) {
        if (t < 60)
            V_DrawTextCentered(FONT_BIG, cx, 70, level.time == 0 ? "TIME" : level.winner == 2 ? "DRAW" : "KO!");
        else if (level.winner < 2) {
            snprintf(msg, sizeof msg, "%s WINS", level.fighters[level.winner].def->name);
            V_DrawTextCentered(FONT_BIG, cx, 60, msg);
        } else {
            V_DrawTextCentered(FONT_BIG, cx, 60, "NO WINNERS");
        }
    }
}
