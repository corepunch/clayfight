#!/usr/bin/env python3
"""
Primal Rage (DOS, 1995) script/data-table extractor.  Stdlib only.

usage:  python3 -I extract_scripts.py [GAME_DIR] [OUT_JSON]
        defaults: ./primalrage  ./scripts.json

Everything below was reverse-engineered from PRAGE.EXE (Watcom C/386, LE executable bound
behind a DOS/4GW-Pro stub).  Addresses are LINEAR addresses after loading the two LE objects
at their preferred bases (object 1 = code @0x10000, object 2 = data @0x80000) and applying
the LE fixups (raw page bytes do NOT contain final pointer values, so fixups are mandatory).

Container (offsets in PRAGE.EXE):
  0x00000  MZ stub (DOS/4GW loader)            0x0F474  "BW" DOS/16M header (4GWPRO.EXP)
  0x26654  inner MZ (bound app)  e_lfanew=0x2A50 -> LE header @0x290A4
  LE: 2 objects, page size 4096, data pages @ inner_MZ+0x4D400 = 0x73A54
      obj1 code  base 0x10000 size 0x64709 (101 pages, file 0x73A54)
      obj2 data  base 0x80000 size 0xD8590 (193 pages, file 0xD8A54; bss beyond 0x141000)
  file offset of a data address A (A < 0x141000) = 0xD8A54 + (A - 0x80000)

Main facts used (see the report / comments at each table for the evidence):
  * Frame id table  @0x0F8A74: u32[18232]  = (gra_file_id << 23) | file offset of the 12-byte
    frame record inside the GRA type-6 chunk.  gra_file_id = index into INDEX.  The same
    encoding is used in GRA frame records ("o" field: top 9 bits = file id that holds the
    pixels, low 23 bits = offset) and for palette references (offset of a sub-palette's
    u32 count inside a type-5 chunk).
  * Frame anchor table @0x124A0C: u8[18232][2] (ax, ay) per frame id, used by hitbox placement.
  * Animation byte-code ("VM"): 16-bit little-endian words.  word < 0x8000 -> show frame id;
    word & 0x8000 -> opcode.  opcode number = (w>>8)&0x1F, operand mode = w&0x6000,
    selector/immediate = w&0xFF.  opcode 0x1F = extended: real opcode = low byte, next word =
    selector/immediate.  Interpreter: 0x2B780 (handlers via jump table 0x2B6C4),
    operand fetch 0x2BDD8, frame-select 0x2A8E8, per-tick driver ~0x2AF50.
    Timing: each object has frame timer [obj+0x20] and frame duration [obj+0x24] (floats,
    in game ticks); every tick timer-=1, when <=0 the script advances to the next frame and
    timer += duration.  Duration is set at animation start (call-site constant, mostly 2 or
    3; 2.0 for every move started through the move table) and changed by SETDUR/ADDDUR or
    by a per-index byte "timing table" (SETTIMING).
  * Core animation script table @0x118298: ptr[62][10]  (row = animation, column = character
    slot 0..9; slots 7..9 duplicate 2..4).
  * Character order (code index): 0 Sauron, 1 Blizzard, 2 Talon, 3 Vertigo, 4 Armadon,
    5 Diablo, 6 Chaos (matches the string table at 0x89823).
  * Move tables, index = char*64 + move_id:
        A @0x12D78C  11 bytes (A[0]=damage, A[7]=sound index into u16 table 0x138974, rest raw)
        B @0x0F3488  20 bytes {code handler, ptr into script table (-> row), ptr sound set,
                              ptr, u32 score points}
        C @0x0F6688   6 bytes raw
        HIT  @0x0E68A4 15 bytes = 5 x (step, box_index, active_ticks)  attacker hit boxes
        GRD  @0x0E8E24 15 bytes = 5 x (step, box_index, active_ticks)  defender boxes (see below)
        BOX  @0x11BC84 u8[7][256][4] = (x, y, w, h) box shapes
    'step' is compared with obj byte +0x63 which is incremented each time the animation
    advances a frame (so step = frame-advance count since the move started).
    A per-frame defender box can also come from a hard-coded frame-id switch in code at
    0x1669C (decoded here with a tiny x86 interpreter).
  * Special-move input definitions: ptr/info[7][32] @0x115AE4 (control types 2 and CPU) and
    @0x1164E4 (control types 0,4,6).  Each entry: u32 ptr to 8 steps x 20 bytes
    {u32 require, u32 reject, u32 abort, u32 window_ticks, u32 0}, u16 move_id, u8, u8.
  * Input bit layout (pattern words, facing-relative; converted by code 0x3CEB8):
        0x00010000 toward(edge)  0x00020000 toward(held)  0x00040000 away(edge)
        0x00080000 away(held)    0x8000 up(held) 0x0080 up(edge)  0x4000 down(held) 0x0040 down(edge)
        buttons: 0x0100,0x0200,0x0400,0x0800 held / 0x0001..0x0008 edge, assumed order
        HI QUICK, HI FIERCE, LO QUICK, LO FIERCE (order of the config-menu strings; not proven)
  * Object descriptors (20 bytes, used by spawn function 0x2B2F4):
        +0 script ptr, +4 u8 class, +5 u8 initial frame duration, +6 u16 sprite attr,
        +8 u16 flags, +0xA u16, +0xC u16, +0x10 palette ref (fileid<<23 | offset).
    Frames shown by an object use that object's palette -> palette assignment for
    selection screen / stages / HUD frames.
  * AI data per character (two-letter codes bb,cc,dd,rr,ss,tt,vv): table of name string
    pointers (mv_*, mvm_*, atmn_*), followed by raw data and a parallel pointer table.
    mv_* = AI joystick macros: list of (input pattern, ticks) terminated by 0xFFFF.

Output JSON (scripts.json):
  container, index[{id,name}]           INDEX order = gra_file_id used everywhere
  frame_ids.frames[id] = [gra_file_id, frame_index, anchor_x, anchor_y]
  characters[i]:
    name, prefix, ai, code_index, base_frame_id
    animations[row 0..61]: script addr, default_ticks_per_frame (+source), used_by_moves,
        referenced_from_code, gra_files, disassembly[...],
        sequence{frames[{frame, hflip, ticks, file, index, dx?, dy?, events?}],
                 collapsed[{file,index,hflip,ticks,ids,...}], end{type,...}, total_ticks}
    extra_animations[]: same, for scripts referenced directly from code (specials, CV, ...)
    moves[id 0..63]: damage, sound_index, A_raw, C_raw, handler, score, row?,
        attack_boxes/defense_boxes[{step, box_index, active_ticks, box[x,y,w,h], frame_id?}],
        button_guess (ids<24), scripts_referenced_by_handler / rows_referenced_by_handler
    frame_defense_boxes{frame_id: {box_index, box}}
    special_inputs{ctrl_type2_and_cpu|ctrl_types_0_4_6: [{slot, move_id, flag6, flag7,
        steps[{require, reject, abort, window, *_names}]}]}
  common_animations[]           code-referenced scripts not attributable to one fighter
  ai{code: {names_table, pointer_table, entries[{name, addr, macro|raw10|choices}]}}
  object_descriptors[]          {addr, script, cls, init_ticks, attr, flags, palette_file,
                                 palette_index, frames[[file,index]]}
  palette_assignment{gra_file: {frame_index: [[palette_file, palette_index], ...]}}
Hitbox geometry (code 0x15BF0): box bytes are in units of 3 px; origin = object screen pos
  minus (anchor_x*0.81, anchor_y*0.708) [0xCF4/0x1000, 0xB56/0x1000 PC scale]; mirrored when
  facing left as x' = 0x40 - x - w and anchor_x' = 0x100 - anchor_x; clipped to the sprite rect.
"""
import json, os, struct, sys

GAME = sys.argv[1] if len(sys.argv) > 1 else 'primalrage'
OUT = sys.argv[2] if len(sys.argv) > 2 else os.path.join(os.path.dirname(os.path.abspath(__file__)), 'scripts.json')

CHARS = [  # code index order used by every per-character table
    dict(name='Sauron',   prefix='REX', ai='rr'),
    dict(name='Blizzard', prefix='KON', ai='bb'),
    dict(name='Talon',    prefix='TAL', ai='tt'),
    dict(name='Vertigo',  prefix='COB', ai='vv'),
    dict(name='Armadon',  prefix='SPI', ai='ss'),
    dict(name='Diablo',   prefix='DIA', ai='dd'),
    dict(name='Chaos',    prefix='HAV', ai='cc'),
]

# ---------------------------------------------------------------- LE loader
class Mem:
    def __init__(self, path):
        d = open(path, 'rb').read()
        self.d = d
        # find the bound app: DOS/16M "BW" header right after the stub, its next_header_pos
        # (u32 at +0x1C) points to the inner MZ whose e_lfanew points to the LE header.
        stub = struct.unpack('<H', d[2:4])[0], struct.unpack('<H', d[4:6])[0]
        bw = (stub[1] - 1) * 512 + stub[0]
        assert d[bw:bw + 2] == b'BW', 'DOS/16M header not found'
        mz = struct.unpack('<I', d[bw + 0x1C:bw + 0x20])[0]
        assert d[mz:mz + 2] == b'MZ'
        le = mz + struct.unpack('<I', d[mz + 0x3C:mz + 0x40])[0]
        assert d[le:le + 2] == b'LE'
        u = lambda o: struct.unpack('<I', d[le + o:le + o + 4])[0]
        npages, pagesize, lastpage = u(0x14), u(0x28), u(0x2C)
        objtab, nobj, optab = u(0x40), u(0x44), u(0x48)
        fixpage, fixrec, datapages = u(0x68), u(0x6C), u(0x80) + mz
        self.layout = dict(bw_header=bw, inner_mz=mz, le_header=le, data_pages=datapages)
        pages = [(d[le + optab + 4 * i] << 16) | (d[le + optab + 4 * i + 1] << 8) | d[le + optab + 4 * i + 2]
                 for i in range(npages)]
        self.objs = []
        for i in range(nobj):
            vsize, base, flags, pidx, pcnt, _ = struct.unpack('<6I', d[le + objtab + 24 * i:le + objtab + 24 * i + 24])
            buf = bytearray(vsize + 8192)
            for k in range(pcnt):
                pn = pages[pidx - 1 + k]
                off = datapages + (pn - 1) * pagesize
                ln = lastpage if pn == npages else pagesize
                buf[k * 4096:k * 4096 + ln] = d[off:off + ln]
            self.objs.append(dict(vsize=vsize, base=base, pidx=pidx, pcnt=pcnt, data=buf,
                                  fileoff=datapages + (pages[pidx - 1] - 1) * pagesize))
        fpt = [struct.unpack('<I', d[le + fixpage + 4 * i:le + fixpage + 4 * i + 4])[0] for i in range(npages + 1)]
        self.ptrs = set()
        for oi, o in enumerate(self.objs):
            for k in range(o['pcnt']):
                pg = o['pidx'] - 1 + k
                p, end = le + fixrec + fpt[pg], le + fixrec + fpt[pg + 1]
                while p < end:
                    src, flg = d[p], d[p + 1]; p += 2
                    if src & 0x20:
                        cnt = d[p]; p += 1
                    else:
                        so = struct.unpack('<h', d[p:p + 2])[0]; p += 2
                    if flg & 0x40:
                        tobj = struct.unpack('<H', d[p:p + 2])[0]; p += 2
                    else:
                        tobj = d[p]; p += 1
                    st = src & 0xF
                    toff = 0
                    if st != 2:
                        if flg & 0x10:
                            toff = struct.unpack('<I', d[p:p + 4])[0]; p += 4
                        else:
                            toff = struct.unpack('<H', d[p:p + 2])[0]; p += 2
                    if src & 0x20:
                        sl = [struct.unpack('<h', d[p + 2 * j:p + 2 * j + 2])[0] for j in range(cnt)]; p += 2 * cnt
                    else:
                        sl = [so]
                    for so in sl:
                        if so < 0:
                            continue
                        so += k * 4096
                        tgt = self.objs[tobj - 1]['base'] + toff if tobj - 1 < len(self.objs) else toff
                        if st == 7:
                            o['data'][so:so + 4] = struct.pack('<I', tgt)
                            self.ptrs.add(o['base'] + so)
                        elif st == 8:
                            o['data'][so:so + 4] = struct.pack('<I', (tgt - (o['base'] + so + 4)) & 0xFFFFFFFF)

    def rd(self, a, n):
        for o in self.objs:
            if o['base'] <= a < o['base'] + o['vsize'] + 4096:
                r = a - o['base']
                return bytes(o['data'][r:r + n])
        raise IndexError(hex(a))
    def u8(self, a): return self.rd(a, 1)[0]
    def s8(self, a): return struct.unpack('<b', self.rd(a, 1))[0]
    def u16(self, a): return struct.unpack('<H', self.rd(a, 2))[0]
    def s16(self, a): return struct.unpack('<h', self.rd(a, 2))[0]
    def u32(self, a): return struct.unpack('<I', self.rd(a, 4))[0]
    def cstr(self, a):
        b = self.rd(a, 128)
        return b.split(b'\0')[0].decode('latin1')
    def isptr(self, a): return a in self.ptrs
    def fileoff(self, a):
        for o in self.objs:
            if o['base'] <= a < o['base'] + o['pcnt'] * 4096:
                return o['fileoff'] + (a - o['base'])
        return None

# ---------------------------------------------------------------- GRA files
class Gra:
    def __init__(self, game):
        self.game = game
        b = open(os.path.join(game, 'INDEX'), 'rb').read()
        self.index = []
        for i in range(len(b) // 20):
            e = b[i * 20:i * 20 + 20]
            v = struct.unpack('<I', e[12:16])[0]
            self.index.append(dict(id=i, name=e[:12].split(b'\0')[0].decode('latin1'), size=v & 0xFFFFFF, flag=v >> 24))
        self.cache = {}
    def info(self, fid):
        if fid in self.cache:
            return self.cache[fid]
        name = self.index[fid]['name']
        path = os.path.join(self.game, name.upper())
        r = dict(name=name, exists=os.path.exists(path), frames=[], frame_chunk=None, palettes=[])
        if r['exists']:
            b = open(path, 'rb').read()
            off = 0
            while off + 8 <= len(b):
                t, _, nx = struct.unpack('<HHI', b[off:off + 8])
                end = nx if (nx > off and nx <= len(b)) else len(b)
                if t == 6:
                    r['frame_chunk'] = off
                    p = off + 8
                    while p + 12 <= len(b):
                        w, h, xh, yh, o = struct.unpack('<HHhhI', b[p:p + 12])
                        r['frames'].append(dict(w=w, h=h, xhot=xh, yhot=yh, pix_file=o >> 23, pix_off=o & 0x7FFFFF))
                        p += 12
                elif t == 5:
                    p = off + 8
                    while p + 4 <= end:
                        n = struct.unpack('<I', b[p:p + 4])[0]
                        if n == 0 or n > 256 or p + 4 + 4 * n > end:
                            break
                        r['palettes'].append(dict(offset=p, count=n))
                        p += 4 + 4 * n
                if nx <= off or nx >= len(b):
                    break
                off = nx
        self.cache[fid] = r
        return r
    def palette_index(self, ref):
        fid, off = ref >> 23, ref & 0x7FFFFF
        if fid >= len(self.index):
            return None
        for i, p in enumerate(self.info(fid)['palettes']):
            if p['offset'] == off:
                return fid, i
        return fid, None

# ---------------------------------------------------------------- frame ids
FRAMETAB, NFRAMES, ANCHORS = 0xF8A74, 18232, 0x124A0C
class Frames:
    def __init__(self, mem, gra):
        self.m, self.g = mem, gra
        self.tab = [mem.u32(FRAMETAB + 4 * i) for i in range(NFRAMES)]
    def lookup(self, fid):
        fid &= 0x7FFF
        if fid >= NFRAMES:
            return None
        v = self.tab[fid]
        gid, off = v >> 23, v & 0x7FFFFF
        if gid >= len(self.g.index):
            return None
        inf = self.g.info(gid)
        r = dict(file=inf['name'], file_id=gid)
        if inf['frame_chunk'] is not None:
            k, rem = divmod(off - inf['frame_chunk'] - 8, 12)
            if rem == 0 and 0 <= k < len(inf['frames']):
                r['index'] = k
        return r
    def anchor(self, fid):
        fid &= 0x7FFF
        return [self.m.u8(ANCHORS + 2 * fid), self.m.u8(ANCHORS + 2 * fid + 1)]

# ---------------------------------------------------------------- script VM
OPN = {0: 'END', 1: 'HOLD', 2: 'SET_B61', 3: 'GOTO', 4: 'LOOPN', 5: 'WAIT_NONZERO', 6: 'SWITCH',
       7: 'DELAY', 8: 'RANDOM_DELAY', 9: 'NOP', 0xA: 'ADD_DURATION', 0xB: 'SET_DURATION', 0xC: 'SPAWN',
       0xD: 'FRAME_SELECT', 0xE: 'CLEAR_VAR', 0xF: 'SET_VAR', 0x10: 'CALL', 0x11: 'CALL_ARG',
       0x12: 'SET_ATTR', 0x13: 'SET_B59', 0x14: 'FLIP_FACING', 0x15: 'CALL_YIELD', 0x16: 'CLR_FLAG29_2',
       0x17: 'INC_VAR', 0x18: 'INC_BRANCH_LT', 0x19: 'DEC_BRANCH_GE', 0x1A: 'SET_DATA_TABLE',
       0x1B: 'CLEAR_DATA_TABLE', 0x1C: 'SET_TIMING_TABLE', 0x1D: 'CLEAR_TIMING_PTR', 0x1E: 'TIMING_OFF',
       0x20: 'MOVE_X', 0x21: 'MOVE_Y', 0x22: 'ADD_Z', 0x25: 'ADD_VEL_X', 0x26: 'ADD_VEL_Y', 0x27: 'ADD_VEL_Z',
       0x28: 'SET_VEL_X', 0x29: 'SET_VEL_Y', 0x2A: 'SET_VEL_Z', 0x2B: 'SET_2C', 0x2C: 'ADD_2C',
       0x2D: 'IF_SPRITE_Y_GE', 0x2E: 'SOUND'}
# var selectors: <0x40 global array (index + player number); 0x40..0x45 = this object's bytes
# +0x52,+0x53,+0x54,+0x55,(+0x56 word),+0x58 ; 0x46..0x4B the same fields of the linked object.
VARN = {0x40: 'v52', 0x41: 'v53', 0x42: 'v54', 0x43: 'v55', 0x44: 'slot', 0x45: 'v58'}
# opcodes whose low byte is a variable selector even in immediate mode
VAR_OPS = {0xE, 0xF, 0x17}

class VM:
    def __init__(self, mem):
        self.m = mem
    def decode(self, a):
        m = self.m
        op = m.u16(a)
        if not op & 0x8000:
            return dict(a=a, kind='frame', frame=op, next=a + 2, targets=[])
        mode, opc, sel, p = op & 0x6000, (op >> 8) & 0x1F, op & 0xFF, a + 2
        ins = dict(a=a, kind='op', raw=op, mode=mode, targets=[], data=[])
        if opc == 0xD:
            ins.update(op='FRAME_SELECT', opc=0xD, var=sel)
            if mode == 0x6000:
                ins['table'] = m.u32(p); p += 4; ins['data'].append(ins['table'])
            else:
                ins['base'] = m.u16(p); p += 2
            ins['next'] = p
            return ins
        ext = opc == 0x1F
        if ext:
            opc, sel = sel, m.u16(p); p += 2
        ins['opc'] = opc
        ins['op'] = OPN.get(opc, 'OP_%02X' % opc)
        if mode == 0:
            if opc in VAR_OPS:
                ins['var'] = sel
            elif ext:
                ins['imm'] = sel - 0x10000 if sel & 0x8000 else sel
            else:
                ins['imm'] = sel - 0x100 if sel & 0x80 else sel
        elif mode == 0x2000:
            ins['var'] = sel
        elif mode == 0x4000:
            ins['sel'] = sel; ins['ptr'] = m.u32(p); p += 4
        else:
            ins['var'] = sel; ins['tabletype'] = m.u16(p + 2); p += 4
        nx = True
        if opc == 6:
            n = m.u16(p); p += 2
            ins['cases'] = [m.u32(p + 4 * k) for k in range(n + 1)]; p += 4 * (n + 1)
            ins['targets'] += ins['cases']
        elif opc == 0xC:
            ins['dx'], ins['dy'] = m.s16(p), m.s16(p + 2); p += 4
        elif opc == 0xF and mode == 0:
            ins['value'] = m.u16(p); p += 2
        elif opc == 0x11:
            ins['arg'] = m.u16(p); p += 2
        elif opc in (0x18, 0x19):
            ins['limit'] = m.u16(p); ins['jump'] = m.u32(p + 2); p += 6
            ins['targets'].append(ins['jump'])
        elif opc == 0x2D:
            ins['jump'] = m.u32(p); p += 4; ins['targets'].append(ins['jump'])
        elif opc == 3:
            ins['targets'].append(ins['ptr']); nx = False
        elif opc == 4:
            ins['targets'].append(ins['ptr'])
        elif opc in (0, 1, 0x15):
            nx = False
        if opc in (0x1A, 0x1C, 0xC) and 'ptr' in ins:
            ins['data'].append(ins['ptr'])
        ins['next'] = p if nx else None
        return ins

    def walk(self, start, stop=frozenset(), limit=3000):
        """static disassembly of everything reachable from start (stops at data addresses)"""
        seen, todo, data = {}, [start], set()
        while todo and len(seen) < limit:
            a = todo.pop()
            while a is not None and a not in seen and a not in stop:
                try:
                    ins = self.decode(a)
                except Exception:
                    break
                seen[a] = ins
                data.update(ins.get('data', ()))
                todo += [t for t in ins['targets'] if t not in seen]
                nxt = ins['next']
                if ins.get('op') in ('CALL', 'CALL_ARG') and nxt in data:
                    nxt = None  # call to a "change animation" routine followed by inline table
                a = nxt
        return [seen[k] for k in sorted(seen)], data

def fmt_ins(ins):
    if ins['kind'] == 'frame':
        return dict(at=hex(ins['a']), frame=ins['frame'] & 0x7FFF, hflip=bool(ins['frame'] & 0x8000))
    r = dict(at=hex(ins['a']), op=ins['op'])
    for k in ('var', 'imm', 'value', 'arg', 'limit', 'dx', 'dy', 'base', 'tabletype', 'sel'):
        if k in ins:
            r[k] = ins[k]
    if 'var' in ins:
        r['var'] = VARN.get(ins['var'], 'g%02x' % ins['var'] if ins['var'] < 0x40 else 'x%02x' % ins['var'])
    for k in ('ptr', 'jump', 'table'):
        if k in ins:
            r[k] = hex(ins[k])
    if 'cases' in ins:
        r['cases'] = [hex(c) for c in ins['cases']]
    return r

LOCAL = {0x40: 0, 0x41: 0, 0x42: 0, 0x43: 0, 0x45: 0}
END_CALLS = {0x370C0}   # "return to neutral / pick next animation" (followed by data in scripts)

def simulate(vm, frames, start, duration, max_steps=4000):
    """Linear playback: list of shown frames with tick counts.  Vars start at 0 (the animation
    start routine 0x2C110 clears v52 and the LOOPN counter).  CALLs are recorded but their side
    effects are unknown.  Stops at END/HOLD/CALL_YIELD, a loop back to an already seen state, or
    a branch on an unknown variable."""
    m = vm.m
    st = dict(LOCAL)
    ip, dur, timing, table, loopc = start, float(duration), None, None, 0
    out, events, seen = [], [], {}
    pend = dict(dx=0, dy=0)
    end = None
    def var(sel):
        return st.get(sel)
    def show(word):
        nonlocal pend
        idx = st[0x40]
        t = dur
        if timing is not None:
            t = float(m.u8(timing + idx))
        f = word & 0x7FFF
        e = dict(frame=f, hflip=bool(word & 0x8000), ticks=t)
        lk = frames.lookup(f)
        if lk:
            e.update(file=lk['file'], index=lk.get('index'))
        if pend['dx'] or pend['dy']:
            e['dx'], e['dy'] = pend['dx'], pend['dy']
        if events:
            e['events'] = list(events); events.clear()
        out.append(e)
        pend = dict(dx=0, dy=0)
    for _ in range(max_steps):
        key = (ip, tuple(sorted(st.items())), loopc, timing)
        if key in seen:
            end = dict(type='loop', to_step=seen[key]); break
        seen[key] = len(out)
        try:
            ins = vm.decode(ip)
        except Exception:
            end = dict(type='decode_error'); break
        if ins['kind'] == 'frame':
            show(ins['frame']); ip = ins['next']; continue
        o = ins['opc']
        nxt = ins['next']
        if o == 0xD:
            v = var(ins['var'])
            if v is None:
                end = dict(type='unknown_var', var=hex(ins['var'])); break
            word = ins['base'] + v if 'base' in ins else m.u16(ins['table'] + 2 * v)
            show(word); ip = nxt; continue
        if o in (0, 1):
            end = dict(type=ins['op']); break
        if o == 3:
            if 'ptr' not in ins:
                end = dict(type='unsupported_operand'); break
            ip = ins['ptr']; continue
        if o == 4:
            if 'ptr' not in ins:
                end = dict(type='unsupported_operand'); break
            loopc += 1
            if loopc <= ins['sel']:
                ip = ins['ptr']
            else:
                loopc = 0; ip = nxt
            continue
        if o == 6:
            v = var(ins['var']) if 'var' in ins else None
            if v is None:
                end = dict(type='unknown_switch'); break
            ip = ins['cases'][v - 1] if 1 <= v <= len(ins['cases']) else nxt
            continue
        if o == 7:
            n = ins.get('imm')
            if out and n is not None:
                out[-1]['ticks'] += n
            elif out:
                out[-1]['delay_var'] = True
            ip = nxt; continue
        if o == 0xA and 'imm' in ins:
            dur += ins['imm']
        elif o == 0xB and 'imm' in ins and ins['imm'] > 0:
            dur = float(ins['imm'])
        elif o == 0xE:
            if 'var' not in ins:
                end = dict(type='unsupported_operand'); break
            st[ins['var']] = 0
        elif o == 0xF:
            if 'value' in ins and 'var' in ins:
                st[ins['var']] = ins['value']
            else:
                end = dict(type='set_from_var'); break
        elif o == 0x17:
            if 'var' not in ins or st.get(ins['var']) is None:
                end = dict(type='unknown_var'); break
            st[ins['var']] += 1
        elif o in (0x18, 0x19):
            v = var(ins['var']) if 'var' in ins else None
            if v is None:
                end = dict(type='unknown_var', var=hex(ins.get('var', 0))); break
            v = v + 1 if o == 0x18 else v - 1
            st[ins['var']] = v
            take = (v < ins['limit']) if o == 0x18 else (v >= ins['limit'])
            ip = ins['jump'] if take else nxt
            continue
        elif o == 0x1A:
            table = ins.get('ptr')
        elif o == 0x1B:
            table = None
        elif o == 0x1C:
            timing = ins.get('ptr')
        elif o == 0x1E:
            timing = None
        elif o in (0x20, 0x21):
            if ins['mode'] == 0x6000 and table is not None:
                tt = ins['tabletype'] & 0xF000
                idx = st.get(ins['var'], 0)
                addr = {0x1000: table + 2 * idx, 0x2000: table + 2 * idx + 1, 0: table + idx}.get(tt)
                val = m.s8(addr) if addr is not None else 0
            else:
                val = ins.get('imm', 0)
            pend['dx' if o == 0x20 else 'dy'] += val
        elif o in (0x10, 0x11, 0x15):
            if 'ptr' not in ins:
                end = dict(type='unsupported_operand'); break
            ev = dict(call=hex(ins['ptr']))
            if 'arg' in ins:
                ev['arg'] = ins['arg']
            if o == 0x15 or ins['ptr'] in END_CALLS:
                if out:
                    out[-1].setdefault('events', []).append(ev)
                end = dict(type='call_yield' if o == 0x15 else 'call_end', call=hex(ins['ptr'])); break
            events.append(ev)
        elif o == 5:
            events.append(dict(wait_nonzero=VARN.get(ins.get('var'), ins.get('var'))))
        elif o == 0xC:
            events.append(dict(spawn=hex(ins.get('ptr', 0)), dx=ins['dx'], dy=ins['dy']))
        elif o == 0x2E:
            events.append(dict(sound=ins.get('imm', ins.get('var'))))
        elif o == 0x14:
            events.append(dict(flip_facing=True))
        elif o in (0x25, 0x26, 0x27, 0x28, 0x29, 0x2A, 0x22, 0x2B, 0x2C, 0x12, 0x13, 0x2, 0x16):
            events.append({ins['op'].lower(): ins.get('imm', VARN.get(ins.get('var'), ins.get('var')))})
        elif o == 0x2D:
            end = dict(type='conditional_on_sprite_y'); break
        ip = nxt
    else:
        end = dict(type='step_limit')
    return dict(frames=out, end=end, total_ticks=sum(f['ticks'] for f in out))

# ---------------------------------------------------------------- tiny x86 for 0x1669C
def frame_switch(mem, entry, frame):
    """Interpret the compare-chain used by code 0x1669C (frame id -> box index).
    Supports exactly the encodings present there: 3D imm32 (cmp eax), 7x rel8 / 0F 8x rel32
    (jcc), EB/E9 (jmp), B8 imm32 (mov eax), 83 C4 xx (add esp), 5A (pop edx), C3 (ret)."""
    ip, eax, flags = entry, frame, None
    for _ in range(500):
        b = mem.rd(ip, 6)
        if b[0] == 0x3D:
            imm = struct.unpack('<i', b[1:5])[0]; flags = (eax, imm); ip += 5
        elif 0x70 <= b[0] <= 0x7F or (b[0] == 0x0F and 0x80 <= b[1] <= 0x8F):
            if b[0] == 0x0F:
                cc, rel, ln = b[1] - 0x80, struct.unpack('<i', b[2:6])[0], 6
            else:
                cc, rel, ln = b[0] - 0x70, struct.unpack('<b', b[1:2])[0], 2
            x, y = flags
            ux, uy = x & 0xFFFFFFFF, y & 0xFFFFFFFF
            take = {0x2: ux < uy, 0x3: ux >= uy, 0x4: x == y, 0x5: x != y, 0x6: ux <= uy, 0x7: ux > uy,
                    0xC: x < y, 0xD: x >= y, 0xE: x <= y, 0xF: x > y}[cc]
            ip = ip + ln + rel if take else ip + ln
        elif b[0] == 0xEB:
            ip = ip + 2 + struct.unpack('<b', b[1:2])[0]
        elif b[0] == 0xE9:
            ip = ip + 5 + struct.unpack('<i', b[1:5])[0]
        elif b[0] == 0xB8:
            eax = struct.unpack('<i', b[1:5])[0]; ip += 5
        elif b[0] == 0x83 and b[1] == 0xC4:
            ip += 3
        elif b[0] == 0x5A:
            ip += 1
        elif b[0] == 0xC3:
            return eax
        else:
            raise ValueError('unsupported opcode %02x at %x' % (b[0], ip))
    raise ValueError('runaway')

# ---------------------------------------------------------------- call-site default durations
START_FUNCS = {0x3CC9C, 0x2C110, 0x3CC50, 0x3CCF0}
ROWREFS = {}   # row -> code addresses that load a script pointer of that row
SCRIPTTAB, NROWS = 0x118298, 62
def row_default_durations(mem):
    """Scan code for 'mov r32,[4*r+disp32]' with disp in the script table near a
    'push imm32(float)' and a call to an animation-start routine."""
    code = mem.objs[0]
    cb, base = code['data'], code['base']
    res = {}
    n = code['vsize']
    for i in range(n - 7):
        if cb[i] == 0x8B and (cb[i + 1] & 0xC7) == 0x04 and (cb[i + 2] & 0xC7) == 0x85:
            disp = struct.unpack('<I', cb[i + 3:i + 7])[0]
            if not (SCRIPTTAB <= disp < SCRIPTTAB + NROWS * 40):
                continue
            row = (disp - SCRIPTTAB) // 40
            ROWREFS.setdefault(row, set()).add(base + i)
            lo, hi = max(0, i - 48), min(n - 5, i + 64)
            dur, call = None, None
            for j in range(lo, hi):
                if cb[j] == 0x68:
                    f = struct.unpack('<f', cb[j + 1:j + 5])[0]
                    if f in (1.0, 2.0, 3.0, 4.0, 5.0, 6.0, 8.0):
                        if dur is None or abs(j - i) < abs(dur[1] - i):
                            dur = (f, j)
                if cb[j] == 0xE8 and j > i:
                    tgt = base + j + 5 + struct.unpack('<i', cb[j + 1:j + 5])[0]
                    if tgt in START_FUNCS and call is None:
                        call = tgt
            if dur and call:
                res.setdefault(row, set()).add(dur[0])
    return {r: sorted(v) for r, v in res.items()}

# ---------------------------------------------------------------- input patterns
def decode_pattern(v):
    names = []
    for bit, nm in ((0x10000, 'toward*'), (0x20000, 'toward'), (0x40000, 'away*'), (0x80000, 'away'),
                    (0x8000, 'up'), (0x80, 'up*'), (0x4000, 'down'), (0x40, 'down*'),
                    (0x1000, 'right'), (0x10, 'right*'), (0x2000, 'left'), (0x20, 'left*'),
                    (0x100, 'HQ'), (0x1, 'HQ*'), (0x200, 'HF'), (0x2, 'HF*'),
                    (0x400, 'LQ'), (0x4, 'LQ*'), (0x800, 'LF'), (0x8, 'LF*')):
        if v & bit:
            names.append(nm)
    return names

# ---------------------------------------------------------------- main extraction
def main():
    mem = Mem(os.path.join(GAME, 'PRAGE.EXE'))
    gra = Gra(GAME)
    frames = Frames(mem, gra)
    vm = VM(mem)
    out = dict(source='PRAGE.EXE', notes=__doc__.strip().split('\n'), container=mem.layout)
    out['index'] = [dict(id=e['id'], name=e['name']) for e in gra.index]

    # global frame-id table (compact): [gra_file_id, frame_index or -1, anchor_x, anchor_y]
    ftab = []
    for f in range(NFRAMES):
        lk = frames.lookup(f) or {}
        ax, ay = frames.anchor(f)
        ftab.append([lk.get('file_id', -1), lk.get('index', -1), ax, ay])
    out['frame_ids'] = dict(table_addr=hex(FRAMETAB), file_offset=mem.fileoff(FRAMETAB),
                           fields=['gra_file_id', 'frame_index', 'anchor_x', 'anchor_y'], frames=ftab)

    rowdur = row_default_durations(mem)
    # rows referenced by move table B -> row index
    chars = []
    for ci, c in enumerate(CHARS):
        cd = dict(c, code_index=ci)
        first = mem.u32(SCRIPTTAB + 4 * ci)
        cd['base_frame_id'] = mem.u16(first - 2)
        # ---- moves
        moves = []
        row_users = {}
        for mid in range(64):
            k = ci * 64 + mid
            A = mem.rd(0x12D78C + 11 * k, 11)
            Bb = 0xF3488 + 20 * k
            B = [mem.u32(Bb + 4 * j) for j in range(5)]
            C = mem.rd(0xF6688 + 6 * k, 6)
            mv = dict(id=mid, damage=A[0], sound_index=A[7], A_raw=list(A), C_raw=list(C),
                      handler=hex(B[0]) if B[0] else None, score=B[4],
                      sound_set=hex(B[2]) if B[2] else None)
            if B[1] and SCRIPTTAB <= B[1] < SCRIPTTAB + NROWS * 40:
                r, colo = divmod(B[1] - SCRIPTTAB, 40)
                mv['row'] = r
                row_users.setdefault(r, []).append(mid)
            elif B[1]:
                mv['script_ref'] = hex(B[1])
            hits, grds = [], []
            for tab, lst in ((0xE68A4, hits), (0xE8E24, grds)):
                t = tab + 0x3C0 * ci + 15 * mid
                for j in range(5):
                    step, bi, act = mem.s8(t + 3 * j), mem.u8(t + 3 * j + 1), mem.u8(t + 3 * j + 2)
                    if step < 0:
                        continue
                    box = list(mem.rd(0x11BC84 + 4 * (ci * 256 + bi), 4))
                    if box[2] < 1 or box[3] < 1:
                        continue  # code ignores boxes with w or h < 1 (0x17077)
                    lst.append(dict(step=step, box_index=bi, active_ticks=act, box=box))
            mv['attack_boxes'] = hits
            mv['defense_boxes'] = grds
            if B[0] or B[1] or hits:
                moves.append(mv)
        cd['moves'] = moves
        # ---- core animation rows
        rows = []
        for r in range(NROWS):
            a = mem.u32(SCRIPTTAB + 4 * (r * 10 + ci))
            ins, data = vm.walk(a)
            ins, data = vm.walk(a, stop=frozenset(data))
            d = rowdur.get(r)
            default = 2.0 if r in row_users else (d[0] if d else 3.0)
            seq = simulate(vm, frames, a, default)
            files = sorted({f['file'] for f in seq['frames'] if 'file' in f})
            rows.append(dict(row=r, script=hex(a), script_file_offset=mem.fileoff(a),
                             used_by_moves=row_users.get(r, []),
                             default_ticks_per_frame=default,
                             default_ticks_source='move table (2.0)' if r in row_users else
                             ('code call sites %s' % d if d else 'unknown, assumed 3.0'),
                             gra_files=files, sequence=seq,
                             disassembly=[fmt_ins(i) for i in ins]))
        for rw in rows:
            seq = rw['sequence']
            col, cur = [], None
            for f in seq['frames']:
                k = (f.get('file'), f.get('index'), f['hflip'])
                if cur and cur[0] == k and not f.get('events') and not f.get('dx') and not f.get('dy'):
                    cur[1]['ticks'] += f['ticks']; cur[1]['ids'].append(f['frame'])
                else:
                    e = dict(file=k[0], index=k[1], hflip=k[2], ticks=f['ticks'], ids=[f['frame']])
                    for kk in ('dx', 'dy', 'events'):
                        if kk in f:
                            e[kk] = f[kk]
                    cur = (k, e); col.append(e)
            seq['collapsed'] = col
        cd['animations'] = rows
        # attack-box step -> frame actually shown at that step (step = frames advanced since start)
        for mv in moves:
            if 'row' not in mv:
                continue
            fl = rows[mv['row']]['sequence']['frames']
            for b in mv['attack_boxes'] + mv['defense_boxes']:
                if 0 <= b['step'] < len(fl):
                    b['frame_id'] = fl[b['step']]['frame']
                    b['frame'] = [fl[b['step']].get('file'), fl[b['step']].get('index')]
            mv['button_guess'] = ['HQ', 'HF', 'LQ', 'LF'][mv['id'] % 4] if mv['id'] < 24 else None
        for mv in moves:
            h = int(mv['handler'], 16) if mv['handler'] else None
            if h:
                mv['rows_referenced_by_handler'] = sorted(r for r, refs in ROWREFS.items()
                                                          if any(h <= x < h + 0x300 for x in refs))
        for rw in rows:
            rw['referenced_from_code'] = sorted(hex(x) for x in ROWREFS.get(rw['row'], ()))
        # ---- frame-specific defender boxes from the code switch at 0x1669C
        jt = 0x16680
        entry = mem.u32(jt + 4 * ci)
        fb = {}
        for f in range(cd['base_frame_id'], min(NFRAMES, cd['base_frame_id'] + 1500)):
            try:
                v = frame_switch(mem, entry, f)
            except Exception:
                continue
            if v != -1:
                fb[f] = dict(box_index=v, box=list(mem.rd(0x11BC84 + 4 * (ci * 256 + v), 4)))
        cd['frame_defense_boxes'] = {str(k): v for k, v in fb.items()}
        # ---- special move inputs
        specials = {}
        for tname, tab in (('ctrl_type2_and_cpu', 0x115AE4), ('ctrl_types_0_4_6', 0x1164E4)):
            lst = []
            for s in range(32):
                e = tab + 8 * (ci * 32 + s)
                p = mem.u32(e)
                if not p:
                    continue
                steps = []
                for j in range(8):
                    q = p + 20 * j
                    req, rej, abo, win = mem.u32(q), mem.u32(q + 4), mem.u32(q + 8), mem.u32(q + 12)
                    steps.append(dict(require=hex(req), reject=hex(rej), abort=hex(abo), window=win,
                                      require_names=decode_pattern(req), reject_names=decode_pattern(rej)))
                while len(steps) > 1 and steps[-1]['require'] == '0x0' and steps[-1]['reject'] == '0x0' \
                        and steps[-2]['require'] == '0x0' and steps[-2]['reject'] == '0x0':
                    steps.pop()
                lst.append(dict(slot=s, move_id=mem.u16(e + 4), flag6=mem.u8(e + 6), flag7=mem.u8(e + 7),
                                steps=steps))
            specials[tname] = lst
        cd['special_inputs'] = specials
        chars.append(cd)
    # ---- extra script entry points referenced directly by code (special moves, fatalities,
    # victory/"CV", projectiles...).  Not table driven, so names are unknown; each entry is
    # assigned to the character whose GRA files it shows.
    known = set()
    for cd in chars:
        for rw in cd['animations']:
            known.add(int(rw['script'], 16))
        known.add(mem.u32(SCRIPTTAB + 4 * cd['code_index']) - 2)   # base-frame-id word, not a script
    code = mem.objs[0]
    def push_near(site):
        # nearest 'push imm32' holding a small float within +-64 bytes of the code reference
        best = None
        o = site - code['base']
        for j in range(max(0, o - 64), min(code['vsize'] - 5, o + 64)):
            if code['data'][j] == 0x68:
                f = struct.unpack('<f', bytes(code['data'][j + 1:j + 5]))[0]
                if f in (1.0, 2.0, 3.0, 4.0, 5.0, 6.0, 8.0) and (best is None or abs(j - o) < best[1]):
                    best = (f, abs(j - o))
        return best[0] if best else None
    allrefs = {}
    for p in mem.ptrs:
        if p < 0x80000:
            t = mem.u32(p)
            if 0x120C00 <= t < 0x13F000 and t % 2 == 0:   # animation-script area
                allrefs.setdefault(t, set()).add(p)
    prefixes = {c['prefix'].lower(): i for i, c in enumerate(CHARS)}
    extra = {i: [] for i in range(len(CHARS))}
    common = []
    for t in sorted(allrefs):
        if t in known:
            continue
        try:
            first = vm.decode(t)
        except Exception:
            continue
        durs = sorted({d for d in (push_near(x) for x in allrefs[t]) if d})
        dflt = durs[0] if durs else 2.0
        seq = simulate(vm, frames, t, dflt, max_steps=3000)
        if not seq['frames'] or seq['end']['type'] in ('step_limit', 'decode_error') \
                or any(f.get('index') is None for f in seq['frames']):
            continue
        cnt = {}
        for f in seq['frames']:
            fn = f.get('file') or ''
            cnt[fn[3:6]] = cnt.get(fn[3:6], 0) + 1
        best = max(cnt, key=cnt.get)
        files = sorted({f['file'] for f in seq['frames'] if 'file' in f})
        ent = dict(script=hex(t), script_file_offset=mem.fileoff(t),
                   referenced_from_code=sorted(hex(x) for x in allrefs[t]),
                   gra_files=files, default_ticks_per_frame=dflt,
                   default_ticks_source=('push near code ref %s' % durs) if durs else 'unknown, assumed 2.0',
                   sequence=seq)
        if best in prefixes:
            extra[prefixes[best]].append(ent)
        else:
            common.append(ent)
    hs = sorted({int(mv['handler'], 16) for cd in chars for mv in cd['moves'] if mv['handler']})
    for i, cd in enumerate(chars):
        cd['extra_animations'] = extra[i]
        for mv in cd['moves']:
            if mv['handler']:
                h = int(mv['handler'], 16)
                nxt = min([x for x in hs if x > h] + [h + 0x400])
                mv['scripts_referenced_by_handler'] = [e['script'] for e in extra[i]
                    if any(h <= int(x, 16) < nxt for x in e['referenced_from_code'])]
    out['common_animations'] = common
    out['characters'] = chars
    out['row_default_durations_from_code'] = {str(k): v for k, v in rowdur.items()}

    # ---- AI blocks
    ai = {}
    addr = 0x80000
    blocks = []
    # locate name tables: runs of pointers to "mv_xx_" strings
    for ci, c in enumerate(CHARS):
        pass
    ptrs = sorted(p for p in mem.ptrs if 0x80000 <= p < 0xE6000)
    def isname(v):
        try:
            s = mem.cstr(v)
        except Exception:
            return None
        return s if (s[:3] in ('mv_', 'mvm', 'atm')) else None
    runs, cur = [], None
    for p in ptrs:
        s = isname(mem.u32(p))
        if s and cur and p == cur[1]:
            cur[1] = p + 4; cur[2].append(s)
        else:
            if cur and len(cur[2]) > 100:
                runs.append(cur)
            cur = [p, p + 4, [s]] if s else None
    if cur and len(cur[2]) > 100:
        runs.append(cur)
    for start, end, names in runs:
        code = names[0].split('_')[1]
        # parallel pointer table = first relocated dword after the raw data block
        pt = min(p for p in mem.ptrs if p >= end)
        entries = []
        for i, nm in enumerate(names):
            tgt = mem.u32(pt + 4 * i)
            e = dict(name=nm, addr=hex(tgt))
            if nm.startswith('mv_'):
                seq = []
                q = tgt
                for _ in range(64):
                    inp, t = mem.u32(q), mem.u32(q + 4)
                    if inp == 0xFFFF:
                        break
                    seq.append([hex(inp), t, decode_pattern(inp)])
                    q += 8
                e['macro'] = seq
            elif nm.startswith('mvm'):
                e['raw10'] = list(mem.rd(tgt, 10))
            else:
                ent = []
                q = tgt
                for _ in range(80):
                    b = mem.rd(q, 4)
                    ent.append(list(b)); q += 4
                    if b[3] == 0xFF or b == b'\0\0\0\0':
                        break
                e['choices'] = ent  # (mvm index, flag, lo, hi) random-range entries (0..255)
            entries.append(e)
        ai[code] = dict(names_table=hex(start), pointer_table=hex(pt), entries=entries)
    out['ai'] = ai

    # ---- object descriptors -> palette assignment
    descs = []
    for p in sorted(mem.ptrs):
        if not (0x80000 <= p < 0x141000) or p % 2:
            continue
        sp = mem.u32(p)
        if not (0x100000 <= sp < 0x141000):
            continue
        ref = mem.u32(p + 16)
        pi = gra.palette_index(ref)
        if not pi or pi[1] is None:
            continue
        descs.append((p, sp, ref, pi))
    palmap = {}
    dlist = []
    for p, sp, ref, (pfid, pidx) in descs:
        ins, data = vm.walk(sp, limit=800)
        ins, data = vm.walk(sp, stop=frozenset(data), limit=800)
        # explicit frame words + frames reached by linear playback (no guessing of
        # FRAME_SELECT ranges, which produced false palette associations)
        fr = set(i['frame'] & 0x7FFF for i in ins if i['kind'] == 'frame')
        try:
            fr.update(f['frame'] for f in simulate(vm, frames, sp, 1.0, max_steps=1500)['frames'])
        except Exception:
            pass
        used = []
        for f in sorted(fr):
            lk = frames.lookup(f)
            if not lk or 'index' not in lk:
                continue
            used.append([lk['file'], lk['index']])
            palmap.setdefault(lk['file'], {}).setdefault(lk['index'], set()).add((gra.index[pfid]['name'], pidx))
        dlist.append(dict(addr=hex(p), script=hex(sp), cls=mem.u8(p + 4), init_ticks=mem.u8(p + 5),
                          attr=mem.u16(p + 6), flags=mem.u16(p + 8), palette_file=gra.index[pfid]['name'],
                          palette_index=pidx, frames=used[:400]))
    out['object_descriptors'] = dlist
    out['palette_assignment'] = {fn: {str(k): sorted([list(x) for x in v]) for k, v in sorted(d.items())}
                                 for fn, d in sorted(palmap.items())}
    with open(OUT, 'w') as fh:
        json.dump(out, fh, indent=1)
    print('wrote', OUT)

if __name__ == '__main__':
    main()
