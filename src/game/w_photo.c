// Convert the shared photo atlas into Clayfight's GRA frames and palettes.
#include "game/resources.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
struct photoset_s { photo_set *set; gra_t *gra; };
photoset_t *W_PhotoBegin(const char *name) {
    photoset_t *ps=I_Malloc(sizeof *ps);
    ps->set=photo_begin(name); ps->gra=I_Malloc(sizeof *ps->gra);
    snprintf(ps->gra->name,sizeof ps->gra->name,"photo:%.9s",name);
    return ps;
}
gra_t *W_PhotoGra(photoset_t *ps) { return ps->gra; }
int W_PhotoFrame(photoset_t *ps,const char *path) { return photo_add(ps->set,path); }
void W_PhotoFinish(photoset_t *ps,const char *backdrop,int reference,int height) {
    photo_atlas *a=photo_finish(ps->set,backdrop,reference,height,PIXELS_PER_UNIT);
    gra_t *g=ps->gra; g->numframes=a->count; g->numpalettes=3;
    g->frames=I_Malloc(a->count*sizeof *g->frames);
    g->palettes=I_Malloc(3*sizeof *g->palettes);
    for(int p=0;p<3;p++) {
        g->palettes[p].count=31;
        for(int i=1;i<=31;i++) {
            pixel_t c=a->colors[i]; int r=(c>>16)&255,gg=(c>>8)&255,b=c&255;
            g->palettes[p].colors[i]=p==0?c:p==1?W_RGB(b,r,gg):W_RGB(gg,b,r);
        }
    }
    for(int i=0;i<a->count;i++) {
        photo_frame *f=&a->frames[i]; patch_t *pt=I_Malloc(sizeof *pt);
        pt->w=f->w; pt->h=f->h; pt->x=f->x; pt->y=f->y;
        pt->pixels=f->pixels; pt->detail=f->detail;
        f->pixels=NULL; f->detail=NULL; // transfer ownership to the game resource
        pt->maxval=31; pt->hasbox=true;
        pt->bx0=-pt->x; pt->by0=-pt->y; pt->bx1=pt->w-1-pt->x; pt->by1=pt->h-1-pt->y;
        g->frames[i]=(frame_t){.w=pt->w,.h=pt->h,.x=pt->x,.y=pt->y,.decoded=true,.patch=pt};
    }
    photo_free(ps->set); ps->set=NULL;
    I_Log("W_PhotoFinish: %s: %d photos",g->name+6,g->numframes);
}
