// i_video.h -- window, framebuffer presentation and input polling (cf. i_video.h)

#ifndef __I_VIDEO__
#define __I_VIDEO__

#include "pr_def.h"

void I_InitGraphics(bool headless);
void I_ShutdownGraphics(void);
void I_FinishUpdate(const pixel_t *screen);
void I_StartTic(void); // pump SDL events into the event queue
bool I_QuitRequested(void);
void I_SaveScreenshot(const pixel_t *screen, const char *path);

#endif
