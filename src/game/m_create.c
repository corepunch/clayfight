// Creation lobby. The phone companion saves ordinary creature files, which
// join the roster immediately when the player returns from the browser.
#include <stdio.h>
#include <string.h>
#include "game/game.h"
static bool launched, browseropened;
static int createtics;
void M_Create(void)
{
    gamestate = GS_CREATE; G_ClearInput(); createtics = 0;
    browseropened = false;
    launched = false; // start on first live tic, so headless QA never opens a browser
}
void M_CreateTicker(void)
{
    if (M_CheckParm("-shots") || M_CheckParm("-checkflow")) return;
    if (!launched) {
        I_StartCreator(M_ParmValue("-creatures", "data/creatures"), M_ParmValue("-data", "data/primrage"));
        launched = true;
    }
    createtics++;
    if (!browseropened && I_CreatorStatus() == 1) { I_OpenCreator(); browseropened = true; }
}
bool M_CreateResponder(const event_t *ev)
{
    if (ev->type != ev_keydown) return false;
    if (ev->key == KEY_ESCAPE || ev->key == PADKEY(0, PK_BACK)) { M_StartMenu(); return true; }
    int pl, bit;
    if (G_Binding(ev->key, &pl, &bit) && (bit & (BT_START | BT_ATTACKS))) {
        SEL_Start(); return true;
    }
    if (ev->key == 'b') { I_OpenCreator(); return true; }
    return false;
}
void M_CreateDrawer(void)
{
    M_Background("CREATE FIGHTER");
    V_DrawTextCentered(FONT_SMALL, 160, 58, "YOUR CLAY FIGURE IS ALREADY A FIGHTER.");
#ifdef CLAY_IOS
    V_DrawTextCentered(FONT_SMALL, 160, 84, "THE CAMERA OPENS HERE ON YOUR DEVICE");
    V_DrawTextCentered(FONT_SMALL, 160, 106, "FOLLOW EACH POSE AND TAKE A PORTRAIT");
    V_DrawTextCentered(FONT_SMALL, 160, 132, "YOUR PHOTOS AND VIDEOS STAY IN THE APP");
#else
    V_DrawTextCentered(FONT_SMALL, 160, 77, "1. OPEN THE CAPTURE WORKSHOP IN A BROWSER");
    V_DrawTextCentered(FONT_SMALL, 160, 92, "2. PHOTOGRAPH 15 POSES + A PORTRAIT");
    V_DrawTextCentered(FONT_SMALL, 160, 107, "3. SAVE YOUR FIGHTER AND RETURN HERE");
    if (I_CreatorStatus() == 1) {
        V_DrawTextCentered(FONT_SMALL, 160, 131, "PHONE / IPAD: SAME WI-FI AS THIS COMPUTER");
        const char *url = I_CreatorURL(true), *query = strchr(url, '?');
        char address[128];
        snprintf(address, sizeof address, "%.*s", query ? (int)(query - url) : (int)strlen(url), url);
        V_DrawTextCentered(FONT_SMALL, 160, 148, address);
        V_DrawTextCentered(FONT_SMALL, 160, 161, query ? query : "");
    } else {
        V_DrawTextCentered(FONT_SMALL, 160, 140, createtics < 180 ? "OPENING YOUR LOCAL WORKSHOP..." : "CHECK BUILD/CAPTURE.LOG IF IT DOES NOT OPEN");
        V_DrawTextCentered(FONT_SMALL, 160, 157, "MANUAL: PYTHON3 TOOLS/CAPTURE_SERVER.PY");
    }
#endif
    V_DrawTextCentered(FONT_SMALL, 160, 176, "ENTER: CHOOSE YOUR FIGHTER");
}
