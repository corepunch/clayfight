// Exercise actual SDL event translation, including repeat and focus recovery.
#include <SDL.h>
#include "platform/platform.h"
#include <assert.h>
#include <stdio.h>
#include <stdarg.h>
static int count,focus;static event_t events[64];
void D_PostEvent(const event_t *e) {assert(count<64);events[count++]=*e;}
void D_ProcessEvents(responder_t *r,int n) {}
void G_ClearInput(void) {focus++;}
void I_Log(const char *fmt,...) {}
void I_Error(const char *fmt,...) {abort();}
static void key(SDL_Scancode code,int down,int repeat) {
    SDL_Event e={0};e.type=down?SDL_KEYDOWN:SDL_KEYUP;e.key.keysym.scancode=code;e.key.repeat=repeat;
    assert(SDL_PushEvent(&e)==1);I_StartTic();
}
int main(void) {
    I_InitGraphics(true);I_StartTic();count=0;
    key(SDL_SCANCODE_W,1,0);key(SDL_SCANCODE_W,1,1);key(SDL_SCANCODE_W,1,0);key(SDL_SCANCODE_W,0,0);
    assert(count==2 && events[0].key=='w' && events[1].type==ev_keyup);
    key(SDL_SCANCODE_RIGHT,1,0);key(SDL_SCANCODE_RIGHT,0,0);assert(events[count-1].key==KEY_RIGHTARROW);
    key(SDL_SCANCODE_KP_ENTER,1,0);key(SDL_SCANCODE_KP_ENTER,0,0);assert(events[count-1].key==KEY_ENTER);
    key(SDL_SCANCODE_TAB,1,0);key(SDL_SCANCODE_TAB,0,0);assert(events[count-1].key=='\t');
    key(SDL_SCANCODE_SEMICOLON,1,0);key(SDL_SCANCODE_SEMICOLON,0,0);assert(events[count-1].key==';');
    int before=count;key(SDL_SCANCODE_F1,1,0);assert(count==before);
    key(SDL_SCANCODE_W,1,0);SDL_Event e={0};e.type=SDL_WINDOWEVENT;e.window.event=SDL_WINDOWEVENT_FOCUS_LOST;
    assert(SDL_PushEvent(&e)==1);I_StartTic();assert(focus==1);
    e.window.event=SDL_WINDOWEVENT_FOCUS_GAINED;assert(SDL_PushEvent(&e)==1);I_StartTic();
    before=count;key(SDL_SCANCODE_W,1,0);assert(count==before+1);
    I_ShutdownGraphics();SDL_Quit();puts("input: SDL keys, repeat suppression, arrows, keypad Enter, Tab and focus reset passed");
}
