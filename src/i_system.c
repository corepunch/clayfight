// i_system.c -- platform timing, memory and errors (cf. i_system.c)

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>

#include <SDL.h>

#include "pr_def.h"
#include "i_system.h"

int I_GetTime(void)
{
    static Uint64 base;
    Uint64 now = SDL_GetPerformanceCounter();
    if (!base)
        base = now;
    return (int)((now - base) * TICRATE / SDL_GetPerformanceFrequency());
}

void I_Error(const char *fmt, ...)
{
    char msg[512];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(msg, sizeof msg, fmt, ap);
    va_end(ap);
    fprintf(stderr, "Error: %s\n", msg);
    SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Clayfight", msg, NULL);
    SDL_Quit();
    exit(1);
}

void I_Log(const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    vprintf(fmt, ap);
    va_end(ap);
    putchar('\n');
}

void *I_Malloc(size_t size)
{
    void *p = calloc(1, size ? size : 1);
    if (!p)
        I_Error("I_Malloc: out of memory (%zu bytes)", size);
    return p;
}
