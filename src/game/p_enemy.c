// p_enemy.c -- Primal Rage PC's matchup/distance/difficulty AI.
// Addresses refer to the relocated PRAGE.EXE, never to host memory. See
// docs/primal-rage-ai.md and tests/ai_original.py for original-byte oracles.
#include <math.h>
#include <stddef.h>
#include <stdio.h>

#include "game/game.h"

#define MATCHUPS 0xE6444
#define MATCHUP_NAMES 0xE65D4
#define WEIGHTS 0xE6764

typedef struct {
    uint32_t macros, matrices, macro_names, matrix_names;
    int nmacros, nmatrices;
} aitable_t;

// Immediate operands in the switches at 0x4748C and 0x47314, character order.
static const aitable_t tables[7] = {
    {0xCB018, 0xCB194, 0xBBAB8, 0xBBC34, 95, 100},
    {0x9A434, 0x9A63C, 0x8AD14, 0x8AF1C, 130, 117},
    {0xE0AB0, 0xE0CB0, 0xDC6D8, 0xDC8D8, 128, 87},
    {0xE5180, 0xE5328, 0xE1D40, 0xE1EE8, 106, 122},
    {0xDB490, 0xDB668, 0xCC258, 0xCC430, 118, 79},
    {0xBA8C4, 0xBA9F8, 0xAB958, 0xABA8C, 77, 99},
    {0xAA710, 0xAA870, 0x9B744, 0x9B8A4, 88, 109},
};

typedef char ai_layout_must_match[(sizeof(prai_t) == 64 && offsetof(prai_t, step) == 0x30) ? 1 : -1];

bool PA_Init(void)
{
    if (!X_Loaded() || X_U32(0x474C3) != tables[0].macros ||
        X_U32(0x47342) != tables[0].matrices || !X_Valid(WEIGHTS, 240))
        return false;
    for (int c = 0; c < 7; c++) {
        const aitable_t *t = &tables[c];
        for (int i = 0; i < t->nmacros; i++) {
            uint32_t p = X_U32(t->macros + i * 4);
            bool end = false;
            for (int j = 0; j < 256 && X_Valid(p + j * 8, 8); j++) {
                if (X_U32(p + j * 8) == 0xFFFF) { end = true; break; }
            }
            if (!end) return false;
        }
        for (int i = 0; i < t->nmatrices; i++) {
            uint32_t p = X_U32(t->matrices + i * 4);
            if (!X_Valid(p, 10)) return false;
            for (int j = 0; j < 10; j++)
                if (X_U8(p + j) >= t->nmacros) return false;
        }
        for (int o = 0; o < 7; o++) {
            uint32_t p = X_U32(MATCHUPS + (c * 10 + o) * 4);
            if (!X_Valid(p, 139 * 4)) return false;
            for (int k = 0; k < 139; k++)
                if (!X_Valid(X_U32(p + k * 4), 80)) return false;
        }
    }
    return true;
}

// 0x46EE8 and predicates 0x46BD8..0x46EE5. The caller observes both actors
// before either command is applied. The original clears event bit 0 each tic.
void PA_Observe(prai_t *a, const prai_view_t *opp)
{
    a->oldopponent = a->opponent;
    if ((opp->action == 7 || opp->action == 8) && opp->move < 64) {
        a->opponent = a->oldopponent == 0 || a->oldopponent == 1 ? 1 : opp->incoming ? 13 : 0;
    } else if (opp->move == 255 && !opp->stance && !opp->action && !opp->motion) {
        a->opponent = 2;
    } else if (opp->move == 255 && opp->stance == 1 && !opp->action &&
               (opp->motion == 5 || opp->motion == 21)) {
        a->opponent = 3;
    } else if (opp->move == 255 && opp->stance == 2 && !opp->action && opp->motion == 4) {
        a->opponent = 4;
    } else if (!opp->stance && !opp->action && opp->motion == 1) {
        a->opponent = opp->forward ? 5 : 6;
    } else if ((opp->special_idle && opp->stance != 2) || opp->motion == 7) {
        a->opponent = 7;
    } else if (opp->action == 11) {
        a->opponent = 9;
    } else if (opp->action == 6) {
        a->opponent = 11;
    } else if (opp->special_idle && opp->stance == 2) {
        a->opponent = 12;
    } else {
        a->opponent = 8;
    }
}

// 0x471B8 / 0x470FC: select one of 139 opponent-context rows for this matchup.
void PA_Response(prai_t *a, int character, int opponent, const prai_view_t *opp)
{
    int context;
    switch (a->opponent) {
    case 0: context = opp->move; break;
    case 1: context = opp->started ? 0x88 : 0x40 + opp->move; break;
    case 2: context = 0x82; break;
    case 3: context = 0x85; break;
    case 4: context = 0x87; break;
    case 5: context = 0x83; break;
    case 6: context = 0x84; break;
    case 7: context = 0x81; break;
    case 9: context = 0x86; break;
    case 11: context = opp->special == 255 ? 0x88 : opp->special; break;
    case 12: context = 0x89; break;
    case 13: context = 0x8A; break;
    default: context = 0x88; break;
    }
    // Invalid observations never index past the executable's tables.
    context = CLAMP(context, 0, 138);
    int pair = character * 10 + opponent;
    a->context = context;
    a->response = X_U32(X_U32(MATCHUPS + pair * 4) + context * 4);
    a->response_name = X_U32(X_U32(MATCHUP_NAMES + pair * 4) + context * 4);
    a->previous = a->opponent;
}

// 0x47314: inclusive distance bands, first match wins (not a random roll).
// distance64 is the signed native X separation: 64 subunits per arcade unit.
void PA_Matrix(prai_t *a, int character, int distance64)
{
    int64_t d = distance64;
    int distance = (int)MIN((d < 0 ? -d : d) / 64, 255);
    uint32_t p = a->response, chosen = p;
    for (int i = 0; i < 20; i++, p += 4) {
        if (distance >= X_U8(p + 2) && distance <= X_U8(p + 3)) {
            chosen = p;
            break;
        }
    }
    // The native loop really scans 20 records, even across a band's final
    // hi=255 record. Preserve its gap/overlap behavior instead of normalizing.
    a->matrixid = X_U8(chosen);
    a->profile = X_U8(chosen + 1);
    a->matrix = X_U32(tables[character].matrices + a->matrixid * 4);
    a->matrix_name = X_U32(tables[character].matrix_names + a->matrixid * 4);
}

// 0x4748C after 0x47314: ten unsigned macro IDs; signed percentage weights.
// roll is the original random(100) result, 0..99. Its PIT entropy source is
// deliberately supplied by the caller so tests can compare identical draws.
void PA_Macro(prai_t *a, int character, int difficulty, int roll)
{
    a->weights = WEIGHTS + a->profile * 80 + CLAMP(difficulty, 0, 7) * 10;
    int sum = 0, column = 9;
    for (int i = 0; i < 9; i++) {
        int weight = X_S8(a->weights + i);
        if (weight && roll <= (sum += weight)) { column = i; break; }
    }
    a->column = column;
    a->macroid = X_U8(a->matrix + column);
    a->macro = X_U32(tables[character].macros + a->macroid * 4);
    a->macro_name = X_U32(tables[character].macro_names + a->macroid * 4);
}

// 0x3CEB8: retain both edge (low byte) and held (high byte) channels.
uint16_t PA_Input(uint32_t pattern, bool faceleft)
{
    uint32_t out = pattern;
    if (pattern & 0x10000) out |= faceleft ? 0x20 : 0x10;
    if (pattern & 0x40000) out |= faceleft ? 0x10 : 0x20;
    if (pattern & 0x20000) out |= faceleft ? 0x2000 : 0x1000;
    if (pattern & 0x80000) out |= faceleft ? 0x1000 : 0x2000;
    return out & 0xFFFF;
}

// 0x47638 + playback in 0x47E5C. step/timer are intentionally bytes: the
// original compares before incrementing, including a neutral terminator tic.
uint32_t PA_Tick(prai_t *a, int character, int opponent, const prai_view_t *self,
                 const prai_view_t *opp, int distance64, int difficulty, int roll)
{
    PA_Observe(a, opp);
    if (self->stance != 2 && (a->opponent == 0 ||
        ((a->opponent == 7 || a->opponent == 9) && a->opponent != a->previous)))
        a->active = 0;
    if (self->action == 0 && !a->active) {
        PA_Response(a, character, opponent, opp);
        PA_Matrix(a, character, distance64);
        PA_Macro(a, character, difficulty, roll);
        a->ticks = 0;
        a->active = 1;
        a->step = a->steptics = 0;
        a->decisions++;
    }
    uint32_t p = a->macro + a->step * 8;
    uint32_t input = X_U32(p);
    if (input == 0xFFFF) {
        a->active = 0;
        input = 0;
    } else if ((int)a->steptics >= X_S16(p + 4)) {
        a->steptics = 0;
        a->step++;
    }
    a->steptics++;
    a->ticks++;
    return input;
}

#ifndef CLAYFIGHT_AI_ORACLE
static bool ready;

static int P_AiCharacter(const fighterdef_t *def)
{
    return CLAMP(def->aiindex >= 0 ? def->aiindex : def->codeindex >= 0 ? def->codeindex : 0, 0, 6);
}

static prai_view_t P_AiView(const fighter_t *f)
{
    prai_view_t v = {.move = 255, .special = 255};
    v.stance = P_IsAirborne(f) ? 2 :
        (P_InAnim(f, ANIM_CROUCH) || P_InAnim(f, ANIM_CROUCHBLOCK) ||
         P_InAnim(f, ANIM_CROUCHHIT) || (f->move >= 0 && f->move / 4 == MG_CROUCH)) ? 1 : 0;
    if (f->attack) {
        v.action = 8;
        v.motion = v.stance == 2 ? 4 : 9;
        v.move = f->move;
        v.started = !f->attackhit; // original +0x8A is cleared when a hit resolves
    } else if (P_InAnim(f, ANIM_DIZZY)) {
        v.motion = 7;
    } else if (P_InAnim(f, ANIM_GETUP)) {
        v.action = 11;
    } else if (P_InAnim(f, ANIM_BLOCK) || P_InAnim(f, ANIM_CROUCHBLOCK)) {
        v.action = 6;
    } else if (f->state->kind == K_HIT || f->state->kind == K_DOWN || f->state->kind == K_SCRIPT) {
        v.action = 3;
    } else if (v.stance == 2) {
        v.motion = 4;
    } else if (P_InAnim(f, ANIM_CROUCH)) {
        v.motion = 5;
        v.stance = 1;
    } else if (P_InAnim(f, ANIM_WALKF) || P_InAnim(f, ANIM_WALKB)) {
        v.motion = 1;
        v.forward = P_InAnim(f, ANIM_WALKF);
    }
    return v;
}

static int P_AiButtons(int bits)
{
    int out = 0;
    const int mapping[] = {BT_HQ, BT_HF, BT_LQ, BT_LF, BT_RIGHT, BT_LEFT, BT_DOWN, BT_UP};
    for (int i = 0; i < 8; i++) if (bits & (1 << i)) out |= mapping[i];
    return out;
}

void P_CpuCmd(fighter_t *f, const fighter_t *opp, ticcmd_t *cmd)
{
    if (!ready) {
        char path[640];
        snprintf(path, sizeof path, "%s/PRAGE.EXE", M_ParmValue("-data", "primalrage"));
        if ((!X_Loaded() && !X_Init(path)) || !PA_Init())
            I_Error("CPU AI needs the supported Primal Rage PC PRAGE.EXE in -data");
        ready = true;
        I_Log("CPU AI: original per-fighter matchup tables and joystick macros");
    }
    prai_view_t self = P_AiView(f), other = P_AiView(opp);
    *cmd = (ticcmd_t){.explicit_edges = true};
    if (f->dead || opp->dead || level.phase != PH_FIGHT || level.hitstop ||
        (gamemode == GM_TRAINING && f->player == 1)) return;
    // Original random(100) is hardware-timer based. Supply deterministic host
    // entropy, consuming it only when the original would select a new macro.
    prai_t observed = f->ai;
    PA_Observe(&observed, &other);
    bool select = !f->ai.active || (self.stance != 2 && (observed.opponent == 0 ||
        ((observed.opponent == 7 || observed.opponent == 9) && observed.opponent != f->ai.previous)));
    int roll = select && self.action == 0 ? P_Random() * 100 / 256 : 0;
    int character = P_AiCharacter(f->def), opponent = P_AiCharacter(opp->def);
    uint32_t pattern = PA_Tick(&f->ai, character, opponent, &self, &other,
            (int)((f->x - opp->x) * 64 / ARCADE_XSCALE), (options.difficulty - 1) * 3, roll);
    uint16_t raw = pattern == 0xFFFF ? 0 : PA_Input(pattern, f->facing < 0);
    cmd->buttons = P_AiButtons(raw >> 8);
    cmd->pressed = P_AiButtons(raw & 0xFF);
}
#endif
