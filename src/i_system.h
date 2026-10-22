// i_system.h -- platform timing, memory and errors (cf. i_system.h)

#ifndef __I_SYSTEM__
#define __I_SYSTEM__

#include <stddef.h>

int I_GetTime(void); // tics since startup
void I_Error(const char *fmt, ...) __attribute__((noreturn, format(printf, 1, 2)));
void I_Log(const char *fmt, ...) __attribute__((format(printf, 1, 2)));
void *I_Malloc(size_t size); // zeroed, never returns NULL

#endif
