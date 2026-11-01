// Arcade resource banks: RLE motion objects, original RGB555 palettes and
// packed character/playfield chips. No DOS archives or converted art files.
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include "game/game.h"

#define RLE_BYTES 0x2000000
#define MAX_BANKS 16
static byte *rle, *tiles[3], *characters;
static char resource_dir[768];
static gra_t body;
static gra_t banks[MAX_BANKS];
static int bank_count;
static short *bank_pals[MAX_BANKS];
lumpinfo_t *lumpinfo;
int numlumps;

pixel_t W_RGB(int r,int g,int b) { return raster_rgb(r,g,b); }
static byte *ReadChip(const char *name,int expected)
{
    char path[1024];snprintf(path,sizeof path,"%s/%s",resource_dir,name);
    FILE *f=fopen(path,"rb");if(!f)I_Error("Missing arcade ROM: %s",path);
    byte *data=I_Malloc(expected);
    size_t n=fread(data,1,expected,f);int extra=fgetc(f);fclose(f);
    if(n!=(size_t)expected || extra!=EOF)I_Error("Incorrect arcade chip size: %s",path);
    return data;
}
static unsigned Word(unsigned at)
{
    if(at>=RLE_BYTES/2)I_Error("Arcade RLE address outside ROM: %#x",at);
    return (unsigned)rle[at*2]<<8|rle[at*2+1];
}
static void Pair(const char *low,const char *high,unsigned offset,int size)
{
    byte *a=ReadChip(low,size),*b=ReadChip(high,size);
    for(int i=0;i<size;i++){rle[offset+i*2]=b[i];rle[offset+i*2+1]=a[i];}
    free(a);free(b);
}
static void Palette(palette_t *p,uint32_t address,int count)
{
    p->count=count-1;
    for(int i=0;i<count;i++){
        unsigned v=X_U16(address+i*2);
        p->colors[i]=W_RGB((v>>10&31)*255/31,(v>>5&31)*255/31,(v&31)*255/31);
    }
}
static void Bounds(patch_t *p)
{
    int x0=p->w,y0=p->h,x1=-1,y1=-1;
    for(int y=0;y<p->h;y++)for(int x=0;x<p->w;x++){
        int v=p->pixels[y*p->w+x];p->maxval=MAX(p->maxval,v);
        if(v){x0=MIN(x0,x);y0=MIN(y0,y);x1=MAX(x1,x);y1=y;}
    }
    p->hasbox=x1>=0;p->bx0=x0-p->x;p->by0=y0-p->y;p->bx1=x1-p->x;p->by1=y1-p->y;
}
static int Run(int mode,int value,int *pixel)
{
    if(mode==0 || ((mode==1 || mode==4 || mode==6) && !(value&15))){*pixel=value&15;return (value>>4)+1;}
    if(mode<=3){*pixel=value&31;return (value>>5)+1;}
    *pixel=value&63;return (value>>6)+1;
}
static patch_t *Decode(int id)
{
    if(id<0 || id>=body.numframes)return NULL;
    frame_t *f=&body.frames[id];if(f->decoded)return f->patch;f->decoded=true;
    unsigned flags=Word(id*4+2),start=(flags&255)<<16|Word(id*4+3),at=start;
    if(start<(unsigned)id*4 || start>=RLE_BYTES/2)return NULL;
    int mode=(flags>>8)&7,w=0,h=0;
    while(h<1024){
        unsigned n=Word(at++);if(n&0x8000)n^=65535;if(!n)break;
        if(n>RLE_BYTES/2-at)I_Error("Arcade object %d: truncated row",id);
        int row=0,pixel;
        for(unsigned j=0;j<n;j++){int v=Word(at++);row+=Run(mode,v&255,&pixel);row+=Run(mode,v>>8,&pixel);}
        w=MAX(w,row);h++;
    }
    if(!w || !h)return NULL;
    if(w>4096 || h>=1024)I_Error("Arcade object %d has invalid dimensions",id);
    patch_t *p=I_Malloc(sizeof *p);p->w=f->w=w;p->h=f->h=h;
    p->x=f->x=(int16_t)Word(id*4);p->y=f->y=(int16_t)Word(id*4+1);p->pixels=I_Malloc(w*h);
    at=start;
    for(int y=0;y<h;y++){
        unsigned n=Word(at++);if(n&0x8000)n^=65535;int x=0;
        for(unsigned j=0;j<n;j++){
            int value=Word(at++);
            for(int k=0;k<2;k++){
                int pixel,count=Run(mode,k?value>>8:value&255,&pixel);
                if(count>w-x)I_Error("Arcade object %d row overflow",id);
                memset(p->pixels+y*w+x,pixel,count);x+=count;
            }
        }
    }
    Bounds(p);f->patch=p;return p;
}

void W_Init(const char *directory)
{
    snprintf(resource_dir,sizeof resource_dir,"%s",directory);
    W_SoundDirectory(directory);
    if(!X_Init(directory))I_Error("Supply Primal Rage 2.3 arcade ROMs in %s",directory);
    rle=I_Malloc(RLE_BYTES);
    Pair("136102-1100a.2v","136102-1101a.2w",0,0x80000);
    for(int i=0;i<12;i++){
        char lo[64],hi[64];int n=i<4?332+2*i:316+2*(i-4),bank=i<4?1:0,slot=i<4?i:i-4;
        snprintf(lo,sizeof lo,"136102-%04d.mol%d.%d",n,bank,slot);
        snprintf(hi,sizeof hi,"136102-%04d.moh%d.%d",n+1,bank,slot);
        Pair(lo,hi,(i<4?0x800000+i*0x200000:0x1000000+(i-4)*0x200000),0x100000);
    }
    unsigned lowest=RLE_BYTES/2;
    for(unsigned at=0;at<lowest;at+=4){unsigned p=(Word(at+2)&255)<<16|Word(at+3);if(p>at && p<lowest)lowest=p;}
    body.numframes=lowest/4;if(body.numframes!=12000)I_Error("Unsupported arcade RLE object table");
    snprintf(body.name,sizeof body.name,"arcade");body.frames=I_Malloc(body.numframes*sizeof *body.frames);
    body.numpalettes=28;body.palettes=I_Malloc(body.numpalettes*sizeof *body.palettes);
    for(int ch=0;ch<7;ch++)for(int alt=0;alt<4;alt++)Palette(&body.palettes[ch*4+alt],X_U32(0x6e38c+ch*4)+alt*128,64);
    tiles[0]=ReadChip("136102-0050a.25n",0x80000);tiles[1]=ReadChip("136102-0051a.27n",0x80000);tiles[2]=ReadChip("136102-0052a.28n",0x80000);
    characters=ReadChip("136102-1045b.23p",0x20000);
    I_Log("W_Init: 12000 arcade motion-object slots, tile planes and character ROM");
}

patch_t *W_Frame(gra_t *g,int index)
{
    if(!g || index<0 || index>=g->numframes)return NULL;
    if(g==&body)return Decode(index);
    frame_t *f=&g->frames[index];
    if(!f->decoded){f->patch=f->offset>=0?Decode(f->offset):NULL;f->decoded=true;}
    return f->patch;
}
static gra_t *Bank(const char *name,int count,int palettes)
{
    if(bank_count==MAX_BANKS)I_Error("Too many arcade art banks");
    gra_t *g=&banks[bank_count];snprintf(g->name,sizeof g->name,"%s",name);g->numframes=count;g->numpalettes=palettes;
    g->frames=I_Malloc(count*sizeof *g->frames);g->palettes=I_Malloc(palettes*sizeof *g->palettes);
    bank_pals[bank_count]=I_Malloc(count*sizeof **bank_pals);
    for(int i=0;i<count;i++)g->frames[i].offset=-1;
    bank_count++;return g;
}
static void Frame(gra_t *g,int slot,int id,int pal)
{
    g->frames[slot].offset=id;bank_pals[g-banks][slot]=pal;
}
static patch_t *Patch(int w,int h)
{
    patch_t *p=I_Malloc(sizeof *p);p->w=w;p->h=h;p->pixels=I_Malloc(w*h);return p;
}
static void Composite(patch_t *dst,const patch_t *src,int x,int y,int bank)
{
    for(int j=0;j<src->h;j++)for(int i=0;i<src->w;i++){
        int v=src->pixels[j*src->w+i];if(!v || x+i<0 || x+i>=dst->w || y+j<0 || y+j>=dst->h)continue;
        dst->pixels[(y+j)*dst->w+x+i]=v+bank;
    }
}
static patch_t *AlphaPatch(uint32_t map)
{
    int w=X_U8(map),h=X_U8(map+1);
    if(!w || w>42 || !h || h>30 || !X_Valid(map,2+w*h*2))I_Error("Invalid arcade alpha map %#x",map);
    patch_t *p=Patch(w*8,h*8);
    for(int y=0;y<p->h;y++)for(int x=0;x<p->w;x++){
        int code=X_U16(map+2+2*(y/8*w+x/8))&4095;
        int v=characters[code*32+y%8*4+x%8/2];
        p->pixels[y*p->w+x]=(v>>(x%2?0:4))&15;
    }
    Bounds(p);return p;
}
static gra_t *Hud(void)
{
    gra_t *g=Bank("hud",HUD_FRAMES,10);
    Palette(&g->palettes[0],0x61482,16); // alpha bank 6: red health vein
    Palette(&g->palettes[1],0x614a2,16); // alpha bank 7: gold stun vein
    Palette(&g->palettes[2],0x61742,32); // common heart and brain sprites
    const uint32_t tables[]={0x1ff43e,0x1ff622,0x1ff806,0x1ff91a};
    const int starts[]={HUD_HEALTH_LEFT,HUD_HEALTH_RIGHT,HUD_STUN_LEFT,HUD_STUN_RIGHT};
    for(int k=0;k<4;k++)for(int i=0;i<(k<2?121:69);i++){
        int slot=starts[k]+i;patch_t *p=AlphaPatch(X_U32(tables[k]+i*4));
        g->frames[slot]=(frame_t){.w=p->w,.h=p->h,.decoded=true,.patch=p};
        bank_pals[g-banks][slot]=k<2?0:1;
    }
    for(int i=0;i<10;i++)Frame(g,HUD_HEART+i,1767+i,2);
    for(int i=0;i<18;i++)Frame(g,HUD_BRAIN+i,1777+i,2);
    for(int c=0;c<7;c++){
        uint32_t desc=X_U32(0x73e74+c*4);
        Palette(&g->palettes[3+c],0x64d02+c*128,64);
        Frame(g,HUD_NAME+c,X_U32(desc),3+c);
    }
    return g;
}
static gra_t *Select(void)
{
    gra_t *g=Bank("select",155,31);
    uint32_t base=X_U32(0x183b0); // native temple setup copies 13 banks
    const int portrait[]={130,91,141,149,125,113,97};
    const int object[]={852,862,877,882,872,857,867};
    const int pals[]={13,4,16,18,12,11,6};
    for(int c=0;c<7;c++){
        Palette(&g->palettes[pals[c]],X_U32(0x676fa+c*4),64);
        for(int k=0;k<5;k++)Frame(g,portrait[c]+k,object[c]+k,pals[c]);
    }
    Palette(&g->palettes[14],base+X_U16(0x6ac2a)*2,128);
    patch_t *bg=Patch(336,240);Composite(bg,Decode(785),0,0,64);Composite(bg,Decode(784),0,0,0);Bounds(bg);
    g->frames[136]=(frame_t){.w=336,.h=240,.decoded=true,.patch=bg};
    bank_pals[g-banks][136]=14;
    Palette(&g->palettes[9],base+X_U16(0x6ac4a)*2,64);g->palettes[10]=g->palettes[9];
    Frame(g,105,786,9);Frame(g,106,787,10);
    static const int heads[]={140,139,148,147,103,102,111};
    static const int headpal[]={28,30,29,1,2,0,27};
    for(int i=0;i<7;i++){
        Palette(&g->palettes[headpal[i]],base+X_U16(0x6ad1c+i*2)*2,64);
        Frame(g,heads[i],X_U32(0x6ad00+i*4),headpal[i]);
    }
    return g;
}
gra_t *W_TryLoadGra(const char *name)
{
    if(!strcasecmp(name,"arcade"))return &body;
    for(int i=0;i<bank_count;i++)if(!strcasecmp(banks[i].name,name))return &banks[i];
    if(!strcasecmp(name,"select"))return Select();
    if(!strcasecmp(name,"hud"))return Hud();
    if(!strcasecmp(name,"large-font")){
        gra_t *g=Bank(name,128,1);Palette(g->palettes,0x61442,16);
        // Native alpha font 3 (VERSION text): ASCII lookup at c510, four
        // column-major tiles per glyph at c370, character-bank offset 80.
        for(int c=32;c<128;c++){
            int glyph=X_U8(0xc510+c);if(glyph>=52)continue;
            uint32_t entry=0xc370+glyph*8;
            int w=X_U16(entry+4)?16:8;
            patch_t *p=Patch(w,16);
            for(int y=0;y<16;y++)for(int x=0;x<w;x++){
                int code=X_U16(entry+2*(x/8*2+y/8))+(glyph>=50?0:128);
                int v=characters[(code&4095)*32+y%8*4+x%8/2];
                p->pixels[y*w+x]=(v>>(x%2?0:4))&15;
            }
            Bounds(p);g->frames[c]=(frame_t){.w=w,.h=16,.decoded=true,.patch=p};
        }
        return g;
    }
    if(!strcasecmp(name,"stone-font")){
        gra_t *g=Bank(name,128,1);Palette(g->palettes,0x68e3a,16);
        for(int c='A';c<='Z';c++)Frame(g,c,391+c-'A',0);
        for(int c='1';c<='9';c++)Frame(g,c,417+c-'1',0);
        Frame(g,'0',426,0);
        const char chars[]="$!.?:#'&";const int ids[]={427,428,429,430,431,432,433,434};
        for(int i=0;chars[i];i++)Frame(g,chars[i],ids[i],0);
        return g;
    }
    if(!strcasecmp(name,"slabs")){
        gra_t *g=Bank(name,38,1);Palette(g->palettes,0x61042,64);Frame(g,37,826,0);return g;
    }
    if(!strcasecmp(name,"title")){
        gra_t *g=Bank(name,46,3);
        // Native title setup 35bc0 copies seven 64-colour banks from 650e2.
        // Plaque, PRIMAL and RAGE select different offsets in their descriptors.
        const uint32_t descriptors[]={0x7422c,0x7423c,0x7425c};
        uint32_t base=X_U32(0x35bc8);
        for(int i=0;i<3;i++)Palette(&g->palettes[i],base+X_U16(descriptors[i]+6)*2,64);
        for(int i=0;i<14;i++)Frame(g,i,447+i,1);
        for(int i=0;i<6;i++)Frame(g,14+i,475+i,2);
        Frame(g,45,446,0);return g;
    }
    if(!strcasecmp(name,"globe")){
        gra_t *g=Bank(name,47,4);uint32_t base=X_U32(0x36ca4);
        for(int i=0;i<4;i++)Palette(&g->palettes[i],base+X_U16(0x74174+i*16+6)*2,64);
        for(int i=0;i<47;i++)Frame(g,i,MAX(491,491+i-8),i<20?0:1+(i-20)/13);
        return g;
    }
    if(!strcasecmp(name,"fonts")){
        gra_t *g=Bank(name,128,1);
        // The authored bevel ramps run from light fill to dark edges.
        // Increasing greys inverted that shading and produced a bright rim.
        Palette(g->palettes,0x61802,16);
        for(int c=0;c<128;c++){
            patch_t *p=Patch(8,8);for(int k=0;k<32;k++){int v=characters[c*32+k];p->pixels[k*2]=v>>4;p->pixels[k*2+1]=v&15;}
            Bounds(p);
            if(p->hasbox && p->bx1<7){
                int w=p->bx1+1;byte *crop=I_Malloc(w*8);
                for(int y=0;y<8;y++)memcpy(crop+y*w,p->pixels+y*8,w);
                free(p->pixels);p->pixels=crop;p->w=w;Bounds(p);
            }
            g->frames[c]=(frame_t){.w=8,.h=8,.decoded=true,.patch=p};
        }
        return g;
    }
    return NULL;
}
gra_t *W_LoadGra(const char *name) {gra_t *g=W_TryLoadGra(name);if(!g)I_Error("Unknown arcade art bank: %s",name);return g;}
gra_t *W_GraById(int id) {(void)id;return &body;}
int W_FramePalette(gra_t *g,int index) {return g>=banks && g<banks+bank_count && index>=0 && index<g->numframes?bank_pals[g-banks][index]:0;}
int W_GuessPalette(const patch_t *p,const gra_t *g)
{
    for(int i=0;i<g->numframes;i++)if(g->frames[i].decoded && g->frames[i].patch==p)return W_FramePalette((gra_t *)g,i);
    return 0;
}

// Every 8x8 playfield tile combines the three original ROM planes. Granularity
// remains 32 after MAME/Atari's 4+2 bit blend; map colour words are usually even.
static int TilePixel(unsigned code,int x,int y)
{
    unsigned at=code*16+y*2+x/4;if(at>=0x80000)I_Error("Arcade tile index outside ROM");
    int lo=tiles[x%4<2?1:0][at],hi=tiles[2][at];
    return ((lo>>(x%2?0:4))&15)|((hi>>(4+x%4)&1)<<5)|((hi>>(x%4)&1)<<4);
}
// The hardware has two 64-column halves. Some source skies have 72
// columns; the native copier's 64-column row stride overwrites their overhang.
static byte *StageMap(uint32_t table,int stage,int bank,int flags,int *height)
{
    uint32_t left=X_U32(table+stage*4),right=X_U32(table+32+stage*4);
    int w=X_U8(left),h=X_U8(left+1),rw=X_U8(right);
    if(w<64 || rw<64 || h<1 || h>64 || X_U8(right+1)!=h
       || !X_Valid(left,2+w*h*2) || !X_Valid(right,2+rw*h*2))
        I_Error("Invalid arcade stage map %#x",left);
    *height=h*8;
    byte *p=I_Malloc(1024*h*8);
    for(int side=0;side<2;side++)for(int y=0;y<h*8;y++)for(int x=0;x<512;x++){
        uint32_t map=side?right:left;int stride=side?rw:w;
        int word=X_U16(map+2+2*(y/8*stride+x/8))|flags;
        int v=TilePixel(bank*4096+(word&4095),word&0x8000?7-x%8:x%8,y%8);
        p[y*1024+side*512+x]=(v+((word>>12)&7)*32)&255;
    }
    return p;
}
// Original scanline routine 23c74: signed 26.6 camera positions and a
// per-arena perspective denominator. The distant layer uses the last floor
// scroll, plus the arena's explicit depth offset; never a guessed width ratio.
int W_ArcadeStageScroll(int stage,int camx,int y)
{
    int horizon=X_U16(0x6ba0c+stage*2),rows=248-horizon;
    int camera=(camx-(ARENA_WIDTH-VIEWWIDTH)/2)*64;
    int step=camera*256/X_U16(0x6b9ec+stage*2);
    int depth=y<horizon?rows+X_U16(0x6ba1c+stage*2):rows-1-(y-horizon);
    return (camera*256-step*depth)/256;
}
const palette_t *W_ArcadeStagePalette(int stage,int color)
{
    static palette_t palettes[8][64];static bool ready[8][64];
    if(stage<0 || stage>=8 || color<0 || color>=0x800 || (color>=0x400 && color<0x480) || color%32)
        I_Error("Invalid arcade scenery palette %d:%#x",stage,color);
    int slot=color/32;
    if(!ready[stage][slot]){
        uint32_t base=color<0x200?0x61502u+color*2:color<0x400?
                      X_U32(0x614e2+stage*4)+(color-0x200)*2:0x63a82u+(color-0x480)*2;
        Palette(&palettes[stage][slot],base,64);ready[stage][slot]=true;
    }
    return &palettes[stage][slot];
}
patch_t *W_ArcadeShadow(int character,int frame)
{
    static const uint32_t bases[]={0x57110,0x5b186,0x59e38,0x5cb90,0x58b02,0x57110,0x5b186};
    if(character<0 || character>=7)return NULL;
    int index=frame-X_U16(bases[character]);
    if(index<0 || index>1100)return NULL; // native shadow updater 341fa
    int id=X_U16(X_U32(0x6b068+character*4)+index*2);
    return id<12000?Decode(id):NULL;
}
const palette_t *W_ArcadeImpactPalette(bool blocked)
{
    static palette_t palettes[2];static bool ready;
    if(!ready){
        // Native contact object 6cf50 starts in colour bank 190; the block
        // script's SET_ATTR 18 switches to 180. Both use all 64 RLE colours.
        Palette(&palettes[0],0x61502+X_U16(0x6cf56)*2,64);
        Palette(&palettes[1],0x61802,64);ready=true;
    }
    return &palettes[blocked?1:0];
}
const palette_t *W_ArcadeTribePalette(int character,int player)
{
    static palette_t palettes[2][7];static bool ready;
    if(!ready){
        // Native 295ce selects the second set for player two, independently
        // of whether the fighters use alternate body colours.
        for(int p=0;p<2;p++)for(int c=0;c<7;c++)Palette(&palettes[p][c],0x63382+p*0x380+c*128,64);
        ready=true;
    }
    return &palettes[CLAMP(player,0,1)][CLAMP(character,0,6)];
}
static void StageCycles(int stage,palette_t *palette,int time)
{
    // PEA argument blocks for the original 349f8 colour-cycle registrations.
    // Each contains length, period, signed first colour, colour group, PF base.
    static const uint32_t calls[8][3]={
        {0x2478c,0x247a2,0x247bc},{0x24736,0x2474c,0x24766},
        {0x247fa},{0},{0},{0x247da},{0},{0},
    };
    for(int i=0;i<3 && calls[stage][i];i++){
        uint32_t p=calls[stage][i];int length=X_S16(p+2),period=X_S16(p+6),offset=X_S16(p+10);
        int start=X_S16(p+14)*64+(offset==-128?0:abs(offset));
        if(length<2 || length>16 || period<1 || start+length>256 || X_U16(p+18)!=0x100)
            I_Error("Invalid arcade palette cycle %#x",p);
        int shift=time>0?(1+(time-1)/period)%length:0;
        pixel_t colors[16];memcpy(colors,palette->colors+start,length*sizeof *colors);
        for(int j=0;j<length;j++)palette->colors[start+j]=colors[(j+(offset<0?-shift:shift)+length)%length];
    }
}
void W_DrawArcadeStage(int stage,int camx)
{
    static struct {byte *far,*floor;int h,fh;palette_t palette;} cache[8];
    if(stage<0 || stage>=8)I_Error("Invalid arcade stage %d",stage);
    if(!cache[stage].far){
        Palette(&cache[stage].palette,X_U32(0x5fea2+stage*4),256);
        cache[stage].far=StageMap(0x1e58c4,stage,X_U8(0x1e5944+stage),0,&cache[stage].h);
        cache[stage].floor=StageMap(0x1e5904,stage,X_U8(0x1e594c+stage),0x4000,&cache[stage].fh);
    }
    palette_t palette=cache[stage].palette;StageCycles(stage,&palette,level.leveltime);
    int horizon=X_U16(0x6ba0c+stage*2);
    for(int y=0;y<VIEWHEIGHT;y++){
        bool far=y<horizon;
        int py=far?y+cache[stage].h-horizon:y-horizon;
        py=CLAMP(py,0,(far?cache[stage].h:cache[stage].fh)-1);
        int scroll=(far?X_U16(0x6b9fc+stage*2)/32:344)+W_ArcadeStageScroll(stage,camx,y)/64;
        const byte *row=(far?cache[stage].far:cache[stage].floor)+py*1024;
        for(int x=0;x<VIEWWIDTH;x++){
            pixel_t c=palette.colors[row[(x+scroll)&1023]];
            int at=y*2*SCREENWIDTH+x*2;
            screen[at]=screen[at+1]=screen[at+SCREENWIDTH]=screen[at+SCREENWIDTH+1]=c;
        }
    }
}
// Audio registry is implemented by the CAGE decoder in w_sound.c.
