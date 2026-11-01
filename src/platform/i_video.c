// i_video.c -- SDL2 window, framebuffer presentation and input (cf. i_video.c)

#include <SDL.h>
#include <string.h>

#include "platform/platform.h"

static SDL_Window *window;
static SDL_Renderer *renderer;
static SDL_Texture *texture;
static bool quit;
static bool headless;
static bool focused=true, keys[SDL_NUM_SCANCODES];
void G_ClearInput(void);

#define MAXPADS 2
static SDL_GameController *pads[MAXPADS];
static bool padstate[MAXPADS][16];

void I_InitGraphics(bool nowindow)
{
    headless = nowindow;
    if (headless)
        SDL_setenv("SDL_VIDEODRIVER", "dummy", 1);
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMECONTROLLER) < 0)
        I_Error("SDL_Init: %s", SDL_GetError());
    if (headless)
        return;

    window = SDL_CreateWindow("Clayfight", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                              SCREENWIDTH, SCREENHEIGHT,
                              SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI);
    if (!window)
        I_Error("SDL_CreateWindow: %s", SDL_GetError());
    SDL_SetWindowMinimumSize(window, SCREENWIDTH, SCREENHEIGHT);
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

// SDL scancode (a physical key) -> the game's key; 0 for keys it has no name for.
static int I_TranslateKey(SDL_Scancode sc)
{
    if (sc >= SDL_SCANCODE_A && sc <= SDL_SCANCODE_Z)
        return 'a' + (sc - SDL_SCANCODE_A);
    if (sc >= SDL_SCANCODE_1 && sc <= SDL_SCANCODE_9)
        return '1' + (sc - SDL_SCANCODE_1);
    switch (sc) {
    case SDL_SCANCODE_0: return '0';
    case SDL_SCANCODE_RETURN:
    case SDL_SCANCODE_KP_ENTER: return KEY_ENTER;
    case SDL_SCANCODE_ESCAPE: return KEY_ESCAPE;
    case SDL_SCANCODE_SPACE: return KEY_SPACE;
    case SDL_SCANCODE_TAB: return '\t';
    case SDL_SCANCODE_UP: return KEY_UPARROW;
    case SDL_SCANCODE_DOWN: return KEY_DOWNARROW;
    case SDL_SCANCODE_LEFT: return KEY_LEFTARROW;
    case SDL_SCANCODE_RIGHT: return KEY_RIGHTARROW;
    case SDL_SCANCODE_MINUS: return '-';
    case SDL_SCANCODE_EQUALS: return '=';
    case SDL_SCANCODE_LEFTBRACKET: return '[';
    case SDL_SCANCODE_RIGHTBRACKET: return ']';
    case SDL_SCANCODE_BACKSLASH: return '\\';
    case SDL_SCANCODE_SEMICOLON: return ';';
    case SDL_SCANCODE_APOSTROPHE: return '\'';
    case SDL_SCANCODE_GRAVE: return '`';
    case SDL_SCANCODE_COMMA: return ',';
    case SDL_SCANCODE_PERIOD: return '.';
    case SDL_SCANCODE_SLASH: return '/';
    default: return 0;
    }
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
        case SDL_KEYUP: {
            int key = I_TranslateKey(e.key.keysym.scancode);
            bool down=e.type==SDL_KEYDOWN;
            if (key && !e.key.repeat && keys[e.key.keysym.scancode]!=down) {
                keys[e.key.keysym.scancode]=down;
                post(down ? ev_keydown : ev_keyup,key);
            }
            break;
        }
        case SDL_WINDOWEVENT:
            if(e.window.event==SDL_WINDOWEVENT_FOCUS_LOST) {
                focused=false;memset(keys,0,sizeof keys);memset(padstate,0,sizeof padstate);
                D_ProcessEvents(NULL,0);G_ClearInput();
            } else if(e.window.event==SDL_WINDOWEVENT_FOCUS_GAINED)focused=true;
            break;
        case SDL_CONTROLLERDEVICEADDED:
            I_OpenPad(e.cdevice.which);
            break;
        case SDL_CONTROLLERDEVICEREMOVED:
            for (int i = 0; i < MAXPADS; i++) {
                if (pads[i] && SDL_JoystickInstanceID(SDL_GameControllerGetJoystick(pads[i])) == e.cdevice.which) {
                    for(int k=0;k<16;k++)if(padstate[i][k])post(ev_keyup,PADKEY(i,k));
                    memset(padstate[i],0,sizeof padstate[i]);
                    SDL_GameControllerClose(pads[i]);
                    pads[i] = NULL;
                }
            }
            break;
        }
    }
    if(focused)I_PollPads();
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
