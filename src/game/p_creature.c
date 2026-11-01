// p_creature.c -- load creature files: one fighter per text file
// (the role Doom gives to info.c + DeHackEd patches; format in
// docs/creature-format.md)

#include <ctype.h>
#include <dirent.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

#include "game/game.h"

fighterdef_t *roster[MAXROSTER];
int numroster;
static char loadedpaths[MAXROSTER][600];

#define MAXTOKENS 24

typedef struct {
    const char *path;
    int line;
    char *tok[MAXTOKENS];
    int ntok;
} parser_t;

static void P_Fail(const parser_t *ps, const char *fmt, ...) __attribute__((noreturn, format(printf, 2, 3)));
static void P_Fail(const parser_t *ps, const char *fmt, ...)
{
    char msg[256];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(msg, sizeof msg, fmt, ap);
    va_end(ap);
    I_Error("%s:%d: %s", ps->path, ps->line, msg);
}

// Split a line into tokens; '#' at the start of a word begins a comment
// (frame references like REXP1#6 contain '#' mid-word).
static void P_Tokenize(parser_t *ps, char *line)
{
    ps->ntok = 0;
    char *p = line;
    while (*p) {
        while (isspace((byte)*p))
            p++;
        if (!*p || *p == '#')
            break;
        if (ps->ntok == MAXTOKENS)
            P_Fail(ps, "line too long");
        ps->tok[ps->ntok++] = p;
        while (*p && !isspace((byte)*p))
            p++;
        if (*p)
            *p++ = 0;
    }
}

static float P_Num(const parser_t *ps, int i)
{
    if (i >= ps->ntok)
        P_Fail(ps, "'%s' needs a number", ps->tok[0]);
    char *end;
    float v = strtof(ps->tok[i], &end);
    if (*end)
        P_Fail(ps, "'%s' is not a number", ps->tok[i]);
    return v;
}

// "REXP1#6" -> S04REXP1.GRA frame 6 ("S04REXP1.GRA#6" also works)
static void P_FrameRef(const parser_t *ps, const char *tok, gra_t **gra, int *frame)
{
    const char *hash = strchr(tok, '#');
    if (!hash || hash == tok)
        P_Fail(ps, "bad frame '%s' (want FILE#index)", tok);
    char name[40];
    int n = (int)(hash - tok);
    if (n > 4 && !strncasecmp(hash - 4, ".GRA", 4))
        snprintf(name, sizeof name, "%.*s", n, tok);
    else
        snprintf(name, sizeof name, "S04%.*s.GRA", n, tok);
    *gra = W_TryLoadGra(name);
    if (!*gra)
        P_Fail(ps, "no art file %s", name);
    *frame = atoi(hash + 1);
    if (*frame < 0 || *frame >= (*gra)->numframes)
        P_Fail(ps, "%s has no frame %d", name, *frame);
}

static int P_MoveSlot(const parser_t *ps, const char *s)
{
    static const char *const groups[NUMMOVEGROUPS] = {"stand", "close", "crouch", "jump", "power", "air"};
    static const char *const buttons[4] = {"hq", "hf", "lq", "lf"};
    const char *dot = strchr(s, '.');
    if (dot && (!strcasecmp(dot + 1, "hp") || !strcasecmp(dot + 1, "lp"))) {
        for (int g = 0; g < 4; g++)
            if ((size_t)(dot - s) == strlen(groups[g]) && !strncasecmp(s, groups[g], dot - s))
                return MP_STAND + g * 2 + (!strcasecmp(dot + 1, "lp"));
    }
    if (dot) {
        for (int g = 0; g < NUMMOVEGROUPS; g++) {
            if ((size_t)(dot - s) != strlen(groups[g]) || strncasecmp(s, groups[g], dot - s))
                continue;
            for (int b = 0; b < 4; b++)
                if (!strcasecmp(dot + 1, buttons[b]))
                    return g * 4 + b;
        }
    }
    P_Fail(ps, "bad move slot '%s' (want e.g. stand.hq, crouch.lf, air.hf)", s);
}

static int P_AnimSlot(fighterdef_t *def, const parser_t *ps, const char *name, bool create)
{
    for (int i = 0; i < NUMSTDANIMS; i++)
        if (!strcasecmp(stdanimnames[i], name))
            return i;
    int i = P_FindAnim(def, name);
    if (i >= 0 || !create)
        return i;
    if (def->numanims == MAXANIMS)
        P_Fail(ps, "too many animations");
    i = def->numanims++;
    snprintf(def->anims[i].name, sizeof def->anims[i].name, "%s", name);
    def->anims[i].next = ANIM_NONE;
    def->anims[i].wanderfrom = -1;
    return i;
}

fighterdef_t *P_LoadCreature(const char *path)
{
    FILE *fp = fopen(path, "r");
    if (!fp)
        I_Error("can't open %s", path);
    fighterdef_t *def = I_Malloc(sizeof *def);
    def->codeindex = -1;
    def->facesleft = true;
    def->walkspeed = 1.5f;
    def->backspeed = 1.2f;
    def->jumpspeed = 7.5f;
    def->width = 40;
    def->numanims = NUMSTDANIMS;
    for (int i = 0; i < NUMSTDANIMS; i++) {
        snprintf(def->anims[i].name, sizeof def->anims[i].name, "%s", stdanimnames[i]);
        def->anims[i].next = ANIM_NONE;
        def->anims[i].wanderfrom = -1;
    }

    def->portrait = def->label = -1;
    def->altpalette = 1;
    photoset_t *photos = NULL; // photographed frames, processed at the end
    char dir[512], backdrop[600] = "", portraitpath[600] = "";
    int height = 84;
    bool photoart = false;
    snprintf(dir, sizeof dir, "%s", path);
    char *slash = strrchr(dir, '/');
    if (slash)
        *slash = 0;
    else
        snprintf(dir, sizeof dir, ".");

    int capstates = 256;
    def->states = I_Malloc(capstates * sizeof *def->states);
    char movenames[NUMMOVES][24] = {{0}};
    parser_t ps = {.path = path};
    char line[512];
    animinfo_t *anim = NULL; // inside "anim ... end"
    movedef_t *move = NULL; // inside "move ... end"

    while (fgets(line, sizeof line, fp)) {
        ps.line++;
        P_Tokenize(&ps, line);
        if (!ps.ntok)
            continue;
        const char *cmd = ps.tok[0];

        if (!strcasecmp(cmd, "end")) {
            if (!anim && !move)
                P_Fail(&ps, "'end' without 'anim' or 'move'");
            if (anim && !anim->count)
                P_Fail(&ps, "animation %s has no frames", anim->name);
            anim = NULL;
            move = NULL;
        } else if (anim) { // a frame: FILE#index tics [flip] [dx n]
            if (def->numstates == capstates) {
                capstates *= 2;
                state_t *grown = I_Malloc(capstates * sizeof *grown);
                memcpy(grown, def->states, def->numstates * sizeof *grown);
                free(def->states);
                def->states = grown;
            }
            state_t *st = &def->states[def->numstates++];
            size_t clen = strlen(cmd);
            if (clen > 4 && !strcasecmp(cmd + clen - 4, ".png")) { // a photo
                char full[1100];
                snprintf(full, sizeof full, "%s/%s", dir, cmd);
                if (!photos)
                    photos = W_PhotoBegin(def->name[0] ? def->name : path);
                st->sprite = W_PhotoGra(photos);
                st->frame = W_PhotoFrame(photos, full);
            } else {
                P_FrameRef(&ps, cmd, &st->sprite, &st->frame);
            }
            size_t len = ps.ntok > 1 ? strlen(ps.tok[1]) : 0;
            if (len > 1 && tolower((byte)ps.tok[1][len - 1]) == 's') { // "2s": stun shares
                ps.tok[1][len - 1] = 0;
                st->stunshares = P_Num(&ps, 1);
                st->tics = st->stunshares * 2;
            } else {
                st->tics = P_Num(&ps, 1);
            }
            for (int i = 2; i < ps.ntok; i++) {
                if (!strcasecmp(ps.tok[i], "flip"))
                    st->hflip = true;
                else if (!strcasecmp(ps.tok[i], "dx"))
                    st->dx = P_Num(&ps, ++i);
                else
                    P_Fail(&ps, "unknown frame option '%s'", ps.tok[i]);
            }
            anim->count++;
        } else if (move) { // box tic tics x0 y0 x1 y1
            if (strcasecmp(cmd, "box"))
                P_Fail(&ps, "expected 'box' or 'end' in a move, got '%s'", cmd);
            if (move->numboxes == MAXBOXES)
                P_Fail(&ps, "too many boxes");
            hitbox_t *b = &move->boxes[move->numboxes++];
            b->tic = P_Num(&ps, 1);
            b->tics = (int)P_Num(&ps, 2);
            b->x0 = (int)P_Num(&ps, 3);
            b->y0 = (int)P_Num(&ps, 4);
            b->x1 = (int)P_Num(&ps, 5);
            b->y1 = (int)P_Num(&ps, 6);
        } else if (!strcasecmp(cmd, "anim")) {
            if (ps.ntok < 2)
                P_Fail(&ps, "anim needs a name");
            int slot = P_AnimSlot(def, &ps, ps.tok[1], true);
            anim = &def->anims[slot];
            if (anim->count)
                P_Fail(&ps, "animation %s defined twice", anim->name);
            anim->first = def->numstates;
            for (int i = 2; i < ps.ntok; i++) {
                const char *o = ps.tok[i];
                bool from = i + 1 < ps.ntok && !strcasecmp(ps.tok[i + 1], "from");
                if (!strcasecmp(o, "hold")) {
                    anim->hold = true;
                } else if (!strcasecmp(o, "loop")) {
                    anim->loop = true;
                    anim->loopfrom = from ? (int)P_Num(&ps, i + 2) : 0;
                    i += from ? 2 : 0;
                } else if (!strcasecmp(o, "wander")) {
                    anim->wanderfrom = from ? (int)P_Num(&ps, i + 2) : 0;
                    i += from ? 2 : 0;
                } else if (!strcasecmp(o, "stun")) {
                    anim->stun = (int)P_Num(&ps, ++i);
                } else if (!strcasecmp(o, "next")) {
                    if (++i >= ps.ntok)
                        P_Fail(&ps, "next needs an animation");
                    anim->next = P_AnimSlot(def, &ps, ps.tok[i], true);
                } else {
                    P_Fail(&ps, "unknown animation option '%s'", o);
                }
            }
        } else if (!strcasecmp(cmd, "move")) {
            if (ps.ntok < 3)
                P_Fail(&ps, "move needs a slot and an animation");
            int slot = P_MoveSlot(&ps, ps.tok[1]);
            move = &def->moves[slot];
            *move = (movedef_t){.valid = true, .attack = {.damage = 5, .level = HIGH}};
            snprintf(movenames[slot], sizeof movenames[slot], "%s", ps.tok[2]);
            int strength = 1;
            for (int i = 3; i < ps.ntok; i++) {
                const char *o = ps.tok[i];
                if (!strcasecmp(o, "damage"))
                    move->attack.damage = (int)P_Num(&ps, ++i);
                else if (!strcasecmp(o, "stun"))
                    move->stun = (int)P_Num(&ps, ++i);
                else if (!strcasecmp(o, "strength"))
                    strength = (int)P_Num(&ps, ++i);
                else if (!strcasecmp(o, "push"))
                    move->attack.push = P_Num(&ps, ++i);
                else if (!strcasecmp(o, "knockdown"))
                    move->attack.knockdown = true;
                else if (!strcasecmp(o, "level") && i + 1 < ps.ntok)
                    move->attack.level = !strcasecmp(ps.tok[++i], "low") ? LOW : HIGH;
                else
                    P_Fail(&ps, "unknown move option '%s'", o);
            }
            if (!move->attack.push)
                move->attack.push = 1.5f + strength;
        } else if (!strcasecmp(cmd, "name")) {
            snprintf(def->name, sizeof def->name, "%s", ps.ntok > 1 ? ps.tok[1] : "?");
        } else if (!strcasecmp(cmd, "art")) { // art PREFIX|photo [left|right]
            if (ps.ntok < 2)
                P_Fail(&ps, "art needs a file prefix or 'photo'");
            photoart = !strcasecmp(ps.tok[1], "photo");
            snprintf(def->prefix, sizeof def->prefix, "%s", photoart ? "" : ps.tok[1]);
            def->facesleft = ps.ntok < 3 || strcasecmp(ps.tok[2], "right");
        } else if (!strcasecmp(cmd, "height")) { // photos: stand pose height in pixels
            height = (int)P_Num(&ps, 1);
        } else if (!strcasecmp(cmd, "backdrop")) { // photos: a shot of the empty scene
            if (ps.ntok < 2)
                P_Fail(&ps, "backdrop needs a .png file");
            snprintf(backdrop, sizeof backdrop, "%s/%s", dir, ps.tok[1]);
        } else if (!strcasecmp(cmd, "portrait")) {
            if (ps.ntok < 2) P_Fail(&ps, "portrait needs a PNG file");
            snprintf(portraitpath, sizeof portraitpath, "%s/%s", dir, ps.tok[1]);
        } else if (!strcasecmp(cmd, "palette")) { // palette N [alt M]
            def->palette = def->altpalette = (int)P_Num(&ps, 1);
            if (ps.ntok > 3 && !strcasecmp(ps.tok[2], "alt"))
                def->altpalette = (int)P_Num(&ps, 3);
        } else if (!strcasecmp(cmd, "select")) {
            def->portrait = (int)P_Num(&ps, 1);
            def->label = (int)P_Num(&ps, 2);
        } else if (!strcasecmp(cmd, "speed")) {
            def->walkspeed = P_Num(&ps, 1);
            def->backspeed = P_Num(&ps, 2);
            def->jumpspeed = P_Num(&ps, 3);
        } else if (!strcasecmp(cmd, "width")) {
            def->width = (int)P_Num(&ps, 1);
        } else {
            P_Fail(&ps, "unknown command '%s'", cmd);
        }
    }
    fclose(fp);
    if (anim || move)
        P_Fail(&ps, "missing 'end'");
    if (!def->name[0] || (!def->prefix[0] && !photoart))
        I_Error("%s: needs 'name' and 'art'", path);

    for (int m = 0; m < NUMMOVES; m++) {
        if (!def->moves[m].valid)
            continue;
        int a = P_AnimSlot(def, &ps, movenames[m], false);
        if (a < 0 || !def->anims[a].count)
            I_Error("%s: a move uses animation '%s', which isn't defined", path, movenames[m]);
        def->moves[m].anim = a;
    }
    if (photos) {
        // the stand pose's first photo sets the scale for every frame
        const state_t *ref = &def->states[def->anims[ANIM_STAND].first];
        W_PhotoFinish(photos, backdrop[0] ? backdrop : NULL, ref->sprite == W_PhotoGra(photos) ? ref->frame : 0, height);
        def->base = W_PhotoGra(photos);
    } else {
        char base[32];
        snprintf(base, sizeof base, "S04%s.GRA", def->prefix);
        def->base = W_LoadGra(base);
    }
    if (portraitpath[0]) {
        photoset_t *port = W_PhotoBegin("portrait");
        W_PhotoFrame(port, portraitpath);
        W_PhotoFinish(port, NULL, 0, 100);
        def->photoport = W_PhotoGra(port);
    }
    P_FinishFighter(def);
    return def;
}

static int P_CompareNames(const void *a, const void *b)
{
    const char *x = strrchr(*(char *const *)a, '/'), *y = strrchr(*(char *const *)b, '/');
    return strcasecmp(x ? x + 1 : *(char *const *)a, y ? y + 1 : *(char *const *)b);
}

// Collect *.txt in dir, and in its subfolders when depth > 0.
static void P_FindCreatures(const char *dir, int depth, char **names, int *n)
{
    DIR *d = opendir(dir);
    if (!d)
        return;
    struct dirent *e;
    while ((e = readdir(d)) && *n < MAXROSTER) {
        if (e->d_name[0] == '.')
            continue;
        char path[600];
        snprintf(path, sizeof path, "%s/%s", dir, e->d_name);
        size_t len = strlen(e->d_name);
        if (len > 4 && !strcasecmp(e->d_name + len - 4, ".txt")) {
            names[(*n)++] = strdup(path);
        } else if (depth > 0) {
            DIR *sub = opendir(path);
            if (sub) {
                closedir(sub);
                P_FindCreatures(path, depth - 1, names, n);
            }
        }
    }
    closedir(d);
}

bool P_RegisterCreature(fighterdef_t *def, const char *path)
{
    if (!def || numroster >= MAXROSTER) return false;
    for (int i = 0; i < numroster; i++)
        if (!strcmp(loadedpaths[i], path)) return false;
    snprintf(loadedpaths[numroster], sizeof loadedpaths[0], "%s", path);
    roster[numroster++] = def;
    return true;
}

void P_RefreshRoster(const char *dir)
{
    char *names[MAXROSTER];
    int n = 0;
    P_FindCreatures(dir, 1, names, &n);
    if (!n)
        I_Error("No creature files in %s.\nAdd a photographed creature to the workshop.", dir);
    qsort(names, n, sizeof names[0], P_CompareNames);
    for (int i = 0; i < n; i++) {
        bool loaded = false;
        for (int j = 0; j < numroster; j++)
            if (!strcmp(loadedpaths[j], names[i])) loaded = true;
        if (!loaded && numroster < MAXROSTER) {
            fighterdef_t *def = P_LoadCreature(names[i]);
            P_RegisterCreature(def, names[i]);
        }
        free(names[i]);
    }
    I_Log("P_LoadRoster: %d creatures from %s", numroster, dir);
}

static int P_RosterOrder(const void *a, const void *b)
{
    const fighterdef_t *x = *(fighterdef_t *const *)a, *y = *(fighterdef_t *const *)b;
    int ix = x->codeindex < 0 ? 100 : x->codeindex;
    int iy = y->codeindex < 0 ? 100 : y->codeindex;
    return ix != iy ? ix - iy : strcasecmp(x->name, y->name);
}

// Original fighters are assembled from the arcade's own script/move tables.
// The sampled state lists serve previews and silhouette inspection. Combat
// executes the ROM bytecode directly, including every source frame.
static void P_ArcadeAnim(fighterdef_t *d,int slot,int row)
{
    animinfo_t *a=&d->anims[slot];a->first=d->numstates;a->next=ANIM_STAND;a->wanderfrom=-1;
    psanim_t vm={0};PS_Start(&vm,PS_RowScript(d->codeindex,row),PS_RowDuration(row));
    if(slot==ANIM_STAND || slot==ANIM_CROUCH){
        int n=slot==ANIM_STAND?30:20;
        for(int i=0;i<n;i++)d->states[d->numstates++]=(state_t){.sprite=d->base,.frame=vm.frameid+i,.tics=3};
        a->count=n;a->next=ANIM_NONE;a->wanderfrom=0;return;
    }
    int previous=-1;
    for(int tic=0;tic<1200 && a->count<128;tic++){
        if(vm.step!=previous && vm.sprite){
            if(d->numstates>=8192)I_Error("Arcade preview state budget exceeded");
            d->states[d->numstates++]=(state_t){.sprite=vm.sprite,.frame=vm.frame,.hflip=vm.hflip,.tics=MAX(vm.timer,1)};
            a->count++;previous=vm.step;
        }
        if(vm.done)break;
        PS_Tick(&vm);
    }
    if(!a->count)I_Error("Arcade %s row %d has no frame",d->name,row);
    a->hold=slot==ANIM_DOWN || slot==ANIM_JUMP;
    if(a->hold)a->next=ANIM_NONE;
}
static void P_LoadArcadeRoster(void)
{
    static const char *const names[]={"SAURON","BLIZZARD","TALON","VERTIGO","ARMADON","DIABLO","CHAOS"};
    static const char *const prefixes[]={"REX","KON","TAL","COB","SPI","DIA","HAV"};
    static const int portraits[]={130,91,141,149,125,113,97};
    static const int rows[]={0,7,10,1,4,13,38,40,42,43,44,46,52,13,54,58};
    static const int widths[]={54,49,44,49,59,54,47};
    for(int c=0;c<7;c++){
        fighterdef_t *d=I_Malloc(sizeof *d);snprintf(d->name,sizeof d->name,"%s",names[c]);snprintf(d->prefix,sizeof d->prefix,"%s",prefixes[c]);
        d->codeindex=c;d->portrait=portraits[c];d->label=-1;d->facesleft=true;d->base=W_LoadGra("arcade");
        d->palette=c*4;d->altpalette=c*4+1;d->width=widths[c];d->walkspeed=c==2?2.47f:c==4?1.6f:1.97f;d->backspeed=d->walkspeed*.75f;d->jumpspeed=10.6f;
        d->numanims=NUMSTDANIMS+NUMMOVES;d->states=I_Malloc(8192*sizeof *d->states);
        for(int a=0;a<NUMSTDANIMS;a++){snprintf(d->anims[a].name,sizeof d->anims[a].name,"%s",stdanimnames[a]);P_ArcadeAnim(d,a,rows[a]);}
        for(int m=0;m<NUMMOVES;m++){
            psmove_t move;if(!PS_Move(c,m,&move))continue;
            int a=NUMSTDANIMS+m;snprintf(d->anims[a].name,sizeof d->anims[a].name,"move_%d",m);P_ArcadeAnim(d,a,move.row);
            d->moves[m]=(movedef_t){.valid=true,.anim=a,.stun=move.stun,.attack={.damage=move.damage,.level=move.hitclass==4 || move.hitclass==5?LOW:HIGH,.push=1.5f+move.strength,.knockdown=move.strength>=3}};
        }
        P_FinishFighter(d);char path[64];snprintf(path,sizeof path,"arcade://%s",names[c]);P_RegisterCreature(d,path);
    }
}

void P_LoadRoster(const char *dir)
{
    if (!PS_Active() && !PS_Init()) I_Error("Arcade script table unavailable");
    p_scripted=true;
    if (!numroster) P_LoadArcadeRoster();
    P_RefreshRoster(dir);
    // Preserve original seven-character order, then the clay roster.
    // Paths stay in load order; refresh only uses them as a membership set.
    qsort(roster, numroster, sizeof roster[0], P_RosterOrder);
}
