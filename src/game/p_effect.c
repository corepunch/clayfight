// Native hit/block contact animations; no square-particle replacement art.
#include "game/game.h"

void P_SpawnSpark(float x,float z,sparkkind_t kind)
{
    if(level.numparticles==MAXPARTICLES)return;
    particle_t *p=&level.particles[level.numparticles++];
    *p=(particle_t){.x=x,.z=z,.blocked=kind==SPARK_BLOCK};
    // All standard arcade moves select 56e28 from their move metadata.
    // The blocked-contact script 56e36 switches to original colour bank 180.
    PS_StartScenery(&p->animation,p->blocked?0x56e36:0x56e28,3,0x51f15e);
}
void P_RunParticles(void)
{
    int n=0;
    for(int i=0;i<level.numparticles;i++){
        particle_t *p=&level.particles[i];
        PS_Tick(&p->animation);
        if(!p->animation.done)level.particles[n++]=*p;
    }
    level.numparticles=n;
}
