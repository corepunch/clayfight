// Creation lobby; the browser companion handles phone cameras and persistence.
#include "game/game.h"
void M_Create(void) { gamestate = GS_CREATE; G_ClearInput(); }
void M_CreateTicker(void) { }
bool M_CreateResponder(const event_t *ev)
{
    if (ev->type == ev_keydown && (ev->key == KEY_ESCAPE || ev->key == PADKEY(0, PK_BACK))) {
        M_StartMenu(); return true;
    }
    return false;
}
void M_CreateDrawer(void)
{
    M_Background("CREATE FIGHTER");
    V_DrawTextCentered(FONT_SMALL, 160, 80, "YOUR CLAY FIGURE IS ALREADY A FIGHTER.");
    V_DrawTextCentered(FONT_SMALL, 160, 100, "NEXT: PHOTOGRAPH ITS POSES AND PORTRAIT.");
}
