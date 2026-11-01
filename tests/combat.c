// Focused controller/AI rules, plus optional ROM-backed integration and captures.
#include "test.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "game/game.h"

static fighterdef_t fixture;
static state_t states[NUMSTDANIMS + NUMMOVES];
static palette_t palette;
static gra_t art = {.numpalettes=1, .palettes=&palette};

static void setup(void)
{
    memset(&fixture, 0, sizeof fixture);
    memset(states, 0, sizeof states);
    fixture.codeindex = -1;
    fixture.base = &art;
    fixture.states = states;
    fixture.numstates = fixture.numanims = NUMSTDANIMS + NUMMOVES;
    fixture.reach = fixture.width = 20;
    fixture.walkspeed = 2; fixture.backspeed = 1; fixture.jumpspeed = 8;
    for (int i=0; i<fixture.numanims; i++) {
        animkind_t kind = i >= NUMSTDANIMS ? K_ATTACK :
            i == ANIM_JUMP ? K_AIR : i == ANIM_LAND ? K_SCRIPT : i >= ANIM_FALL && i <= ANIM_GETUP ? K_DOWN :
            i >= ANIM_BLOCK && i <= ANIM_DIZZY ? K_HIT : K_FREE;
        states[i] = (state_t){.anim=i,.kind=kind,.tics=1,.next=-1};
        fixture.anims[i] = (animinfo_t){.first=i,.count=1,.next=ANIM_STAND,.wanderfrom=-1,.kind=kind};
    }
    for (int m=0; m<NUMMOVES; m++) {
        fixture.moves[m] = (movedef_t){.valid=true,.anim=NUMSTDANIMS+m,.stun=20,
            .attack={.damage=1,.push=0}};
        if (P_MoveIsAir(m)) states[NUMSTDANIMS+m].kind=K_AIR;
    }
    memset(&level, 0, sizeof level);
    level.phase=PH_FIGHT; level.time=3600;
    p_scripted=false; options.sound=false; options.difficulty=2;
    P_SpawnFighter(&level.fighters[0], &fixture, 0, 100, 0);
    P_SpawnFighter(&level.fighters[1], &fixture, 1, 200, 0);
    M_ClearRandom();
}

TEST(combat_move_selection)
{
    setup();
    fighter_t *f=&level.fighters[0], *o=&level.fighters[1];
    const int buttons[]={BT_HQ,BT_HF,BT_LQ,BT_LF,BT_HQ|BT_HF,BT_LQ|BT_LF};
    for(int posture=0;posture<4;posture++)for(int b=0;b<6;b++) {
        P_SetAnim(f,ANIM_STAND); f->oldbuttons=0;
        o->x=posture==1 ? 130 : 200;
        f->z=posture==3 ? 20 : 0;
        if(posture==3) P_SetAnim(f,ANIM_JUMP);
        f->cmd.buttons=buttons[b] | (posture==2 ? BT_DOWN : 0);
        P_PlayerThink(f,o);
        int want=b<4 ? posture*4+b : MP_STAND+posture*2+b-4;
        CHECK(f->move==want);
        CHECK((f->state->kind==K_AIR)==(posture==3));
        f->airattackused=false;
    }
    // Sparse photographs fall back to their standing fierce pose for a pair.
    fixture.moves[MP_STAND].valid=false;
    f->z=0; P_SetAnim(f,ANIM_STAND); f->oldbuttons=0;
    f->cmd.buttons=BT_HQ|BT_HF; P_PlayerThink(f,o);
    CHECK(f->move==1);
}

TEST(combat_guard_and_recovery)
{
    setup();
    fighter_t *f=&level.fighters[0], *o=&level.fighters[1];
    f->facing=-1; f->cmd.buttons=BT_LEFT; P_PlayerThink(f,o);
    CHECK(f->blocking && f->facing==1); // turn before interpreting movement
    CHECK(f->momx<0);
    f->cmd.buttons=BT_LEFT|BT_HQ; P_PlayerThink(f,o); CHECK(!f->blocking);
    P_SetAnim(f,ANIM_JUMP);f->z=20;f->cmd.buttons=BT_LEFT;
    P_PlayerThink(f,o);CHECK(!f->blocking);
    f->z=0;P_SetAnim(f,ANIM_BLOCK);f->stun=14;
    for(int t=0;t<13;t++) { P_FighterThinker(f); CHECK(P_InAnim(f,ANIM_BLOCK)); }
    P_FighterThinker(f);P_PlayerThink(f,o);CHECK(P_InAnim(f,ANIM_STAND));
    P_SetAnim(f,ANIM_HIT);f->stun=25;
    for(int t=0;t<24;t++) { P_FighterThinker(f); CHECK(f->state->kind==K_HIT); }
    P_FighterThinker(f);P_FighterThinker(f);CHECK(f->state->kind==K_FREE);
}

TEST(combat_brain_and_dizzy)
{
    setup();fighter_t *f=&level.fighters[0], *o=&level.fighters[1];
    // Full particle pool suppresses resource-only effects in this rule fixture.
    level.numparticles=MAXPARTICLES;
    hit_t hit={.atk=&fixture.moves[0].attack,.stun=25,.blocked=true};
    f->brain=MAX_BRAIN-1;P_ApplyHit(o,f,&hit);
    CHECK(f->brain==MAX_BRAIN && !f->dizzypending && !f->dizzy);
    hit.blocked=false;P_ApplyHit(o,f,&hit);
    CHECK(f->dizzypending && !f->dizzy && f->stun==25);
    f->cmd.buttons=BT_HQ;P_PlayerThink(f,o);CHECK(!f->dizzy);
    for(int t=0;t<26;t++)P_FighterThinker(f);
    P_PlayerThink(f,o);CHECK(f->dizzy && P_InAnim(f,ANIM_DIZZY));
    int brain=f->brain;
    f->cmd.buttons=BT_LEFT;P_PlayerThink(f,o);
    P_PlayerThink(f,o);CHECK(f->brain==brain); // holding does not mash
    f->cmd.buttons=BT_RIGHT;P_PlayerThink(f,o);CHECK(f->brain<brain);
    for(int t=0;t<110;t++) { P_FighterThinker(f);P_PlayerThink(f,o); }
    CHECK(!f->dizzy && f->brain==0);
    setup();f=&level.fighters[0];o=&level.fighters[1];level.numparticles=MAXPARTICLES;
    attackinfo_t knock=fixture.moves[0].attack;knock.knockdown=true;
    hit=(hit_t){.atk=&knock,.stun=25};f->brain=90;P_ApplyHit(o,f,&hit);
    P_PlayerThink(f,o);CHECK(f->dizzypending && !f->dizzy && f->state->kind==K_DOWN);
    f->z=f->momz=0;P_SetAnim(f,ANIM_STAND);P_PlayerThink(f,o);CHECK(f->dizzy);
    setup();f=&level.fighters[0];f->brain=40;f->braindelay=BRAIN_RECOVERY_DELAY;
    for(int t=0;t<BRAIN_RECOVERY_DELAY;t++)P_FighterThinker(f);
    CHECK(f->brain==40);P_FighterThinker(f);CHECK(f->brain==39);
}

TEST(combat_cpu_commitment)
{
    setup();fighter_t *f=&level.fighters[0], *o=&level.fighters[1];ticcmd_t cmd;
    f->aihold=4;f->aiwait=6;f->aiplan=BT_LEFT|BT_DOWN;
    P_CpuCmd(f,o,&cmd);CHECK(cmd.buttons==(BT_LEFT|BT_DOWN));
    o->x=50;P_CpuCmd(f,o,&cmd);CHECK(cmd.buttons==(BT_RIGHT|BT_DOWN));
    level.hitstop=3;int hold=f->aihold,wait=f->aiwait;
    P_CpuCmd(f,o,&cmd);CHECK(cmd.buttons==0 && f->aihold==hold && f->aiwait==wait);
    level.hitstop=0;level.phase=PH_INTRO;P_CpuCmd(f,o,&cmd);
    CHECK(cmd.buttons==0 && f->aihold==hold && f->aiwait==wait);
    level.phase=PH_FIGHT;P_SetAnim(f,ANIM_HIT);P_CpuCmd(f,o,&cmd);
    CHECK(cmd.buttons==0 && f->aihold==0);
    P_SetAnim(f,ANIM_JUMP);f->z=20;f->momz=-1;f->aiwait=0;o->x=135;
    P_CpuCmd(f,o,&cmd);CHECK(cmd.buttons&BT_ATTACKS);
    f->cmd=cmd;P_PlayerThink(f,o);CHECK(f->airattackused && P_MoveIsAir(f->move));
    setup();f=&level.fighters[0];o=&level.fighters[1];o->x=140;
    int powers=0;
    for(int t=0;t<64;t++) {
        f->aiwait=f->aihold=0;P_CpuCmd(f,o,&cmd);
        if((cmd.buttons&(BT_HQ|BT_HF))==(BT_HQ|BT_HF) ||
           (cmd.buttons&(BT_LQ|BT_LF))==(BT_LQ|BT_LF))powers++;
    }
    CHECK(powers>0);
    // Difficulty changes the decision interval, not the fighter's rules.
    setup();f=&level.fighters[0];o=&level.fighters[1];o->x=300;
    options.difficulty=1;P_CpuCmd(f,o,&cmd);int easy=f->aiwait;
    setup();f=&level.fighters[0];o=&level.fighters[1];o->x=300;
    options.difficulty=3;P_CpuCmd(f,o,&cmd);CHECK(f->aiwait<easy);
}

TEST(combat_air_move_end_and_sparse_landing)
{
    setup();fighter_t *f=&level.fighters[0];
    f->z=20;CHECK(P_StartMove(f,12,ANIM_JUMP));
    P_AnimTick(f);
    CHECK(P_InAnim(f,ANIM_JUMP) && !f->attack && !f->attacklive);
    fixture.anims[ANIM_LAND].count=0;
    f->z=1;f->momz=-2;P_FighterThinker(f);
    CHECK(P_InAnim(f,ANIM_STAND) && f->z==0 && f->momz==0);
    f->z=100;f->momz=-1;P_SetAnim(f,ANIM_FALL);
    P_AnimTick(f);CHECK(P_InAnim(f,ANIM_FALL));
}

static const char *capture_dir;
static void animation_scripts(void)
{
    const int rows[]={0,7,10,1,ROW_JUMPMOVE,13,38,40,42,43,44,46,ROW_LYING,ROW_GETUP,54,58};
    FILE *report=NULL;
    if(capture_dir) {
        char path[512];snprintf(path,sizeof path,"%s/scripts.csv",capture_dir);
        report=fopen(path,"w");CHECK(report);
        fputs("character,slot,row,ticks,frames,done,min_bottom,max_bottom,min_move_y,max_move_y,mirrored_frames\n",report);
    }
    for(int c=0;c<7;c++)for(int slot=0;slot<NUMSTDANIMS+NUMMOVES;slot++) {
        psmove_t move;int row;
        if(slot<NUMSTDANIMS)row=rows[slot];
        else {CHECK(PS_Move(c,slot-NUMSTDANIMS,&move));row=move.row;}
        psanim_t vm={0};PS_StartRow(&vm,c,row,slot<NUMSTDANIMS?PS_RowDuration(row):2);
        int minb=10000,maxb=-10000,frames=0,t=0,last=-1,mirrors=0;float y=0,miny=0,maxy=0;
        for(;t<400;t++) {
            CHECK(!vm.failed && vm.frameid==vm.frame);
            y+=vm.movey;miny=MIN(y,miny);maxy=MAX(y,maxy);
            if(vm.step!=last) {
                patch_t *p=W_Frame(vm.sprite,vm.frame);
                CHECK(p && p->hasbox && p->maxval<=roster[c]->base->palettes[c*4].count);
                minb=MIN(minb,p->by1);maxb=MAX(maxb,p->by1);frames++;mirrors+=vm.hflip;last=vm.step;
            }
            if(vm.done)break;
            PS_Tick(&vm);
        }
        bool cyclic=slot==ANIM_STAND || slot==ANIM_WALKF || slot==ANIM_WALKB ||
            slot==ANIM_CROUCH || slot==ANIM_BLOCK || slot==ANIM_CROUCHBLOCK ||
            slot==ANIM_DIZZY || slot==ANIM_WIN ||
            (slot==NUMSTDANIMS+14 && (c==0 || c==1 || c==5 || c==6));
        CHECK(vm.done!=cyclic);
        if(report)fprintf(report,"%s,%d,%d,%d,%d,%d,%d,%d,%.0f,%.0f,%d\n",
            roster[c]->name,slot,row,t,frames,vm.done,minb,maxb,miny,maxy,mirrors);
    }
    if(report)fclose(report);
}
static void capture(const char *name)
{
    if(!capture_dir)return;
    char path[512];snprintf(path,sizeof path,"%s/%s.bmp",capture_dir,name);
    D_Display();I_SaveScreenshot(screen,path);
}

static void animation_landing(void)
{
    p_scripted=true;
    for(int c=0;c<numroster;c++) {
        if(!roster[c]->anims[ANIM_JUMP].count || !roster[c]->anims[ANIM_LAND].count)continue;
        G_InitNew(roster[c],roster[(c+1)%7],(bool[]){false,false},&stages[2]);
        level.phase=PH_FIGHT;
        fighter_t *f=&level.fighters[0],*o=&level.fighters[1];
        f->cmd.buttons=BT_UP;P_PlayerThink(f,o);
        int t=0;
        while(!P_InAnim(f,ANIM_LAND) && t++<180) {
            f->cmd.buttons=0;P_PlayerThink(f,o);P_FighterThinker(f);
        }
        CHECK(t<180 && f->z==0 && f->momz==0);
        int frame=f->state->frame;
        f->cmd.buttons=0;P_PlayerThink(f,o);
        CHECK(P_InAnim(f,ANIM_LAND) && f->state->frame==frame);
        int landingticks=0;
        while(P_InAnim(f,ANIM_LAND) && landingticks++<180) {
            P_PlayerThink(f,o);P_FighterThinker(f);
        }
        CHECK(landingticks>1 && landingticks<180 && P_InAnim(f,ANIM_STAND));
    }
}

static void animation_attack_recovery(void)
{
    p_scripted=true;
    for(int c=0;c<numroster;c++)for(int slot=0;slot<NUMMOVES;slot++) {
        if(!roster[c]->moves[slot].valid || !P_MoveIsAir(slot))continue;
        G_InitNew(roster[c],roster[(c+1)%7],(bool[]){false,false},&stages[2]);
        level.phase=PH_FIGHT;
        fighter_t *f=&level.fighters[0];
        CHECK(P_SetAnim(f,ANIM_JUMP));
        for(int t=0;t<8;t++)P_AnimTick(f);
        f->z=40;f->momz=0;
        CHECK(P_StartMove(f,slot,ANIM_JUMP));
        bool finished=false;
        for(int t=0;t<150;t++) {
            // Keep the body airborne to check the script's own recovery.
            P_AnimTick(f);
            if(f->anim.done) {finished=true;break;}
            if(P_InAnim(f,ANIM_JUMP)) {finished=true;break;}
        }
        // Some air moves explicitly loop until landing; finite moves must
        // resume the jump instead of holding the attack pose and hit boxes.
        if(finished) {
            char name[64];snprintf(name,sizeof name,"air-recovery-c%d-m%d",c,slot);capture(name);
            CHECK(P_InAnim(f,ANIM_JUMP) && !f->attack && !f->attacklive);
            if(c<7)NEAR(f->spritelift,f->def->airpivot);
        }
    }
}

static void animation_root_registration(void)
{
    p_scripted=true;
    for(int c=0;c<7;c++)for(int facing=-1;facing<=1;facing+=2)
    for(int slot=0;slot<NUMMOVES;slot++) {
        if(P_MoveIsAir(slot))continue;
        G_InitNew(roster[c],roster[(c+1)%7],(bool[]){false,false},&stages[2]);
        level.phase=PH_FIGHT;
        fighter_t *f=&level.fighters[0];f->facing=facing;
        psmove_t move;CHECK(PS_Move(c,slot,&move));
        psanim_t reference={0};PS_StartRow(&reference,c,move.row,2);
        float x=f->x-reference.movex*facing,lift=reference.movey;bool pictured=false;
        CHECK(P_StartMove(f,slot,ANIM_STAND));
        for(int t=0;t<150 && f->attack;t++) {
            if(!pictured && fabsf(lift)>.01f && facing==1) {
                char name[64];snprintf(name,sizeof name,"root-c%d-m%d",c,slot);capture(name);
                pictured=true;
            }
            // ROM art faces left: negative source X is forward. Movement,
            // drawing and collision must use the same signed pivot changes.
            NEAR(f->x,x);NEAR(f->spritelift,lift);
            CHECK(P_SpriteLift(f)==(int)lroundf(lift));
            P_AnimTick(f);PS_Tick(&reference);
            x-=reference.movex*facing;lift+=reference.movey;
        }
    }
}

static void animation_sequences(void)
{
    p_scripted=true;
    int cases=0;
    for(int c=0;c<numroster;c++)for(int facing=-1;facing<=1;facing+=2)
    for(int direction=-1;direction<=1;direction++)for(int slot=-1;slot<NUMMOVES;slot++) {
        if(slot>=0 && (!P_MoveIsAir(slot) || !roster[c]->moves[slot].valid))continue;
        for(int phase=0;phase<(slot<0?1:4);phase++) {
            G_InitNew(roster[c],roster[(c+1)%7],(bool[]){false,false},&stages[2]);
            level.phase=PH_FIGHT;
            fighter_t *f=&level.fighters[0],*o=&level.fighters[1];
            o->x=f->x+facing*160;f->facing=facing;
            f->cmd.buttons=BT_UP | (direction*facing>0?BT_RIGHT:direction*facing<0?BT_LEFT:0);
            P_PlayerThink(f,o);
            bool attacked=false,landed=false,apex=false,pictured=false;int t;
            const int starts[]={0,8,20,(int)(f->def->jumpspeed/GRAVITY*1.5f)};
            for(t=0;t<220;t++) {
                if(slot>=0 && t==starts[phase] && P_IsAirborne(f)) {
                    CHECK(P_StartMove(f,slot,ANIM_JUMP));attacked=true;
                }
                f->cmd.buttons=0;P_PlayerThink(f,o);P_FighterThinker(f);
                if(!pictured && slot>=0 && f->attacklive && facing==1 && direction==0 && phase==1) {
                    char name[64];snprintf(name,sizeof name,"attack-air-c%d-m%d",c,slot);capture(name);
                    pictured=true;
                }
                patch_t *p=P_StatePatch(f->state);box_t hurt;
                CHECK(p && p->hasbox && isfinite(f->x) && isfinite(f->z));
                CHECK(P_HurtBox(f,&hurt));
                if(c<7)CHECK(!f->anim.failed);
                if(c<7 && slot<0 && t==0)NEAR(f->z,0);
                if(c<7 && slot<0 && t==0)CHECK(f->state->frame==X_U16(PS_RowScript(c,ROW_JUMPMOVE)));
                if(f->state->kind==K_AIR) {
                    CHECK(p->by1-P_SpriteLift(f)-(int)lroundf(f->z)<=6);
                    CHECK(hurt.z0>=-6.5f);
                }
                bool firstapex=!apex && f->momz<0;
                bool firstland=!landed && f->z==0 && f->momz==0;
                if(slot<0 && facing==1 && direction==0 && (t==0 || firstapex || firstland)) {
                    char name[64];snprintf(name,sizeof name,"jump-c%d-%s",c,
                        firstland?"land":firstapex?"apex":"takeoff");capture(name);
                }
                apex|=firstapex;landed|=firstland;
                if(landed && P_InAnim(f,ANIM_STAND))break;
            }
            if(t>=220)printf("Sequence stalled: %s facing %d dir %d slot %d phase %d\n",
                f->def->name,facing,direction,slot,phase);
            CHECK(t<220 && apex && landed && !f->attack && !f->attacklive);
            if(slot>=0)CHECK(attacked);
            cases++;
        }
    }
    printf("Animation sequences: %d jump/attack/landing cases across %d fighters\n",cases,numroster);
}

static void animation_photo_states(void)
{
    p_scripted=true;int checked=0;
    for(int c=0;c<numroster;c++) {
        if(roster[c]->codeindex>=0)continue;
        for(int a=0;a<roster[c]->numanims;a++)for(int facing=-1;facing<=1;facing+=2) {
            if(!roster[c]->anims[a].count)continue;
            G_InitNew(roster[c],roster[0],(bool[]){false,false},&stages[2]);
            fighter_t *f=&level.fighters[0];f->facing=facing;
            CHECK(P_SetAnim(f,(animnum_t)a));
            if(f->state->kind==K_AIR)f->z=40;
            for(int t=0;t<400;t++) {
                const patch_t *p=P_StatePatch(f->state);box_t box;
                CHECK(p && p->hasbox && p->maxval<=f->pal->count);
                CHECK(P_HurtBox(f,&box) && isfinite(box.x0) && isfinite(box.z0));
                if(f->state->kind==K_AIR)CHECK(box.z0>=f->z);
                P_AnimTick(f);
            }
            checked++;
        }
    }
    printf("Photo animations: %d animation/facing cases checked\n",checked);
}

static void animation_ground_and_recovery(void)
{
    p_scripted=true;
    int cases=0;
    for(int c=0;c<numroster;c++)for(int facing=-1;facing<=1;facing+=2) {
        for(int slot=0;slot<NUMMOVES;slot++) {
            if(!roster[c]->moves[slot].valid || P_MoveIsAir(slot))continue;
            G_InitNew(roster[c],roster[(c+1)%7],(bool[]){false,false},&stages[2]);
            level.phase=PH_FIGHT;
            fighter_t *f=&level.fighters[0];f->facing=facing;
            CHECK(P_StartMove(f,slot,ANIM_STAND));
            int t=0;bool pictured=false;
            while(f->attack && t++<300) {
                if(!pictured && facing==1 && f->attacklive && (slot==1 || slot==7 || slot==11 || slot==17)) {
                    char name[64];snprintf(name,sizeof name,"attack-ground-c%d-m%d",c,slot);capture(name);
                    pictured=true;
                }
                CHECK(!P_IsAirborne(f) && P_StatePatch(f->state));
                if(c<7)CHECK(!f->anim.failed);
                P_FighterThinker(f);
            }
            CHECK(t<300 && P_InAnim(f,ANIM_STAND) && !f->attacklive);
            CHECK(P_SpriteLift(f)==0);
            cases++;
        }
        if(!roster[c]->anims[ANIM_FALL].count)continue;
        for(int air=0;air<2;air++) {
            G_InitNew(roster[c],roster[(c+1)%7],(bool[]){false,false},&stages[2]);
            level.phase=PH_FIGHT;
            fighter_t *f=&level.fighters[0];f->facing=facing;
            f->z=air?70:1;f->momz=3;
            CHECK(P_SetAnim(f,ANIM_FALL));
            bool down=false,up=false;int t,upticks=0;
            for(t=0;t<500;t++) {
                P_FighterThinker(f);
                CHECK(P_StatePatch(f->state));
                if(c<7)CHECK(!f->anim.failed);
                if(P_IsAirborne(f))CHECK(P_InAnim(f,ANIM_FALL));
                if(P_IsAirborne(f))CHECK(P_StatePatch(f->state)->by1-P_SpriteLift(f)-(int)lroundf(f->z)<=6);
                bool firstdown=!down && P_InAnim(f,ANIM_DOWN);
                bool firstup=!up && P_InAnim(f,ANIM_GETUP);
                if(c<7 && firstdown) {
                    CHECK(f->anim.done && f->state->frame==f->def->states[f->def->anims[ANIM_GETUP].first].frame);
                    CHECK(f->def->anims[ANIM_DOWN].count==1);
                }
                down|=firstdown;up|=firstup;
                if(P_InAnim(f,ANIM_GETUP))upticks++;
                if(facing==1 && air && (t==0 || firstdown || firstup || upticks==20)) {
                    char name[64];snprintf(name,sizeof name,"recovery-c%d-%s",c,
                        firstdown?"down":firstup?"up":upticks==20?"up-mid":"fall");capture(name);
                }
                if(P_InAnim(f,ANIM_STAND))break;
            }
            if(t>=500)printf("Recovery stalled: %s frame %d anim %d\n",f->def->name,f->state->frame,f->state->anim);
            CHECK(t<500 && down && up && f->z==0 && f->momz==0);
            CHECK(P_SpriteLift(f)==0 && !f->attack);
            cases++;
        }
    }
    printf("Animation sequences: %d ground attacks/recoveries across %d fighters\n",cases,numroster);
}

static void retail_walking(void)
{
    p_scripted=true;
    // Native 0x1a712 bounds both frame tables with this roster-wide count.
    const int counts[]={24,22,16,30,20,24,22};
    for(int c=0;c<7;c++)for(int back=0;back<2;back++)for(int facing=-1;facing<=1;facing+=2) {
        G_InitNew(roster[c],roster[(c+1)%7],(bool[]){false,false},&stages[2]);
        level.phase=PH_FIGHT;
        fighter_t *f=&level.fighters[0],*o=&level.fighters[1];
        int row=back?ROW_WALKB:ROW_WALKF;
        animnum_t anim=back?ANIM_WALKB:ANIM_WALKF;
        uint32_t frames=PS_RowScript(c,row);
        CHECK(X_U16(0x6affe + c*2)==counts[c]);
        int period=counts[c]*(int)PS_RowDuration(row);
        for(int t=0;t<period*3+8;t++) {
            f->x=ARENA_WIDTH/2; o->x=f->x+facing*120;
            level.camx=f->x-VIEWWIDTH/2;
            f->cmd.buttons=(facing*(back?-1:1)>0)?BT_RIGHT:BT_LEFT;
            P_PlayerThink(f,o);
            P_FighterThinker(f);
            CHECK((f->x-ARENA_WIDTH/2)*facing*(back?-1:1)>0);
            // Include the old fall-through boundary and a full Talon cycle.
            if(facing==1 && (t==0 || t==21 || t==period-3 || t==period || t==period+8 ||
                            (c==2 && back && t<period && t%3==0))) {
                char name[64];snprintf(name,sizeof name,"walk-c%d-d%d-t%03d",c,back,t);capture(name);
            }
            CHECK(P_InAnim(f,anim) && f->state->kind==K_FREE);
            int index=(t/(int)PS_RowDuration(row))%counts[c];
            CHECK(f->state->frame==(X_U16(frames+index*2)&0x7fff));
            CHECK(f->state->hflip==!!(X_U16(frames+index*2)&0x8000));
            CHECK(f->facing==facing && !P_IsAirborne(f));
            CHECK(P_SpriteLift(f)==0);
            box_t hurt; CHECK(P_HurtBox(f,&hurt));
            NEAR(hurt.z0,-P_StatePatch(f->state)->by1);
        }
        f->cmd.buttons=(facing*(back?1:-1)>0)?BT_RIGHT:BT_LEFT;
        P_PlayerThink(f,o);
        CHECK(P_InAnim(f,back?ANIM_WALKF:ANIM_WALKB));
        CHECK(f->state->frame==X_U16(PS_RowScript(c,back?ROW_WALKF:ROW_WALKB)));
        // Input changes must leave the cycle immediately, without a stale
        // walking table/pivot leaking into idle, crouch, jump or an attack.
        const int inputs[]={0,BT_DOWN,BT_UP,BT_HQ};
        const animnum_t wants[]={ANIM_STAND,ANIM_CROUCH,ANIM_JUMP,ANIM_MOVE};
        for(int i=0;i<4;i++) {
            f->z=f->momz=0; f->oldbuttons=0;P_SetAnim(f,anim);
            f->cmd.buttons=inputs[i];P_PlayerThink(f,o);
            CHECK(P_InAnim(f,wants[i]));
            CHECK(!f->anim.framecycle);
            CHECK(P_SpriteLift(f)==0);
        }
        CHECK(f->def->anims[anim].count==counts[c]);
        const animinfo_t *preview=&f->def->anims[anim];
        CHECK(f->def->states[preview->first+preview->count-1].next==preview->first);
    }
}

static void retail_air_registration(void)
{
    p_scripted=true;
    G_InitNew(roster[0],roster[2],(bool[]){false,false},&stages[2]);
    const int lifts[]={102,70,77,92,73,102,70};
    for(int c=0;c<7;c++)for(int back=0;back<2;back++) {
        fighter_t *f=&level.fighters[0];
        P_SpawnFighter(f,roster[c],0,ARENA_WIDTH/2,roster[c]->palette);
        P_SetAnim(f,back?ANIM_WALKB:ANIM_WALKF);
        for(int t=0;t<60;t++)P_AnimTick(f);
        f->momz=f->def->jumpspeed;
        CHECK(P_SetAnim(f,ANIM_JUMP));
        CHECK(!f->anim.framecycle && P_SpriteLift(f)==0);
        int airticks=0;
        for(;f->z>=0;f->z+=f->momz,f->momz-=GRAVITY) {
            P_AnimTick(f);
            int lift=P_SpriteLift(f);
            if(f->z>=30) {
                CHECK(c==4?lift>=lifts[c]:lift==lifts[c]);
                airticks++;
            }
            CHECK(P_StatePatch(f->state)->by1-lift-f->z<=7);
        }
        CHECK(airticks>0);
        f->z=f->momz=0;P_SetAnim(f,ANIM_STAND);
        CHECK(P_SpriteLift(f)==0 && f->spritelift==0);
    }
}
static void readme_screenshots(void)
{
    CHECK(capture_dir);
    p_scripted=true;options.sound=false;options.difficulty=2;options.seconds=60;
    gamemode=GM_ARCADE;
    SEL_Reset();SEL_Start();
    event_t join={ev_keydown,'o'},right={ev_keydown,KEY_RIGHTARROW};
    CHECK(SEL_Responder(&join));CHECK(SEL_Responder(&right));
    for(int t=0;t<120;t++)SEL_Ticker();
    capture("fighter-select");
    const struct {int a,b,stage;const char *name;} scenes[]={
        {0,2,0,"sauron-talon-cove"},
        {1,6,1,"blizzard-chaos-inferno"},
        {3,4,3,"vertigo-armadon-tomb"},
        {5,0,5,"diablo-sauron-ruins"},
        {7,11,6,"boxer-lizard-hollows"},
    };
    for(size_t i=0;i<sizeof scenes/sizeof scenes[0];i++) {
        CHECK(scenes[i].b<numroster);
        G_InitNew(roster[scenes[i].a],roster[scenes[i].b],(bool[]){true,true},&stages[scenes[i].stage]);
        level.phase=PH_FIGHT;
        for(int t=0;t<=260;t++) {
            G_Ticker();
            if(t==80 || t==140 || t==200 || t==260) {
                char name[96];snprintf(name,sizeof name,"%s-%03d",scenes[i].name,t);capture(name);
            }
        }
    }
}

static void retail_combat(void)
{
    p_scripted=true;
    CHECK(P_CheckAnims()==0);
    bool cpu[2]={false,false};
    for(int c=0;c<7;c++) {
        G_InitNew(roster[c],roster[(c+1)%7],cpu,&stages[2]);level.phase=PH_FIGHT;
        fighter_t *f=&level.fighters[0],*o=&level.fighters[1];
        for(int posture=0;posture<4;posture++)for(int low=0;low<2;low++) {
            f->oldbuttons=0;f->airattackused=false;f->z=posture==3?20:0;
            f->x=ARENA_WIDTH/2-60;f->momx=f->momz=0;
            o->x=f->x+f->def->reach+o->def->reach+(posture==1?0:45);
            level.camx=(f->x+o->x)/2-VIEWWIDTH/2;
            P_SetAnim(f,posture==3?ANIM_JUMP:ANIM_STAND);
            f->cmd.buttons=(low?BT_LQ|BT_LF:BT_HQ|BT_HF)|(posture==2?BT_DOWN:0);
            P_PlayerThink(f,o);
            CHECK(f->move==MP_STAND+posture*2+low);
            CHECK((f->state->kind==K_AIR)==(posture==3));
            if(capture_dir) {
                for(int t=0;t<6;t++) { P_FighterThinker(f);P_SeparateFighters(f,o); }
                char name[48];snprintf(name,sizeof name,"power-c%d-p%d-l%d",c,posture,low);capture(name);
            }
        }
        psmove_t move;CHECK(PS_Move(c,11,&move));CHECK(move.hitclass==4 || move.hitclass==5);
        // Exercise CPU air attacks explicitly; a short sampled fight need
        // not happen to contain one after animation/recovery timings change.
        P_SpawnFighter(f,roster[c],0,ARENA_WIDTH/2,roster[c]->palette);
        CHECK(P_SetAnim(f,ANIM_JUMP));
        for(int t=0;t<8;t++)P_AnimTick(f);
        f->z=20;f->momz=-1;o->x=f->x+f->def->reach+o->def->reach+5;
        P_CpuCmd(f,o,&f->cmd);P_PlayerThink(f,o);
        CHECK(f->attack && f->airattackused && P_MoveIsAir(f->move));
    }
    cpu[0]=cpu[1]=true;options.difficulty=2;
    G_InitNew(roster[0],roster[2],cpu,&stages[0]);
    int attacks=0,air=0,powers=0;
    for(int t=0;t<420;t++) {
        G_Ticker();
        if(t==60)CHECK(level.phase==PH_INTRO && level.phasetime==61);
        for(int p=0;p<2;p++) {
            fighter_t *f=&level.fighters[p];
            attacks+=f->attack!=NULL;air+=f->attack && P_IsAirborne(f);powers+=f->move>=MP_STAND;
        }
        if(t%6==0) { char name[48];snprintf(name,sizeof name,"fight-%05d",t);capture(name); }
    }
    CHECK(attacks>0);
    CHECK(level.fighters[0].health<MAX_HEALTH || level.fighters[1].health<MAX_HEALTH);
    printf("CPU reel: %d attacking ticks, %d air ticks, %d power ticks\n",attacks,air,powers);
    // Each style must engage and finish a round, rather than stall at its range.
    for(int c=0;c<7;c++) {
        G_InitNew(roster[c],roster[(c+2)%7],cpu,&stages[0]);
        level.phase=PH_FIGHT;
        for(int t=0;t<3000 && level.phase==PH_FIGHT;t++)G_Ticker();
        CHECK(level.phase!=PH_FIGHT);
        CHECK(level.fighters[0].health<MAX_HEALTH || level.fighters[1].health<MAX_HEALTH);
    }
    // Real overlapping boxes: standing guard loses to a low, down-away blocks.
    G_InitNew(roster[0],roster[2],(bool[]){false,false},&stages[0]);level.phase=PH_FIGHT;
    fighter_t *a=&level.fighters[0],*b=&level.fighters[1];
    int landed=0;
    for(int pass=0;pass<2;pass++) {
        P_SetAnim(b,ANIM_STAND);a->x=480;b->x=570;b->oldbuttons=0;
        b->cmd.buttons=BT_RIGHT|(pass?BT_DOWN:0);P_PlayerThink(b,a);
        CHECK(P_StartMove(a,11,ANIM_CROUCH));
        for(int t=0;t<80 && a->attack;t++) {
            P_AnimTick(a);hit_t h;
            if(P_AttackLands(a,b,&h)) { CHECK(h.blocked==(pass!=0));landed++;break; }
        }
    }
    CHECK(landed==2);
    // Existing authored power/dizzy rows in the game; no new artwork.
    G_InitNew(roster[0],roster[2],(bool[]){false,false},&stages[0]);level.phase=PH_FIGHT;
    a=&level.fighters[0];b=&level.fighters[1];a->x=480;b->x=600;
    P_StartMove(a,MP_CROUCH,ANIM_CROUCH);for(int t=0;t<8;t++)P_AnimTick(a);capture("power");
    P_SetAnim(a,ANIM_STAND);b->brain=MAX_BRAIN;b->dizzypending=true;
    P_PlayerThink(b,a);CHECK(b->dizzy);
    for(int t=0;t<16;t++)P_FighterThinker(b);
    capture("dizzy");
    for(int c=7;c<numroster;c++) {
        G_InitNew(roster[c],roster[2],(bool[]){false,false},&stages[0]);level.phase=PH_FIGHT;
        a=&level.fighters[0];b=&level.fighters[1];a->brain=MAX_BRAIN;a->dizzypending=true;
        P_PlayerThink(a,b);CHECK(a->dizzy && !a->blocking && a->move<0);
        for(int t=0;t<8;t++)P_FighterThinker(a);
        if(c==7)capture("clay-dizzy");
    }
}

int main(int argc,char **argv)
{
    myargc=argc;myargv=argv;
    char **testargv=I_Malloc((argc+1)*sizeof *testargv);
    int testargc=1;testargv[0]=argv[0];
    for(int i=1;i<argc;i++) {
        if(!strcmp(argv[i],"--captures") && i+1<argc) { capture_dir=argv[++i];continue; }
        if(!strcmp(argv[i],"--retail")) {
            W_Init("data/primrage");PS_Init();V_Init();
            P_LoadRoster("data/creatures");
            test_register("animation_retail_scripts",animation_scripts);
            test_register("animation_landing",animation_landing);
            test_register("animation_attack_recovery",animation_attack_recovery);
            test_register("animation_root_registration",animation_root_registration);
            test_register("animation_sequences",animation_sequences);
            test_register("animation_photo_states",animation_photo_states);
            test_register("animation_ground_recovery",animation_ground_and_recovery);
            test_register("animation_retail_walking",retail_walking);
            test_register("animation_retail_air",retail_air_registration);
            test_register("combat_retail",retail_combat);
        } else testargv[testargc++]=argv[i];
    }
    if(capture_dir && PS_Active())test_register("readme_screenshots",readme_screenshots);
    int result=test_main(testargc,testargv);
    free(testargv);I_Shutdown();return result;
}
