#!/usr/bin/env python3
"""Differential AI tests: production C versus original relocated x86 bytes.

Requires Unicorn (tests/requirements-ai.txt). No extracted game bytes are
committed. Only hardware entropy and the disabled debug-menu query are hooked;
the original classifier, selectors, timers, coordinate distance and facing
conversion execute as machine code. Run from the repository root.
"""
import ctypes as C
import itertools
import os
import random
import struct
import sys

sys.path.insert(0, os.path.join(os.path.dirname(__file__), '..', 'tools'))
from extract_scripts import Mem
from primal_ai import descriptors, MATCHUPS, MATCHUP_NAMES
try:
    from unicorn import Uc, UC_ARCH_X86, UC_MODE_32, UC_HOOK_CODE
    from unicorn.x86_const import (UC_X86_REG_EAX, UC_X86_REG_EBX, UC_X86_REG_ECX,
        UC_X86_REG_EDX, UC_X86_REG_ESI, UC_X86_REG_EDI, UC_X86_REG_EBP,
        UC_X86_REG_ESP, UC_X86_REG_EIP, UC_X86_REG_EFLAGS)
except ImportError:
    sys.exit('Install tests/requirements-ai.txt into a small venv; set AI_PYTHON for make checkai-original.')


class AI(C.Structure):
    _fields_ = [(n, C.c_uint32) for n in ('decisions', 'active', 'column', 'ticks',
        'previous', 'response', 'matrix', 'macro', 'weights', 'oldopponent', 'opponent')]
    _fields_ += [(n, C.c_uint8) for n in ('context', 'matrixid', 'macroid', 'profile', 'step', 'steptics')]
    _fields_ += [('reserved', C.c_uint8 * 2)]
    _fields_ += [(n, C.c_uint32) for n in ('response_name', 'matrix_name', 'macro_name')]


class View(C.Structure):
    _fields_ = [(n, C.c_uint8) for n in ('motion', 'action', 'stance', 'move', 'special', 'started',
                                      'forward', 'special_idle', 'incoming')]


def view(**kw):
    return View(**dict(dict(move=255, special=255), **kw))


def copy(a):
    return AI.from_buffer_copy(bytes(a))


class Original:
    STATE, PLAYER, OBJECT, ATTR, STACK, STOP = 0x1556B0, 0x154C70, 0x180000, 0x190000, 0x3F0000, 0x3FF000

    def __init__(self, mem):
        self.u = Uc(UC_ARCH_X86, UC_MODE_32)
        self.u.mem_map(0x10000, 0x1F0000)
        self.u.mem_map(0x300000, 0x100000)
        for obj in mem.objs:
            self.u.mem_write(obj['base'], bytes(obj['data'][:obj['vsize']]))
        self.roll, self.draws = 0, 0
        self.u.hook_add(UC_HOOK_CODE, self.entropy, begin=0x50D08, end=0x50D08)
        self.u.hook_add(UC_HOOK_CODE, lambda u, a, s, d: self.ret(0), begin=0x2E364, end=0x2E364)

    def wr(self, addr, value, fmt='I'):
        self.u.mem_write(addr, struct.pack('<' + fmt, value))

    def ret(self, eax):
        self.u.reg_write(UC_X86_REG_EAX, eax & 0xFFFFFFFF)
        sp = self.u.reg_read(UC_X86_REG_ESP)
        ip = struct.unpack('<I', self.u.mem_read(sp, 4))[0]
        self.u.reg_write(UC_X86_REG_ESP, sp + 4)
        self.u.reg_write(UC_X86_REG_EIP, ip)

    def entropy(self, u, addr, size, data):
        self.draws += 1
        self.ret((self.roll * (1 << 32) + 99) // 100)

    def call(self, addr, eax=0, edx=0):
        for reg in (UC_X86_REG_EBX, UC_X86_REG_ECX, UC_X86_REG_ESI, UC_X86_REG_EDI, UC_X86_REG_EBP):
            self.u.reg_write(reg, 0)
        self.u.reg_write(UC_X86_REG_EAX, eax)
        self.u.reg_write(UC_X86_REG_EDX, edx)
        self.u.reg_write(UC_X86_REG_EFLAGS, 2)
        self.wr(self.STACK, self.STOP)
        self.u.reg_write(UC_X86_REG_ESP, self.STACK)
        self.u.emu_start(addr, self.STOP, count=20000)
        assert self.u.reg_read(UC_X86_REG_EIP) == self.STOP, 'Original did not return at %x' % addr
        return self.u.reg_read(UC_X86_REG_EAX)

    def state(self, ai):
        self.u.mem_write(self.STATE, bytes(ai))

    def read(self):
        return bytes(self.u.mem_read(self.STATE, 64))

    def actors(self, character, opponent, own, other, distance=0, faceleft=False):
        self.wr(0x14D25C, self.ATTR)
        for i, (c, v) in enumerate(((character, own), (opponent, other))):
            p, obj = self.PLAYER + i * 0x94, self.OBJECT + i * 0x100
            self.u.mem_write(p, bytes(0x94))
            self.u.mem_write(obj, bytes(0x100))
            self.wr(p, obj)
            self.wr(p + 0x10, 0x231FC if v.special_idle else 0)
            self.wr(p + 0x40, 0x80000 | (0x80 if v.incoming else 0))
            for offset, val in ((0x52, v.motion), (0x53, v.action), (0x54, v.stance),
                                (0x5F, v.move), (0x64, v.special), (0x7A, c), (0x8A, v.started), (0x63, 0)):
                self.wr(p + offset, val, 'B')
            # CPU-owned flag is player +0x63 (0x154CD3).
            self.wr(p + 0x63, 1, 'B')
            self.wr(obj + 0x18, distance if i == 0 else 0, 'i')
            self.wr(obj + 0x24, 0 if v.special_idle else 0x40000000)
            self.wr(obj + 0x28, 0x4000 if not faceleft else 0, 'H')
            self.wr(obj + 0x34, -1 if distance > 0 else 1, 'h')
            self.wr(obj + 0x56, i, 'H')
            self.wr(self.ATTR + i * 32, 0 if faceleft else 0x8000, 'H')
            self.wr(0x155DA0 + i * 2, (0x2000 if faceleft else 0x1000) if v.forward else 0, 'H')
        self.wr(0x155210, 0)
        self.wr(0x155214, 0)
        self.wr(0x153079, 0, 'B')


def equal(actual, expected, label):
    if bytes(actual) != bytes(expected):
        fields = [(n, getattr(actual, n), getattr(AI.from_buffer_copy(expected), n))
                  for n, _ in AI._fields_ if n != 'reserved' and getattr(actual, n) != getattr(AI.from_buffer_copy(expected), n)]
        raise AssertionError('%s: C/original %r' % (label, fields))


def main():
    game = sys.argv[1] if len(sys.argv) > 1 else 'primalrage'
    libpath = sys.argv[2] if len(sys.argv) > 2 else 'build/ai-oracle.so'
    mem = Mem(os.path.join(game, 'PRAGE.EXE'))
    lib = C.CDLL(os.path.abspath(libpath))
    lib.X_Init.argtypes = [C.c_char_p]
    assert lib.X_Init(os.fsencode(os.path.join(game, 'PRAGE.EXE')))
    assert lib.PA_Init()
    lib.PA_Observe.argtypes = [C.POINTER(AI), C.POINTER(View)]
    lib.PA_Response.argtypes = [C.POINTER(AI), C.c_int, C.c_int, C.POINTER(View)]
    lib.PA_Matrix.argtypes = [C.POINTER(AI), C.c_int, C.c_int]
    lib.PA_Macro.argtypes = [C.POINTER(AI), C.c_int, C.c_int, C.c_int]
    lib.PA_Tick.argtypes = [C.POINTER(AI), C.c_int, C.c_int, C.POINTER(View), C.POINTER(View), C.c_int, C.c_int, C.c_int]
    lib.PA_Tick.restype = C.c_uint32
    lib.PA_Input.argtypes = [C.c_uint32, C.c_bool]
    lib.PA_Input.restype = C.c_uint16
    original = Original(mem)
    desc = descriptors(mem)
    checks = 0
    rng = random.Random(0x50524149)

    # Classify actual raw player-state combinations, including attack memory,
    # native special states, direction and incoming-object predicate.
    observations = [view(), view(motion=5, stance=1), view(motion=21, stance=1),
        view(motion=4, stance=2), view(motion=1), view(motion=1, forward=1),
        view(motion=7), view(action=11), view(action=6, special=34),
        view(special_idle=1), view(special_idle=1, stance=2), view(action=3)]
    observations += [view(action=action, move=move, started=started, incoming=incoming)
        for action, move, started, incoming in itertools.product((7, 8), (0, 7, 23, 63), (0, 1), (0, 1))]
    for old in range(14):
        for other in observations:
            a = AI(opponent=old)
            original.actors(0, 2, view(), other, 6400)
            original.state(a)
            original.call(0x46EE8, eax=1)
            lib.PA_Observe(C.byref(a), C.byref(other))
            equal(a, original.read(), 'observe')
            checks += 1
    print('original x86: %d classifier cases passed' % checks, flush=True)

    # Every matchup and all reachable context dispatches, not just mirror fights.
    response_count = 0
    for character, opponent in itertools.product(range(7), repeat=2):
        cases = [(state, move, 255, started) for state, move, started in
                 itertools.product((0, 1), range(64), (0, 1))]
        cases += [(11, 255, special, 0) for special in list(range(139)) + [255]]
        cases += [(state, 255, 255, 0) for state in (2, 3, 4, 5, 6, 7, 8, 9, 12, 13)]
        for state, move, special, started in cases:
            other = view(move=move, special=special, started=started)
            a = AI(opponent=state)
            original.actors(character, opponent, view(), other)
            original.state(a)
            original.call(0x471B8)
            lib.PA_Response(C.byref(a), character, opponent, C.byref(other))
            equal(a, original.read(), 'response %d/%d/%d' % (character, opponent, state))
            response_count += 1
    print('original x86: %d matchup/context dispatches passed' % response_count, flush=True)

    # Check boundary-1/boundary/boundary+1 for every distinct runtime response.
    distance_count = 0
    for character in range(7):
        seen = set()
        original.actors(character, 0, view(), view())
        for opponent, context in itertools.product(range(7), range(139)):
            table = mem.u32(MATCHUPS + (character * 10 + opponent) * 4)
            addr = mem.u32(table + context * 4)
            if addr in seen:
                continue
            seen.add(addr)
            distances = {0, 1, 32, 64, 128, 192, 254, 255, 256}
            for i in range(20):
                for bound in (mem.u8(addr + 4 * i + 2), mem.u8(addr + 4 * i + 3)):
                    distances.update(max(0, b) for b in (bound - 1, bound, bound + 1))
            for distance in sorted(distances):
                separation = distance * 64 + (distance % 3 - 1)
                a = AI(response=addr)
                original.wr(original.OBJECT + 0x18, separation, 'i')
                original.state(a)
                original.call(0x47314)
                lib.PA_Matrix(C.byref(a), character, separation)
                equal(a, original.read(), 'distance %d/%x/%d' % (character, addr, separation))
                distance_count += 1
    print('original x86: %d distance-boundary cases passed' % distance_count, flush=True)

    # Weighted macro selection: all 100 rolls, 8 difficulties, 3 profiles.
    # Enter 0x47531 after the separately verified response/matrix selectors.
    macro_count = 0
    for character, d in enumerate(desc):
        original.actors(character, 0, view(), view())
        for profile, difficulty, roll in itertools.product(range(3), range(8), range(100)):
            matrixid = (profile * 8 + difficulty) % d['nmatrices']
            a = AI(matrix=mem.u32(d['matrices'] + matrixid * 4), profile=profile)
            original.state(a)
            original.wr(0x155788, difficulty)
            original.roll = roll
            # Tail needs EBP = macro pointer table and [ESP] = name table.
            # Use a tiny test-only trampoline to supply the native call frame.
            stub = b'\x68' + struct.pack('<I', d['macro_names']) + b'\xbd' + struct.pack('<I', d['macros'])
            stub += b'\xe9' + struct.pack('<i', 0x47531 - (0x1A0000 + len(stub) + 5))
            # Restore the registers popped by the native epilogue, then return.
            original.u.mem_write(0x1A0000, stub)
            original.u.ctl_remove_cache(0x1A0000, 0x1A0040)
            for offset in range(0, 28, 4): original.wr(original.STACK + offset, original.STOP if offset == 24 else 0)
            original.u.reg_write(UC_X86_REG_EDX, 0)
            original.u.reg_write(UC_X86_REG_ESP, original.STACK)
            original.u.emu_start(0x1A0000, original.STOP, count=20000)
            assert original.u.reg_read(UC_X86_REG_EIP) == original.STOP
            lib.PA_Macro(C.byref(a), character, difficulty, roll)
            equal(a, original.read(), 'macro %d/%d/%d/%d' % (character, profile, difficulty, roll))
            macro_count += 1
    print('original x86: %d weighted macro selections passed' % macro_count, flush=True)

    # Every macro in both directions, executed through the native full driver.
    # Inactive own action freezes selection but original playback keeps running.
    playback_count = 0
    for character, d in enumerate(desc):
        for macro in range(d['nmacros']):
            ptr = mem.u32(d['macros'] + macro * 4)
            for faceleft in (False, True):
                a = AI(active=1, macro=ptr)
                for tick in range(4096):
                    own, other = view(action=3), view()
                    original.actors(character, 0, own, other, 6400, faceleft)
                    original.state(a)
                    original.call(0x47E5C)
                    raw = struct.unpack('<H', original.u.mem_read(0x155DA0, 2))[0]
                    pattern = lib.PA_Tick(C.byref(a), character, 0, C.byref(own), C.byref(other), 6400, 3, 0)
                    actual = lib.PA_Input(pattern, faceleft)
                    assert raw == actual, ('input', character, macro, tick, raw, actual)
                    equal(a, original.read(), 'playback %d/%d/%d' % (character, macro, tick))
                    playback_count += 1
                    if not a.active:
                        break
                else:
                    raise AssertionError('macro did not terminate')
    print('original x86: %d macro playback tics (both facings) passed' % playback_count, flush=True)

    # Multi-tic decision streams include selection, interruptions, air immunity,
    # action lockout, macro completion, changing matchups, distance and RNG draws.
    stream_count = 0
    for character, opponent, difficulty in itertools.product(range(7), range(7), (0, 3, 6)):
        a = AI()
        original.state(a)
        for tick in range(180):
            other = observations[(tick // 6) % len(observations)]
            own = view() if tick % 40 < 25 else view(action=3, stance=2 if tick % 2 else 0)
            distance = int((tick % 70 - 35) * 200)
            roll = rng.randrange(100)
            original.actors(character, opponent, own, other, distance, bool(tick % 2))
            original.state(a)
            original.roll = roll
            original.wr(0x155788, difficulty)
            before = original.draws
            original.call(0x47E5C)
            raw = struct.unpack('<H', original.u.mem_read(0x155DA0, 2))[0]
            decisions = a.decisions
            pattern = lib.PA_Tick(C.byref(a), character, opponent, C.byref(own), C.byref(other), distance, difficulty, roll)
            assert raw == lib.PA_Input(pattern, bool(tick % 2)), ('stream input', character, opponent, tick)
            assert original.draws - before == a.decisions - decisions
            equal(a, original.read(), 'stream %d/%d/%d/%d' % (character, opponent, difficulty, tick))
            stream_count += 1
    print('original x86: %d integrated decision-stream tics passed' % stream_count, flush=True)
    print('PASS: %d original-byte differential cases; no AI routine replaced by a hook' %
          (checks + response_count + distance_count + macro_count + playback_count + stream_count))


if __name__ == '__main__':
    main()
