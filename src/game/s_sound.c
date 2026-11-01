// Arcade CAGE commands. Character tables are read from the 68020 program ROM.
#include <string.h>
#include "game/game.h"
#define MAXVARIANTS 5
static const int commands[NUMSFX][MAXVARIANTS] = {
    [sfx_hit] = {0x8064,0x8065,0x8066,0x8067},
    [sfx_hithard] = {0x8068,0x8069,0x806a},
    [sfx_block] = {0x806b},
    [sfx_swish] = {0x8049},
    [sfx_thud] = {0x806f},
    [sfx_crowd] = {0x80e3},
    [sfx_select] = {0x806c},
};
static const sfxdata_t *effects[NUMSFX][MAXVARIANTS], *voices[7][4];
static const char *const prefixes[]={"REX","KON","TAL","COB","SPI","DIA","HAV"};
void S_Init(void)
{
    I_InitSound();
    for(int i=0;i<NUMSFX;i++)for(int k=0;k<MAXVARIANTS && commands[i][k];k++)
        effects[i][k]=W_ArcadeSound(commands[i][k]);
    for(int c=0;c<7;c++){
        // Victory roar, attack voice and pain voice use the retail lookup tables.
        voices[c][0]=W_ArcadeSound(X_U16(0x6b0b0+c*2));
        voices[c][1]=W_ArcadeSound(X_U16(0x6e3e8+c*2));
        voices[c][2]=voices[c][3]=W_ArcadeSound(X_U16(0x69c64+c*2));
    }
}
static float S_Sep(float x)
{
    return x<0?0:((x-level.camx)/VIEWWIDTH-0.5f)*1.2f;
}
void S_StartSound(sfxenum_t id,float x,float vol)
{
    if(!options.sound || !options.volume || id<0 || id>=NUMSFX)return;
    int n=0;while(n<MAXVARIANTS && effects[id][n])n++;
    if(n)I_StartSound(effects[id][M_Random()%n],vol*options.volume/10.f,S_Sep(x));
}
void S_StartVoice(const char *prefix,int which,float x,float vol)
{
    if(!options.sound || !options.volume)return;
    for(int c=0;c<7;c++)if(!strcmp(prefix,prefixes[c])){
        I_StartSound(voices[c][which&3],vol*options.volume/10.f,S_Sep(x));return;
    }
}
