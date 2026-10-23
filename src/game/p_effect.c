// p_effect.c -- impact particles (cosmetic; cf. the puff/blood things in p_mobj.c)

#include <math.h>

#include "game/game.h"

static const struct {
    int count;
    float speed;
    pixel_t colors[4];
} sparks[] = {
    [SPARK_HIT] = {10, 2.2f, {0xffd02020, 0xffa01010, 0xffffa060, 0xffd02020}},
    [SPARK_BIG] = {18, 3.0f, {0xffe02020, 0xffa01010, 0xffffd080, 0xffffffff}},
    [SPARK_BLOCK] = {8, 1.8f, {0xffffe040, 0xffffffff, 0xffff9020, 0xffffe040}},
};

void P_SpawnSpark(float x, float z, sparkkind_t kind)
{
    for (int i = 0; i < sparks[kind].count && level.numparticles < MAXPARTICLES; i++) {
        float a = M_Random() / 256.f * 6.2831853f;
        float s = sparks[kind].speed * (0.4f + M_Random() / 256.f);
        level.particles[level.numparticles++] = (particle_t){
            .x = x, .z = z,
            .vx = cosf(a) * s,
            .vz = sinf(a) * s + 1,
            .life = 14 + (M_Random() & 15),
            .color = sparks[kind].colors[i & 3],
        };
    }
}

void P_RunParticles(void)
{
    int n = 0;
    for (int i = 0; i < level.numparticles; i++) {
        particle_t *p = &level.particles[i];
        p->x += p->vx;
        p->z += p->vz;
        p->vz -= 0.25f;
        if (--p->life > 0 && p->z > -4)
            level.particles[n++] = *p;
    }
    level.numparticles = n;
}
