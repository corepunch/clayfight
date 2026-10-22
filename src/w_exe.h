// w_exe.h -- read data tables straight out of PRAGE.EXE (cf. w_wad.h)
//
// The move scripts, frame tables, hit boxes and special-move inputs of
// Primal Rage are compiled into its executable. Like Doom reading lumps out
// of a WAD, we load the executable's two LE objects at their link addresses,
// apply the relocations, and then read tables by their linear address.

#ifndef __W_EXE__
#define __W_EXE__

#include "pr_def.h"

bool X_Init(const char *path); // false if missing or not the expected build
bool X_Loaded(void);
bool X_Valid(uint32_t addr, int len);
byte X_U8(uint32_t addr);
int8_t X_S8(uint32_t addr);
uint16_t X_U16(uint32_t addr);
int16_t X_S16(uint32_t addr);
uint32_t X_U32(uint32_t addr);

#endif
