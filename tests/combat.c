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
            i == ANIM_JUMP ? K_AIR : i >= ANIM_FALL && i <= ANIM_GETUP ? K_DOWN :
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

static const char *capture_dir;
static void capture(const char *name)
{
    if(!capture_dir)return;
    char path[512];snprintf(path,sizeof path,"%s/%s.bmp",capture_dir,name);
    D_Display();I_SaveScreenshot(screen,path);
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
    CHECK(attacks>0 && air>0);
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
            test_register("combat_retail",retail_combat);
        } else testargv[testargc++]=argv[i];
    }
    int result=test_main(testargc,testargv);
    free(testargv);I_Shutdown();return result;
}
