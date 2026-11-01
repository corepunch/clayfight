// Arcade scenery descriptors, linked objects and original animation scripts.
#include "game/game.h"
stage_t stages[] = {
    {.name="THE COVE"}, {.name="THE INFERNO"}, {.name="THE STRIP"},
    {.name="THE TOMB"}, {.name="THE CLIFF"}, {.name="THE RUINS"},
    {.name="THE HOLLOWS"}, {.name="DINOSAUR GRAVEYARD"},
};
int numstages=sizeof stages/sizeof stages[0];
int R_ArenaWidth(void) {return VIEWWIDTH+2*(X_U16(0x6ba36)/64); }

typedef struct {
    uint32_t descriptor,script;
    int x,z,flags,bias,duration,frame,lasttic;
    float scale;
    const palette_t *palette;
    psanim_t animation;
} scenery_t;
static struct {int count;scenery_t objects[64];} scenes[8];

static void Scenery(int stage,uint32_t descriptor,int x,int z,int inherited,int depth)
{
    if(depth>8 || scenes[stage].count==64 || !X_Valid(descriptor,16))
        I_Error("Invalid arcade scenery descriptor %#x",descriptor);
    scenery_t *p=&scenes[stage].objects[scenes[stage].count++];
    p->descriptor=descriptor;p->x=x;p->z=z;
    int flags=X_U16(descriptor+8);
    p->flags=((flags^inherited)&0x4000)?V_FLIP:0;
    p->duration=X_U8(descriptor+5);
    p->palette=W_ArcadeStagePalette(stage,X_U16(descriptor+6));
    p->scale=X_U16(descriptor+12)/4096.f;
    if(p->scale<=0 || p->scale>1)I_Error("Invalid scenery scale at %#x",descriptor);
    if(flags&0x800){p->frame=X_U32(descriptor);return;}
    p->script=X_U32(descriptor);
    if(p->duration)PS_StartScenery(&p->animation,p->script,p->duration,(descriptor^(uint32_t)x^(uint32_t)z)|1);
    uint32_t ip=p->script;
    // Expand only the script's initial operations. Duration-zero objects hold
    // their first frame forever; continuing would run into the next script.
    for(int n=0;n<64;n++){
        int word=X_U16(ip);ip+=2;
        if(!(word&0x8000)){p->frame=word;return;}
        if(word==0xcc01 || word==0xcc00){
            uint32_t child=X_U32(ip);int dx=X_S16(ip+4),dy=X_S16(ip+6);ip+=8;
            Scenery(stage,child,x+dx*(word==0xcc01?64:1),z-dy*(word==0xcc01?64:1),flags^inherited,depth+1);
        }else if((word&0xff00)==0x9300)p->bias=(int8_t)word;
        else if((word&0x9f00)==0x8d00){
            if(!p->duration)I_Error("Animated scenery has zero duration at %#x",descriptor);
            p->frame=p->animation.frameid;return;
        }else I_Error("Unsupported arcade scenery operation %#x at %#x",word,ip-2);
    }
    I_Error("Runaway arcade scenery script %#x",p->script);
}
void R_PrepareStage(stage_t *st)
{
    if(st->prepared)return;
    int stage=(int)(st-stages);
    uint32_t entry=X_U32(0x6c418+stage*4);
    for(int n=0;X_U32(entry);n++,entry+=12){
        if(n>=64 || !X_Valid(entry,12))I_Error("Invalid arcade stage object table");
        Scenery(stage,X_U32(entry),(int32_t)X_U32(entry+4),X_S16(entry+8),0,0);
    }
    // A second original table supplies water, lava, bones and drifting fire.
    entry=X_U32(0xf2648+stage*4);int count=X_U16(entry);entry+=2;
    if(count>128)I_Error("Invalid arcade extra scenery count");
    for(int i=0;i<count;i++,entry+=12){
        int kind=X_U8(entry+10);
        Scenery(stage,X_U32(0x6d724+kind*12),(int32_t)X_U32(entry),X_S16(entry+6),X_U16(entry+8),0);
    }
    st->prepared=true;
}
static void Props(stage_t *st,int camx,bool foreground)
{
    R_PrepareStage(st);int stage=(int)(st-stages);
    // The source object list sorts by foot depth, including the script's
    // priority bias. Draw far scenery first and near scenery over the fighters.
    int order[64],count=scenes[stage].count;
    for(int i=0;i<count;i++){
        order[i]=i;
        for(int j=i;j>0;j--){
            scenery_t *a=&scenes[stage].objects[order[j-1]],*b=&scenes[stage].objects[order[j]];
            int da=240-MAX(0,a->z/64)+a->bias,db=240-MAX(0,b->z/64)+b->bias;
            if(da<=db)break;
            int t=order[j-1];order[j-1]=order[j];order[j]=t;
        }
    }
    gra_t *g=W_LoadGra("arcade");
    for(int i=0;i<count;i++){
        scenery_t *p=&scenes[stage].objects[order[i]];
        int y=239-p->z/64;
        if((y>FLOOR_Y)!=foreground)continue;
        if(p->duration && p->script){
            if(level.leveltime<p->lasttic){PS_StartScenery(&p->animation,p->script,p->duration,(p->descriptor^(uint32_t)p->x^(uint32_t)p->z)|1);p->lasttic=0;}
            while(p->lasttic<level.leveltime){PS_Tick(&p->animation);p->lasttic++;}
            p->frame=p->animation.frameid;
        }
        int x=VIEWWIDTH/2+(p->x-W_ArcadeStageScroll(stage,camx,y))/64;
        V_DrawPatchScaled(x,y,W_Frame(g,p->frame),p->palette,p->flags,p->scale,p->scale);
    }
}
static void Tribes(int camx)
{
    static struct {psanim_t animation;int lasttic,character;uint32_t script;} people[4];
    static const stage_t *stage;
    const int offsets[]={-58,-36,76,98};
    if(stage!=level.stage){
        for(int i=0;i<4;i++)people[i].script=0;
        stage=level.stage;
    }
    for(int i=0;i<4;i++){
        int character=MAX(0,level.fighters[i/2].def->codeindex);
        int variant=(character+i%2)%6;
        uint32_t descriptor=0x6d270+variant*16,script=X_U32(descriptor);
        if(people[i].script!=script || people[i].character!=character || level.leveltime<people[i].lasttic){
            PS_StartScenery(&people[i].animation,script,X_U8(descriptor+5),0x71be0001+i);
            people[i].script=script;people[i].lasttic=0;people[i].character=character;
        }
        while(people[i].lasttic<level.leveltime){PS_Tick(&people[i].animation);people[i].lasttic++;}
        // Ambient followers retain their original 3D-model animation and team
        // colours. They are scenery in this port; eating/worship AI is separate.
        V_DrawPatchScaled(ARENA_WIDTH/2+offsets[i]-camx,FLOOR_Y-7+i%2*3,
                         W_Frame(W_LoadGra("arcade"),people[i].animation.frameid),
                         W_ArcadeTribePalette(character,i/2),i/2?V_FLIP:0,0.75f,0.75f);
    }
}
void R_DrawStage(const stage_t *st,int camx)
{
    W_DrawArcadeStage((int)(st-stages),camx);
    Props((stage_t *)st,camx,false);
    Tribes(camx);
}
void R_DrawStageForeground(const stage_t *st,int camx) {Props((stage_t *)st,camx,true);}
