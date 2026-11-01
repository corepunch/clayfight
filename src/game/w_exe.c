// Big-endian 68020 program ROM. Runtime data comes only from arcade chips.
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "game/resources.h"
static byte *program;
static const int PROGRAM_SIZE = 0x200000;
bool X_Loaded(void) { return program != NULL; }
bool X_Init(const char *directory)
{
    static const char *const names[] = {"136102-1044b.29l", "136102-1043b.28l", "136102-1042b.27l", "136102-1041b.25l"};
    if (program) return true;
    program = I_Malloc(PROGRAM_SIZE);
    byte *chip = I_Malloc(0x80000);
    for (int lane=0;lane<4;lane++) {
        char path[1024]; snprintf(path,sizeof path,"%s/%s",directory,names[lane]);
        FILE *f=fopen(path,"rb");
        if (!f) { free(chip);free(program);program=NULL;return false; }
        size_t n=fread(chip,1,0x80000,f);int extra=fgetc(f);fclose(f);
        if(n!=0x80000 || extra!=EOF) I_Error("Arcade program chip has incorrect size: %s",names[lane]);
        for(int i=0;i<0x80000;i++)program[i*4+lane]=chip[i];
    }
    free(chip);
    if(X_U32(0x6d964)!=0x57112 || X_U16(0x57112)!=3818 || X_U32(0x6e38c)!=0x63f02)
        I_Error("Unsupported arcade program revision; expected Primal Rage 2.3 Dec 1994 (primrageo)");
    I_Log("X_Init: arcade 68020 ROM, 2 MB, primrageo 2.3");return true;
}
bool X_Valid(uint32_t a,int n) {return program && n>=0 && a<(uint32_t)PROGRAM_SIZE && (uint32_t)n<=(uint32_t)PROGRAM_SIZE-a;}
byte X_U8(uint32_t a) {return X_Valid(a,1)?program[a]:0;}
int8_t X_S8(uint32_t a) {return (int8_t)X_U8(a);}
uint16_t X_U16(uint32_t a) {return X_Valid(a,2)?(uint16_t)(program[a]<<8|program[a+1]):0;}
int16_t X_S16(uint32_t a) {return (int16_t)X_U16(a);}
uint32_t X_U32(uint32_t a) {return X_Valid(a,4)?((uint32_t)X_U16(a)<<16)|X_U16(a+2):0;}
