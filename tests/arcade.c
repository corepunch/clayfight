// One process covers the arcade migration: rules, resources and real captures.
#include <assert.h>
#include <stdio.h>
#include <time.h>
#include <sys/stat.h>
#include <string.h>
#include <stdlib.h>
#include "game/game.h"
void CF_FlowChecks(void);void CF_RetailChecks(void);
static void shot(const char *name)
{
    char path[512];snprintf(path,sizeof path,"build/arcade-review/%s.bmp",name);
    D_Display();I_SaveScreenshot(screen,path);
}
static void hud_checks(void)
{
    gra_t *g=W_LoadGra("hud");
    for(int i=0;i<HUD_HEART;i++){
        const patch_t *p=W_Frame(g,i);
        assert(p && p->h==16 && p->w==(i<HUD_STUN_LEFT?120:80));
        assert(p->maxval<=15 && p->hasbox);
    }
    assert(W_Frame(g,HUD_HEART)==W_Frame(W_LoadGra("arcade"),1767));
    assert(W_Frame(W_LoadGra("stone-font"),'5')->h==16);
    assert(V_TextWidth(FONT_BIG,"57")>V_TextWidth(FONT_SMALL,"57"));
    bool cpu[2]={false,false};gamemode=GM_ARCADE;
    G_InitNew(roster[5],roster[2],cpu,&stages[2]);
    level.phase=PH_FIGHT;level.leveltime=12;level.time=57*TICRATE;shot("hud-full");
    // The original heart's opaque texels must be presented at exact 2x,
    // including its negative hotspot and the mirrored right-player artwork.
    const patch_t *heart=W_Frame(g,HUD_HEART);
    const palette_t *pal=&g->palettes[2];
    for(int side=0;side<2;side++)for(int y=0;y<heart->h;y++)for(int x=0;x<heart->w;x++){
        int value=heart->pixels[y*heart->w+x],sy=y-heart->y;
        int sx=side?332+heart->x-1-x:4-heart->x+x;
        if(!value || sx<0 || sx>=VIEWWIDTH || sy<0 || sy>=VIEWHEIGHT)continue;
        for(int dy=0;dy<2;dy++)for(int dx=0;dx<2;dx++)
            assert(screen[(sy*2+dy)*SCREENWIDTH+sx*2+dx]==pal->colors[value]);
    }
    level.fighters[0].health=37;level.fighters[0].shownhealth=46;level.fighters[0].flash=8;
    level.fighters[0].brain=32;level.fighters[1].health=level.fighters[1].shownhealth=68;
    level.fighters[1].brain=70;shot("hud-damaged");
    level.fighters[0].health=0;level.fighters[0].shownhealth=0;level.fighters[0].flash=0;
    level.fighters[0].dead=true;level.phase=PH_KO;level.phasetime=40;level.time=0;shot("hud-empty");
    gamemode=GM_TRAINING;G_InitNew(roster[5],roster[2],cpu,&stages[2]);
    level.phase=PH_FIGHT;shot("hud-training");
    gamemode=GM_TUG;G_InitNew(roster[5],roster[2],cpu,&stages[2]);
    level.phase=PH_FIGHT;shot("hud-tug");
    gamemode=GM_ARCADE;G_InitNew(roster[7],roster[2],cpu,&stages[2]);
    char saved[sizeof roster[7]->name];memcpy(saved,roster[7]->name,sizeof saved);
    snprintf(roster[7]->name,sizeof roster[7]->name,"ABCDEFGHIJKLMNOP");
    match.wins[0]=4;match.wins[1]=3;level.phase=PH_FIGHT;shot("hud-custom");
    memcpy(roster[7]->name,saved,sizeof saved);
    puts("arcade HUD: authored bar states, mirrored 2x heart, stone digits and custom-name capture passed");
}
static void font_checks(void)
{
    byte *chip=I_Malloc(0x20000);
    FILE *rom=fopen("data/primrage/136102-1045b.23p","rb");assert(rom);
    assert(fread(chip,1,0x20000,rom)==0x20000);fclose(rom);
    const char *text="VERSION 2.3";int pen=16;
    const pixel_t bg=0xff172131;V_Clear(bg);V_DrawText(FONT_BIG,pen,16,text);
    for(const char *c=text;*c;c++){
        int glyph=X_U8(0xc510+*c);uint32_t entry=0xc370+glyph*8;
        int width=X_U16(entry+4)?16:8;
        for(int y=0;y<16;y++)for(int x=0;x<width;x++){
            int tile=X_U16(entry+2*(x/8*2+y/8))+(glyph>=50?0:128);
            int packed=chip[(tile&4095)*32+y%8*4+x%8/2];
            int value=(packed>>(x%2?0:4))&15,rgb=X_U16(0x61442+value*2);
            pixel_t expected=value?W_RGB((rgb>>10&31)*255/31,(rgb>>5&31)*255/31,(rgb&31)*255/31):bg;
            for(int dy=0;dy<2;dy++)for(int dx=0;dx<2;dx++)
                assert(screen[((16+y)*2+dy)*SCREENWIDTH+(pen+x)*2+dx]==expected);
        }
        pen+=width;
    }
    assert(V_TextWidth(FONT_BIG,text)==pen-16); // native 8/16-pixel advances, including I and space
    V_Clear(bg);V_DrawText(FONT_SMALL,16,16,"A");
    const patch_t *small=W_Frame(W_LoadGra("fonts"),'A');
    for(int y=0;y<8;y++)for(int x=0;x<small->w;x++){
        int packed=chip['A'*32+y*4+x/2],value=(packed>>(x%2?0:4))&15;
        int rgb=X_U16(0x61802+value*2);
        pixel_t expected=value?W_RGB((rgb>>10&31)*255/31,(rgb>>5&31)*255/31,(rgb&31)*255/31):bg;
        assert(screen[(16+y)*2*SCREENWIDTH+(16+x)*2]==expected);
    }
    assert(W_LoadGra("fonts")->palettes[0].colors[1]>W_LoadGra("fonts")->palettes[0].colors[15]);
    assert(V_TextWidth(FONT_TIMER,"57")>V_TextWidth(FONT_BIG,"57"));
    free(chip);
    puts("arcade fonts: intro alpha glyphs, column-major tile assembly, native cell spacing, exact 2x RGB and descending compact-font bevel passed");
}
static void title_palette_checks(void)
{
    gra_t *g=W_LoadGra("title");
    const uint32_t descriptors[]={0x7422c,0x7423c,0x7425c};
    const int slots[]={45,13,19},origins[][2]={{0,0},{14,14},{15,19}};
    uint32_t base=X_U32(0x35bc8);assert(base==0x650e2);
    for(int i=0;i<20;i++){
        const patch_t *p=W_Frame(g,i);
        assert(p && p->maxval<=g->palettes[W_FramePalette(g,i)].count);
    }
    M_StartTitle();for(int t=0;t<120;t++)M_Ticker();D_Display();
    for(int layer=0;layer<3;layer++){
        const patch_t *p=W_Frame(g,slots[layer]);
        int color=X_U16(descriptors[layer]+6);
        for(int value=0;value<64;value++){
            int rgb=X_U16(base+color*2+value*2);
            pixel_t expected=W_RGB((rgb>>10&31)*255/31,(rgb>>5&31)*255/31,(rgb&31)*255/31);
            assert(g->palettes[layer].colors[value]==expected);
        }
        if(!layer)continue; // lettering correctly occludes the original plaque
        for(int y=0;y<p->h;y++)for(int x=0;x<p->w;x++){
            int value=p->pixels[y*p->w+x];if(!value)continue;
            int rgb=X_U16(base+color*2+value*2);
            pixel_t expected=W_RGB((rgb>>10&31)*255/31,(rgb>>5&31)*255/31,(rgb&31)*255/31);
            int sx=origins[layer][0]+x-p->x,sy=origins[layer][1]+y-p->y;
            for(int dy=0;dy<2;dy++)for(int dx=0;dx<2;dx++)
                assert(screen[(sy*2+dy)*SCREENWIDTH+sx*2+dx]==expected);
        }
    }
    puts("arcade title: descriptor palettes and every visible PRIMAL/RAGE texel match the original RGB555 colours at exact 2x");
}
static pixel_t source_rgb(uint32_t address)
{
    int rgb=X_U16(address);
    return W_RGB((rgb>>10&31)*255/31,(rgb>>5&31)*255/31,(rgb&31)*255/31);
}
static void palette_reference(const palette_t *pal,uint32_t address,int count)
{
    assert(pal && pal->count==count-1);
    for(int i=0;i<count;i++)assert(pal->colors[i]==source_rgb(address+i*2));
}
static void art_palette_reference(gra_t *g,int frame,uint32_t address,int count)
{
    const patch_t *p=W_Frame(g,frame);assert(p);
    const palette_t *pal=&g->palettes[W_FramePalette(g,frame)];
    palette_reference(pal,address,count);assert(p->maxval<count);
    // Exercise the selected frame's actual palette through the shared draw
    // path, including zero-index transparency and every visible texel at 2x.
    const pixel_t bg=0xff172131;V_Clear(bg);V_DrawPatch(p->x,p->y,p,pal,0);
    for(int y=0;y<MIN(p->h,VIEWHEIGHT);y++)for(int x=0;x<MIN(p->w,VIEWWIDTH);x++){
        int value=p->pixels[y*p->w+x];pixel_t expected=value?source_rgb(address+value*2):bg;
        for(int dy=0;dy<2;dy++)for(int dx=0;dx<2;dx++)
            assert(screen[(y*2+dy)*SCREENWIDTH+x*2+dx]==expected);
    }
}
static void palette_checks(void)
{
    gra_t *body=W_LoadGra("arcade"),*select=W_LoadGra("select"),*hud=W_LoadGra("hud"),*globe=W_LoadGra("globe");
    for(int c=0;c<7;c++){
        for(int alt=0;alt<4;alt++)palette_reference(&body->palettes[c*4+alt],X_U32(0x6e38c+c*4)+alt*128,64);
        for(int player=0;player<2;player++)palette_reference(W_ArcadeTribePalette(c,player),X_U32(0x295da)+c*128+player*0x380,64);
        art_palette_reference(hud,HUD_NAME+c,X_U32(0x351f6)+c*128,64);
    }
    const int portraits[]={130,91,141,149,125,113,97},heads[]={140,139,148,147,103,102,111};
    uint32_t temple=X_U32(0x183b0);
    for(int c=0;c<7;c++){
        for(int k=0;k<5;k++)art_palette_reference(select,portraits[c]+k,X_U32(0x676fa+c*4),64);
        assert(W_Frame(select,heads[c])==W_Frame(body,X_U32(0x6ad00+c*4)));
        art_palette_reference(select,heads[c],temple+X_U16(0x6ad1c+c*2)*2,64);
    }
    art_palette_reference(select,136,temple+X_U16(0x6ac2a)*2,128);
    for(int i=0;i<2;i++)art_palette_reference(select,105+i,temple+X_U16(0x6ac4a)*2,64);
    for(int i=0;i<47;i++){
        int layer=i<20?0:1+(i-20)/13;
        art_palette_reference(globe,i,X_U32(0x36ca4)+X_U16(0x74174+layer*16+6)*2,64);
    }
    palette_reference(&hud->palettes[0],0x613c2+6*32,16);
    palette_reference(&hud->palettes[1],0x613c2+7*32,16);
    for(int i=HUD_HEART;i<HUD_NAME;i++)art_palette_reference(hud,i,X_U32(0x2952e)+X_U16(0x6d3d6)*2,32);
    for(int c='0';c<='9';c++)art_palette_reference(W_LoadGra("stone-font"),c,X_U32(0x44700),16);
    art_palette_reference(W_LoadGra("slabs"),37,0x61042,64);
    for(int stage=0;stage<8;stage++){
        const int colors[]={0x100,0x180,0x200,0x240,0x280,0x2c0,0x300,0x340,0x380,0x5a0};
        for(unsigned i=0;i<sizeof colors/sizeof *colors;i++){
            int color=colors[i];uint32_t source=color<0x200?X_U32(0x2952e)+color*2:
                color<0x400?X_U32(0x614e2+stage*4)+(color-0x200)*2:X_U32(0x2956a)+(color-0x480)*2;
            palette_reference(W_ArcadeStagePalette(stage,color),source,64);
        }
    }
    palette_reference(W_ArcadeImpactPalette(false),X_U32(0x2952e)+X_U16(0x6cf56)*2,64);
    palette_reference(W_ArcadeImpactPalette(true),X_U32(0x2952e)+0x180*2,64);
    // Review all seven selected heads in their real screen, in source order.
    SEL_Reset();gamemode=GM_TUG;SEL_Start();
    for(int cell=0;cell<7;cell++){
        char name[48];snprintf(name,sizeof name,"palette-select-%d",cell);shot(name);
        event_t right={ev_keydown,'d'};assert(SEL_Responder(&right));
    }
    puts("arcade palettes: 28 fighter variants, 14 team banks, all portraits/heads, temple/podiums, globe layers, HUD, timer, shadows, contacts and eight scenery banks match source RGB555; exact 2x art texels passed");
}
static void scenery_checks(void)
{
    // Independently decoded source samples at native (168,100)/(168,230),
    // centred camera and time zero. Catch wrong banks, floor halves and crop.
    const pixel_t pixels[8][2]={
        {0xff731041,0xff6a3900},
        {0xffc54a10,0xff621810},
        {0xffc5e6ee,0xff626a5a},
        {0xffcd5252,0xff003900},
        {0xffb4bdcd,0xff8394c5},
        {0xffc5bdb4,0xff52524a},
        {0xff94948b,0xff4a3110},
        {0xff182039,0xff524139}
    };
    pixel_t *saved=I_Malloc(sizeof screen);bool cpu[2]={false,false};
    int middle=(ARENA_WIDTH-VIEWWIDTH)/2;
    assert(ARENA_WIDTH==1080); // native camera bounds: signed 26.6 value 5d00
    for(int kind=0;kind<2;kind++)for(int i=0;i<10;i++){
        const patch_t *p=W_Frame(W_LoadGra("arcade"),(kind?33:62)+i);
        assert(p->maxval<=W_ArcadeImpactPalette(kind!=0)->count);
    }
    assert(W_ArcadeImpactPalette(true)->colors[51]==W_RGB(0,0,255));
    M_ClearRandom();int expected_random=P_Random();M_ClearRandom();
    for(int stage=0;stage<8;stage++){
        G_InitNew(roster[stage%7],roster[(stage+5)%7],cpu,&stages[stage]);
        level.leveltime=0;W_DrawArcadeStage(stage,middle);
        assert(screen[100*2*SCREENWIDTH+168*2]==pixels[stage][0]);
        assert(screen[230*2*SCREENWIDTH+168*2]==pixels[stage][1]);
        for(int y=0;y<SCREENHEIGHT;y+=2)for(int x=0;x<SCREENWIDTH;x+=2){
            int at=y*SCREENWIDTH+x;
            assert(screen[at]==screen[at+1] && screen[at]==screen[at+SCREENWIDTH] && screen[at]==screen[at+SCREENWIDTH+1]);
        }
        level.leveltime=120;
        for(int cam=0;cam<=ARENA_WIDTH-VIEWWIDTH;cam+=middle){
            R_DrawStage(level.stage,cam);R_DrawStageForeground(level.stage,cam);
            memcpy(saved,screen,sizeof screen);
            R_DrawStage(level.stage,cam);R_DrawStageForeground(level.stage,cam);
            assert(!memcmp(saved,screen,sizeof screen));
        }
    }
    assert(P_Random()==expected_random); // rendering never consumes combat RNG
    G_InitNew(roster[0],roster[5],cpu,&stages[7]);level.leveltime=120;
    R_DrawStage(level.stage,middle);R_DrawStageForeground(level.stage,middle);memcpy(saved,screen,sizeof screen);
    level.leveltime=0;R_DrawStage(level.stage,middle);R_DrawStageForeground(level.stage,middle);
    level.leveltime=120;R_DrawStage(level.stage,middle);R_DrawStageForeground(level.stage,middle);
    assert(!memcmp(saved,screen,sizeof screen)); // deterministic rewind/round reset
    level.leveltime=0;W_DrawArcadeStage(1,middle);memcpy(saved,screen,sizeof screen);
    level.leveltime=20;W_DrawArcadeStage(1,middle);assert(memcmp(saved,screen,sizeof screen));
    free(saved);
    assert(W_ArcadeShadow(0,3818)==W_Frame(W_LoadGra("arcade"),7152));
    assert(W_ArcadeShadow(1,5126)==W_Frame(W_LoadGra("arcade"),10578));
    for(int c=0;c<7;c++){
        const state_t *stand=&roster[c]->states[roster[c]->anims[ANIM_STAND].first];
        assert(W_ArcadeShadow(c,stand->frame));
        assert(C_Start(roster[0]));campaign.order[0]=c;C_StartFight();
        assert(level.stage==&stages[X_U8(0x69a40+c)]);C_Abandon();
    }
    G_InitNew(roster[5],roster[2],cpu,&stages[2]);level.phase=PH_FIGHT;
    P_SpawnSpark(middle+158,90,SPARK_HIT);P_SpawnSpark(middle+196,90,SPARK_BLOCK);
    assert(level.particles[0].animation.frameid==62 && level.particles[1].animation.frameid==33);
    for(int t=0;t<9;t++)P_RunParticles();shot("impacts");
    for(int t=0;t<40;t++)P_RunParticles();assert(level.numparticles==0);
    const int clips[]={0,1,2,7};
    for(int c=0;c<4;c++){
        G_InitNew(roster[0],roster[5],cpu,&stages[clips[c]]);level.phase=PH_FIGHT;
        for(int t=0;t<12;t++){
            level.leveltime=60+t*4;level.camx=middle-20+t*4;
            char name[64];snprintf(name,sizeof name,"scene-reel-%03d",c*12+t);shot(name);
        }
    }
    G_InitNew(roster[0],roster[5],cpu,&stages[0]);level.phase=PH_FIGHT;
    level.camx=0;level.fighters[0].x=98;level.fighters[1].x=238;shot("stage-left");
    level.camx=ARENA_WIDTH-VIEWWIDTH;level.fighters[0].x=level.camx+98;level.fighters[1].x=level.camx+238;shot("stage-right");
    M_StartTitle();
    for(int t=0;t<=110;t++){
        M_Ticker();
        if(t>=32 && (t-32)%6==0){char name[64];snprintf(name,sizeof name,"title-reel-%03d",t);shot(name);}
    }
    puts("arcade scenery: eight source banks/crops, both floor halves, 2x pixels, camera extremes, deterministic animations, shadows, impacts and campaign mapping passed");
}
static void capture_checks(void)
{
    M_StartTitle();for(int i=0;i<120;i++)M_Ticker();shot("title");
    M_StartMenu();shot("menu");M_OpenStart();shot("start");M_OpenOptions();shot("options");
    gamemode=GM_TUG;SEL_Start();shot("select");
    event_t tab={ev_keydown,'\t'};SEL_Responder(&tab);shot("select-clay");
    gamemode=GM_ARCADE;
    bool cpu[2]={false,false};
    for(int i=0;i<8;i++){
        G_InitNew(roster[i%7],roster[(i+5)%7],cpu,&stages[i]);
        level.phase=PH_FIGHT;char name[32];snprintf(name,sizeof name,"stage-%d",i);shot(name);
    }
    cpu[0]=cpu[1]=true;
    G_InitNew(roster[0],roster[5],cpu,&stages[2]);
    for(int t=0;t<180;t++){
        G_Ticker();
        if(t%6==0){char name[64];snprintf(name,sizeof name,"reel-%03d",t);shot(name);}
    }
    assert(C_Start(roster[0]));shot("campaign");C_StartFight();
    campaign.active=true;gamestate=GS_CONTINUE;shot("continue");
    gamestate=GS_ENDING;shot("ending");C_Abandon();
    G_InitNew(roster[7],roster[2],cpu,&stages[2]);shot("clay-fight");
}
int main(int argc,char **argv)
{
    clock_t start=clock();myargc=argc;myargv=argv;
    mkdir("build",0777);mkdir("build/arcade-review",0777);
    I_InitGraphics(true);W_Init("data/primrage");assert(PS_Init());V_Init();P_LoadRoster("data/creatures");
    options.sound=false;S_Init();assert(SCREENWIDTH==672 && SCREENHEIGHT==480);assert(numroster>=12);
    // These commands cover effects plus all seven character voice tables.
    for(int c=0;c<7;c++)for(int k=0;k<3;k++){
        const unsigned table[]={0x6b0b0,0x6e3e8,0x69c64};
        const sfxdata_t *sound=W_ArcadeSound(X_U16(table[k]+c*2));
        assert(sound && sound->length>100 && sound->rate==11025);
        int lo=255,hi=0;
        for(int i=0;i<sound->length;i++){lo=MIN(lo,sound->data[i]);hi=MAX(hi,sound->data[i]);}
        assert(hi-lo>4);
    }
    puts("arcade sound: original DSP effects and all seven voice tables decoded");
    assert(P_CheckAnims()==0);
    CF_FlowChecks();CF_RetailChecks();capture_checks();hud_checks();scenery_checks();title_palette_checks();font_checks();palette_checks();I_Shutdown();
    printf("arcade: all checks passed, CPU %.3f seconds\n",(double)(clock()-start)/CLOCKS_PER_SEC);return 0;
}
