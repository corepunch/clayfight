#!/usr/bin/env python3
"""pr2creature.py -- convert Primal Rage's fighters into Clayfight creature files.

  python3 tools/pr2creature.py [GAME_DIR] [OUT_DIR]     (defaults: primalrage creatures/primalrage)

Reads the move scripts and tables out of GAME_DIR/PRAGE.EXE (via
extract_scripts.py's LE loader and instruction decoder), plays every script
linearly the way the game's interpreter would, and writes one text file per
fighter in the creature format documented in docs/creature-format.md.

What becomes what:
  frame words, durations, timing tables  -> explicit frames with tics
  MOVE_X root-motion tables              -> per-frame "dx" (PC pixels, + = forward)
  FRAME_SELECT v52 + native 0x38294      -> "wander from N" (the random breathing idle)
  native 0x3A214 (arg n)                 -> "stun n" (reaction spread over the attacker's stun)
  GOTO back / HOLD / return-to-neutral   -> "loop from N" / "hold" / default next
  move tables A/B/C + attack boxes       -> "move" blocks; boxes in pixels relative to the
                                            hotspot, timed in tics from the start of the move

The output is derived from your copy of the game: keep it out of version control.
"""
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import extract_scripts as X  # noqa: E402

GAME = sys.argv[1] if len(sys.argv) > 1 else 'primalrage'
OUT = sys.argv[2] if len(sys.argv) > 2 else os.path.join('creatures', 'primalrage')

XSCALE, YSCALE = 0.81, 0.708  # PC pixels per arcade unit
ENDS = {0x370C0, 0x3618C, 0x35058, 0x383A0, 0x3CAFC}  # natives that end the animation
BREATHE, STUNDUR, DUR3 = 0x38294, 0x3A214, 0x36658

# What PRAGE.EXE doesn't say (select-screen frames, walk speeds, body width,
# the second player's colours) -- carried over from Clayfight's own tables.
META = {
    0: dict(file='sauron', alt=0, select=(130, 138), speed=(1.6, 1.2, 7.5), width=44),
    1: dict(file='blizzard', alt=0, select=(91, 96), speed=(1.5, 1.2, 7.8), width=40),
    2: dict(file='talon', alt=1, select=(141, 146), speed=(2.0, 1.6, 8.2), width=36),
    3: dict(file='vertigo', alt=0, select=(149, 154), speed=(1.5, 1.2, 7.5), width=40),
    4: dict(file='armadon', alt=1, select=(125, 90), speed=(1.3, 1.0, 7.0), width=48),
    5: dict(file='diablo', alt=2, select=(113, 112), speed=(1.6, 1.2, 7.5), width=44),
    6: dict(file='chaos', alt=0, select=(97, 104), speed=(1.5, 1.2, 7.8), width=38),
}

# Player-one object descriptors, indexed in character order. Their +16 field
# identifies the actual palette; Vertigo uses 1, the other six use 3.
PLAYER_OBJECTS = [0x10A7AC, 0x10A7D4, 0x10A824, 0x10A84C, 0x10A7FC, 0x10A874, 0x10A89C]

# Engine animations and the core row each comes from.
STANDARD = [
    ('stand', 0), ('crouch', 1), ('walkf', 7), ('walkb', 10), ('jump', 4), ('land', 13),
    ('block', 38), ('crouchblock', 40), ('hit', 42), ('crouchhit', 43), ('hithard', 44),
    ('fall', 46), ('down', 52), ('getup', 13), ('dizzy', 54), ('win', 58),
]
ROWDUR = [2, 2, 3, 2, 2, 2, 1, 3, 1, 1, 3, 1, 1, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,
          2, 2, 2, 2, 2, 2, 2, 3, 3, 3, 2, 3, 3, 3, 3, 3, 2, 3, 2, 3, 3, 3, 3, 3, 1, 1, 4, 3, 3, 3, 3]
GROUPS = ['stand', 'close', 'crouch', 'jump', 'power', 'air']  # move ids 0..23, 4 buttons each
BUTTONS = ['hq', 'hf', 'lq', 'lf']


class Converter:
    def __init__(self, game):
        self.mem = X.Mem(os.path.join(game, 'PRAGE.EXE'))
        self.gra = X.Gra(game)
        self.frames = X.Frames(self.mem, self.gra)
        self.vm = X.VM(self.mem)
        self.warnings = []

    def ref(self, frameid):
        """frame id -> ('REXP1', index) or None"""
        lk = self.frames.lookup(frameid)
        if not lk or lk.get('index') is None:
            return None
        return lk['file'].upper()[3:-4], lk['index']

    def cycle(self, base, table):
        """frame ids the breathing index walks through (same rule as p_script.c)"""
        ids, first = [], None
        for n in range(64):
            fid = self.mem.u16(base + 2 * n) if table else base + n
            if table and fid & 0x8000:
                break
            r = self.ref(fid)
            if not r:
                break
            if first is None:
                first = r[0]
            elif r[0] != first:
                break
            ids.append(fid)
        return ids

    def flatten(self, start, duration):
        """Play a script. Returns frames [{id, flip, ticks, dx}], end, stun."""
        m, vm = self.mem, self.vm
        var = {0x40: 0, 0x41: 0, 0x42: 0, 0x43: 0, 0x44: 0, 0x45: 0}
        ip, dur, timing, table, loopc = start, float(duration), None, None, 0
        out, seen, dx, stun = [], {}, 0.0, None

        def show(word):
            nonlocal dx
            t = float(m.u8(timing + var[0x40])) if timing else dur
            # after the stun call, frames that run at the script duration last
            # one share of the attacker's stun instead
            shares = 1.0 if stun and not timing else 0.0
            out.append(dict(id=word & 0x7FFF, flip=bool(word & 0x8000), ticks=t, dx=dx, shares=shares))
            dx = 0.0

        def tabval(ins):
            if ins['mode'] == 0x6000:
                if table is None:
                    return 0
                tt, i = ins['tabletype'] & 0xF000, var.get(ins['var'], 0)
                addr = {0x1000: table + 2 * i, 0x2000: table + 2 * i + 1}.get(tt, table + i)
                return m.s8(addr)
            if ins['mode'] == 0x2000:
                return var.get(ins['var'], 0)
            return ins.get('imm', 0)

        for _ in range(4000):
            key = (ip, tuple(sorted(var.items())), loopc, timing, table, dur)
            if key in seen:
                return out, ('loop', seen[key]), stun
            seen[key] = len(out)
            ins = vm.decode(ip)
            if ins['kind'] == 'frame':
                show(ins['frame'])
                ip = ins['next']
                continue
            o, nxt = ins['opc'], ins['next']
            if o == 0xD:  # FRAME_SELECT
                nx = vm.decode(nxt)
                if ins['var'] == 0x40 and nx.get('opc') == 0x10 and nx.get('ptr') == BREATHE:
                    ids = self.cycle(ins.get('table', ins.get('base')), 'table' in ins)
                    if out and ids and out[-1]['id'] == ids[0]:
                        out.pop()
                    first = len(out)
                    for fid in ids:
                        show(fid)
                    return out, ('wander', first), stun
                v = var.get(ins['var'], 0)
                show(ins['base'] + v if 'base' in ins else m.u16(ins['table'] + 2 * v))
                ip = nxt
                continue
            if o in (0, 1):
                return out, ('hold', None), stun
            if o == 3:
                ip = ins['ptr']
                continue
            if o == 4:
                loopc += 1
                if loopc <= ins['sel']:
                    ip = ins['ptr']
                    continue
                loopc = 0
            elif o == 6:
                v = var.get(ins.get('var'), 0)
                cases = ins['cases']
                ip = cases[v - 1] if 1 <= v <= len(cases) else nxt
                continue
            elif o == 7 and out:
                out[-1]['ticks'] += tabval(ins)
            elif o == 8 and out:
                out[-1]['ticks'] += tabval(ins) / 2  # random delay: use the average
            elif o == 0xA:
                dur += tabval(ins)
            elif o == 0xB and tabval(ins) > 0:
                dur = float(tabval(ins))
            elif o == 0xE:
                var[ins['var']] = 0
            elif o == 0xF and 'value' in ins:
                var[ins['var']] = ins['value']
            elif o == 0x17:
                var[ins['var']] = var.get(ins['var'], 0) + 1
            elif o in (0x18, 0x19):
                v = var.get(ins['var'], 0) + (1 if o == 0x18 else -1)
                var[ins['var']] = v
                if (v < ins['limit']) if o == 0x18 else (v >= ins['limit']):
                    ip = ins['jump']
                    continue
            elif o == 0x1A:
                table = ins.get('ptr')
            elif o == 0x1B:
                table = None
            elif o == 0x1C:
                timing = ins.get('ptr')
            elif o in (0x1D, 0x1E):
                timing = None
            elif o == 0x20:
                dx += tabval(ins) * XSCALE
            elif o in (0x10, 0x11, 0x15):
                if ins.get('ptr') == STUNDUR and 'arg' in ins:
                    stun = ins['arg']
                if ins.get('ptr') == DUR3:
                    dur = 3.0
                if o == 0x15 or ins.get('ptr') in ENDS:
                    return out, ('end', None), stun
            elif o == 0xC and 'ptr' not in ins:
                pass
            ip = nxt
        self.warnings.append('runaway script at %#x' % start)
        return out, ('end', None), stun

    def merge(self, frames, keep):
        """Collapse repeats of one picture (ids duplicated for timing). `keep`
        holds raw indices that must stay frame boundaries. Returns merged
        frames and a raw -> merged index map."""
        res, index = [], []
        for i, f in enumerate(frames):
            r = self.ref(f['id'])
            if not r:
                self.warnings.append('frame id %d has no picture' % f['id'])
                index.append(max(len(res) - 1, 0))
                continue
            if (res and i not in keep and not f['dx'] and res[-1]['ref'] == r and res[-1]['flip'] == f['flip']
                    and bool(res[-1]['shares']) == bool(f['shares'])):
                res[-1]['ticks'] += f['ticks']
                res[-1]['shares'] += f['shares']
            else:
                res.append(dict(ref=r, flip=f['flip'], ticks=f['ticks'], dx=f['dx'], shares=f['shares']))
            index.append(len(res) - 1)
        return res, index

    def anim(self, name, start, duration, extra=''):
        raw, end, stun = self.flatten(start, duration)
        keep = {end[1]} if end[0] in ('loop', 'wander') else set()
        frames, index = self.merge(raw, keep)
        if end[0] == 'wander':
            # the intro and the wandering cycle merge separately
            frames, index = self.merge(raw[:end[1]], set())
            cyc, _ = self.merge(raw[end[1]:], set())
            start_at = len(frames)
            frames += cyc
        flags = []
        if end[0] == 'hold':
            flags.append('hold')
        elif end[0] == 'loop':
            k = index[end[1]] if end[1] < len(index) else 0
            flags.append('loop' if k == 0 else 'loop from %d' % k)
        elif end[0] == 'wander':
            flags.append('wander' if start_at == 0 else 'wander from %d' % start_at)
        if stun:
            flags.append('stun %d' % stun)
        lines = ['anim %s%s%s' % (name, (' ' + ' '.join(flags)) if flags else '', extra)]
        for f in frames:
            tics = num(f['shares']) + 's' if f['shares'] else num(f['ticks'])
            line = '  %s#%d %s' % (f['ref'][0], f['ref'][1], tics)
            if f['flip']:
                line += ' flip'
            if abs(f['dx']) >= 0.05:
                line += ' dx %s' % num(f['dx'])
            lines.append(line)
        lines.append('end')
        loopstart = end[1] if end[0] in ('loop', 'wander') else None
        return lines, (raw, loopstart)

    def character(self, ci, meta):
        c = X.CHARS[ci]
        name, prefix = c['name'].upper(), c['prefix']
        palette_file, palette = self.gra.palette_index(self.mem.u32(PLAYER_OBJECTS[ci] + 16))
        if self.gra.index[palette_file]['name'].upper() != 'S04%s.GRA' % prefix.upper() or palette is None:
            raise ValueError('Unexpected player palette for %s' % name)
        out = [
            '# %s -- converted from PRAGE.EXE (character %d) by tools/pr2creature.py' % (name, ci),
            '# Derived from your copy of Primal Rage: do not redistribute.',
            '',
            'name %s' % name,
            'art %s left' % prefix,
            'prage %d' % ci,
            'palette %d alt %d' % (palette, meta['alt']),
            'select %d %d' % meta['select'],
            'speed %s %s %s' % tuple(num(v) for v in meta['speed']),
            'width %d' % meta['width'],
            '',
        ]
        for aname, row in STANDARD:
            lines, _ = self.anim(aname, self.mem.u32(X.SCRIPTTAB + 4 * (row * 10 + ci)), ROWDUR[row])
            out += lines + ['']

        rownames, rawframes = {}, {}
        moves = []
        for mid in range(24):
            k = ci * 64 + mid
            rowptr = self.mem.u32(0xF3488 + 20 * k + 4)
            if not X.SCRIPTTAB <= rowptr < X.SCRIPTTAB + 62 * 40:
                continue
            row = (rowptr - X.SCRIPTTAB) // 40
            slot = '%s.%s' % (GROUPS[mid // 4], BUTTONS[mid % 4])
            if row not in rownames:
                rownames[row] = slot.replace('.', '_')
                lines, rawframes[row] = self.anim(rownames[row], self.mem.u32(X.SCRIPTTAB + 4 * (row * 10 + ci)), 2)
                out += lines + ['']
            A = self.mem.rd(0x12D78C + 11 * k, 11)
            C = self.mem.rd(0xF6688 + 6 * k, 6)
            level = 'low' if C[2] in (4, 5) else 'high'
            head = 'move %s %s damage %d stun %d strength %d level %s%s' % (
                slot, rownames[row], A[0], A[1], A[4], level, ' knockdown' if A[4] >= 3 else '')
            pieces = []
            raw, loopstart = rawframes[row]
            t = 0xE68A4 + 0x3C0 * ci + 15 * mid
            for j in range(5):
                step, bi, act = self.mem.s8(t + 3 * j), self.mem.u8(t + 3 * j + 1), self.mem.u8(t + 3 * j + 2)
                bx, by, bw, bh = self.mem.rd(0x11BC84 + 4 * (ci * 256 + bi), 4)
                if step < 0 or bw < 1 or bh < 1:
                    continue
                if step >= len(raw):
                    self.warnings.append('%s %s: box at step %d past the animation' % (name, slot, step))
                    continue
                # the game places a live box from the anchor of whatever frame is
                # showing, so split it where the frame changes
                tic = sum(f['ticks'] for f in raw[:step])
                end, k, frame_end = tic + act, step, tic
                while tic < end:
                    if k >= len(raw):  # looping animation: carry on from the loop start
                        if loopstart is None:
                            break
                        k = loopstart
                    frame_end += raw[k]['ticks']
                    span = min(end, frame_end) - tic
                    if span > 0:
                        ax, ay = self.frames.anchor(raw[k]['id'])
                        x0, y0 = round(-ax * XSCALE + bx * 3), round(-ay * YSCALE + by * 3)
                        rect = (x0, y0, x0 + bw * 3, y0 + bh * 3)
                        if pieces and pieces[-1][2] == rect and pieces[-1][0] + pieces[-1][1] == tic:
                            pieces[-1][1] += span  # same rectangle, continuing
                        else:
                            pieces.append([tic, span, rect])
                        tic += span
                    k += 1
            body = ['  box %s %s  %d %d %d %d' % (num(t0), num(span), *rect) for t0, span, rect in pieces]
            moves += [head] + body + ['end', '']
        out += ['# moves: <group>.<button>; groups stand, close, crouch, jump, power, air', ''] + moves
        return meta['file'], out


def num(v):
    return str(int(v)) if float(v).is_integer() else ('%.2f' % v).rstrip('0')


def main():
    conv = Converter(GAME)
    os.makedirs(OUT, exist_ok=True)
    for ci in range(7):
        fname, lines = conv.character(ci, META[ci])
        path = os.path.join(OUT, fname + '.txt')
        with open(path, 'w') as fh:
            fh.write('\n'.join(lines).rstrip() + '\n')
        print('wrote', path)
    for w in sorted(set(conv.warnings)):
        print('warning:', w)


if __name__ == '__main__':
    main()
