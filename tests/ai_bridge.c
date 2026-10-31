// Minimal platform for loading the production AI into the x86 oracle driver.
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include "game/game.h"
void *I_Malloc(size_t size) { void *p = calloc(1, size); if (!p) abort(); return p; }
void I_Log(const char *fmt, ...) { (void)fmt; }
void I_Error(const char *fmt, ...) { va_list ap; va_start(ap, fmt); vfprintf(stderr, fmt, ap); va_end(ap); abort(); }
