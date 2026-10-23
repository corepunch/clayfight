// i_video.c -- SDL2 window, framebuffer presentation and input (cf. i_video.c)

#include <SDL.h>

#include "platform/platform.h"

static SDL_Window *window;
static SDL_Renderer *renderer;
static SDL_Texture *texture;
static bool quit;
static bool headless;

#define MAXPADS 2
static SDL_GameController *pads[MAXPADS];
static bool padstate[MAXPADS][16];

void I_InitGraphics(bool nowindow)
{
    headless = nowindow;
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMECONTROLLER) < 0)
        I_Error("SDL_Init: %s", SDL_GetError());
    if (headless)
        return;

    window = SDL_CreateWindow("Clayfight", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                              SCREENWIDTH * 3, SCREENHEIGHT * 3,
                              SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI);
    if (!window)
        I_Error("SDL_CreateWindow: %s", SDL_GetError());
    renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (!renderer)
        I_Error("SDL_CreateRenderer: %s", SDL_GetError());
    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "nearest");
    SDL_RenderSetLogicalSize(renderer, SCREENWIDTH, SCREENHEIGHT);
    SDL_RenderSetIntegerScale(renderer, SDL_TRUE);
    texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING,
                                SCREENWIDTH, SCREENHEIGHT);
}

void I_ShutdownGraphics(void)
{
    for (int i = 0; i < MAXPADS; i++)
        if (pads[i])
            SDL_GameControllerClose(pads[i]);
    if (texture)
        SDL_DestroyTexture(texture);
    if (renderer)
        SDL_DestroyRenderer(renderer);
    if (window)
        SDL_DestroyWindow(window);
}

void I_FinishUpdate(const pixel_t *screen)
{
    if (headless)
        return;
    SDL_UpdateTexture(texture, NULL, screen, SCREENWIDTH * sizeof(pixel_t));
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
    SDL_RenderClear(renderer);
    SDL_RenderCopy(renderer, texture, NULL, NULL);
    SDL_RenderPresent(renderer);
}

static void post(evtype_t type, int key)
{
    event_t ev = {type, key};
    D_PostEvent(&ev);
}

// Game controllers: turn button/axis state into edge-triggered virtual keys.
static void I_PollPads(void)
{
    for (int i = 0; i < MAXPADS; i++) {
        SDL_GameController *c = pads[i];
        if (!c)
            continue;
        int ax = SDL_GameControllerGetAxis(c, SDL_CONTROLLER_AXIS_LEFTX);
        int ay = SDL_GameControllerGetAxis(c, SDL_CONTROLLER_AXIS_LEFTY);
#define BTN(b) SDL_GameControllerGetButton(c, SDL_CONTROLLER_BUTTON_##b)
        bool now[16] = {
            [PK_UP] = ay < -16000 || BTN(DPAD_UP),
            [PK_DOWN] = ay > 16000 || BTN(DPAD_DOWN),
            [PK_LEFT] = ax < -16000 || BTN(DPAD_LEFT),
            [PK_RIGHT] = ax > 16000 || BTN(DPAD_RIGHT),
            [PK_B0] = BTN(X), // high quick
            [PK_B1] = BTN(Y), // high fierce
            [PK_B2] = BTN(A), // low quick
            [PK_B3] = BTN(B), // low fierce
            [PK_START] = BTN(START),
            [PK_BACK] = BTN(BACK),
        };
#undef BTN
        for (int k = 0; k < 16; k++) {
            if (now[k] != padstate[i][k])
                post(now[k] ? ev_keydown : ev_keyup, PADKEY(i, k));
            padstate[i][k] = now[k];
        }
    }
}

static void I_OpenPad(int index)
{
    for (int i = 0; i < MAXPADS; i++) {
        if (!pads[i]) {
            pads[i] = SDL_GameControllerOpen(index);
            return;
        }
    }
}

void I_StartTic(void)
{
    SDL_Event e;
    while (SDL_PollEvent(&e)) {
        switch (e.type) {
        case SDL_QUIT:
            quit = true;
            break;
        case SDL_KEYDOWN:
            if (!e.key.repeat)
                post(ev_keydown, e.key.keysym.scancode);
            break;
        case SDL_KEYUP:
            post(ev_keyup, e.key.keysym.scancode);
            break;
        case SDL_CONTROLLERDEVICEADDED:
            I_OpenPad(e.cdevice.which);
            break;
        case SDL_CONTROLLERDEVICEREMOVED:
            for (int i = 0; i < MAXPADS; i++) {
                if (pads[i] && SDL_JoystickInstanceID(SDL_GameControllerGetJoystick(pads[i])) == e.cdevice.which) {
                    SDL_GameControllerClose(pads[i]);
                    pads[i] = NULL;
                }
            }
            break;
        }
    }
    I_PollPads();
}

bool I_QuitRequested(void)
{
    return quit;
}

void I_SaveScreenshot(const pixel_t *screen, const char *path)
{
    SDL_Surface *s = SDL_CreateRGBSurfaceWithFormatFrom((void *)screen, SCREENWIDTH, SCREENHEIGHT, 32,
                                                        SCREENWIDTH * 4, SDL_PIXELFORMAT_ARGB8888);
    if (!s || SDL_SaveBMP(s, path) < 0)
        I_Log("I_SaveScreenshot: %s", SDL_GetError());
    SDL_FreeSurface(s);
}
