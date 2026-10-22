// w_exe.c -- read data tables straight out of PRAGE.EXE (cf. w_wad.c)
//
// PRAGE.EXE layout (Watcom C/386, bound behind a DOS/4GW Professional stub):
//   MZ stub -> "BW" DOS/16M header (u32 at +0x1C = offset of the inner MZ)
//   inner MZ -> e_lfanew -> LE header
//   LE object 1: code, linked at 0x10000;  object 2: data, linked at 0x80000
// The raw pages do not hold final pointer values: internal fixups (source
// types 7 = 32-bit offset, 8 = 32-bit self-relative) must be applied before
// any pointer stored in a table can be followed.

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "pr_def.h"
#include "i_system.h"
#include "w_exe.h"

#define MAXOBJS 4

typedef struct {
    uint32_t base, size;
    byte *data;
} leobj_t;

static leobj_t objs[MAXOBJS];
static int numobjs;

static uint16_t rd16(const byte *p) { return p[0] | p[1] << 8; }
static uint32_t rd32(const byte *p) { return p[0] | p[1] << 8 | p[2] << 16 | (uint32_t)p[3] << 24; }

static void wr32(byte *p, uint32_t v)
{
    p[0] = v;
    p[1] = v >> 8;
    p[2] = v >> 16;
    p[3] = v >> 24;
}

bool X_Loaded(void)
{
    return numobjs > 0;
}

bool X_Init(const char *path)
{
    FILE *fp = fopen(path, "rb");
    if (!fp)
        return false;
    fseek(fp, 0, SEEK_END);
    long size = ftell(fp);
    fseek(fp, 0, SEEK_SET);
    byte *d = I_Malloc(size);
    bool ok = fread(d, 1, size, fp) == (size_t)size;
    fclose(fp);

#define NEED(cond) do { if (!(cond)) { ok = false; goto done; } } while (0)
    NEED(ok && size > 64 && d[0] == 'M' && d[1] == 'Z');
    uint32_t bw = (rd16(d + 4) - 1) * 512 + rd16(d + 2);
    NEED(bw + 0x20 < (uint32_t)size && d[bw] == 'B' && d[bw + 1] == 'W');
    uint32_t mz = rd32(d + bw + 0x1C);
    NEED(mz + 0x40 < (uint32_t)size && d[mz] == 'M' && d[mz + 1] == 'Z');
    uint32_t le = mz + rd32(d + mz + 0x3C);
    NEED(le + 0x84 < (uint32_t)size && d[le] == 'L' && d[le + 1] == 'E');

    uint32_t npages = rd32(d + le + 0x14), pagesize = rd32(d + le + 0x28), lastpage = rd32(d + le + 0x2C);
    uint32_t objtab = rd32(d + le + 0x40), nobj = rd32(d + le + 0x44), optab = rd32(d + le + 0x48);
    uint32_t fixpage = rd32(d + le + 0x68), fixrec = rd32(d + le + 0x6C), datapages = rd32(d + le + 0x80) + mz;
    NEED(nobj >= 2 && nobj <= MAXOBJS && pagesize == 4096);

    // object table: vsize, base, flags, first page index (1-based), page count
    uint32_t pidx[MAXOBJS], pcnt[MAXOBJS];
    for (uint32_t i = 0; i < nobj; i++) {
        const byte *o = d + le + objtab + 24 * i;
        objs[i].size = rd32(o);
        objs[i].base = rd32(o + 4);
        pidx[i] = rd32(o + 12);
        pcnt[i] = rd32(o + 16);
        objs[i].data = I_Malloc(MAX(objs[i].size, pcnt[i] * pagesize) + pagesize);
        for (uint32_t k = 0; k < pcnt[i]; k++) {
            const byte *pe = d + le + optab + 4 * (pidx[i] - 1 + k);
            uint32_t pn = pe[0] << 16 | pe[1] << 8 | pe[2];
            uint32_t off = datapages + (pn - 1) * pagesize;
            uint32_t len = pn == npages ? lastpage : pagesize;
            NEED(off + len <= (uint32_t)size);
            memcpy(objs[i].data + k * pagesize, d + off, len);
        }
    }
    numobjs = (int)nobj;

    // fixups, page by page
    for (uint32_t i = 0; i < nobj; i++) {
        for (uint32_t k = 0; k < pcnt[i]; k++) {
            uint32_t pg = pidx[i] - 1 + k;
            const byte *p = d + le + fixrec + rd32(d + le + fixpage + 4 * pg);
            const byte *end = d + le + fixrec + rd32(d + le + fixpage + 4 * (pg + 1));
            while (p < end) {
                int src = p[0], flg = p[1], cnt = 1, srcoff = 0;
                p += 2;
                if (src & 0x20)
                    cnt = *p++;
                else {
                    srcoff = (int16_t)rd16(p);
                    p += 2;
                }
                int tobj;
                if (flg & 0x40) {
                    tobj = rd16(p);
                    p += 2;
                } else {
                    tobj = *p++;
                }
                uint32_t toff = 0;
                if ((src & 0xF) != 2) {
                    if (flg & 0x10) {
                        toff = rd32(p);
                        p += 4;
                    } else {
                        toff = rd16(p);
                        p += 2;
                    }
                }
                uint32_t target = (tobj >= 1 && tobj <= (int)nobj ? objs[tobj - 1].base : 0) + toff;
                for (int j = 0; j < cnt; j++) {
                    int so = srcoff;
                    if (src & 0x20) {
                        so = (int16_t)rd16(p);
                        p += 2;
                    }
                    if (so < 0)
                        continue; // continuation of a fixup from the previous page
                    byte *at = objs[i].data + k * pagesize + so;
                    if ((src & 0xF) == 7)
                        wr32(at, target);
                    else if ((src & 0xF) == 8)
                        wr32(at, target - (objs[i].base + k * pagesize + so + 4));
                }
            }
        }
    }
#undef NEED
done:
    free(d);
    if (!ok)
        numobjs = 0;
    else
        I_Log("X_Init: %s: code @%#x, data @%#x", path, objs[0].base, objs[1].base);
    return ok;
}

static const byte *X_Ptr(uint32_t addr, int len)
{
    for (int i = 0; i < numobjs; i++)
        if (addr >= objs[i].base && addr + len <= objs[i].base + objs[i].size + 4096)
            return objs[i].data + (addr - objs[i].base);
    return NULL;
}

bool X_Valid(uint32_t addr, int len)
{
    return X_Ptr(addr, len) != NULL;
}

byte X_U8(uint32_t a)
{
    const byte *p = X_Ptr(a, 1);
    return p ? p[0] : 0;
}

int8_t X_S8(uint32_t a)
{
    return (int8_t)X_U8(a);
}

uint16_t X_U16(uint32_t a)
{
    const byte *p = X_Ptr(a, 2);
    return p ? rd16(p) : 0;
}

int16_t X_S16(uint32_t a)
{
    return (int16_t)X_U16(a);
}

uint32_t X_U32(uint32_t a)
{
    const byte *p = X_Ptr(a, 4);
    return p ? rd32(p) : 0;
}
