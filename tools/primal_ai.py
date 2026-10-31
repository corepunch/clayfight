#!/usr/bin/env python3
"""Decode the runtime AI tables, using PRAGE.EXE's dispatcher switches.

No name-run alignment guesses: macro, matrix and matchup pointers come from
0x4748C, 0x47314 and 0x470FC. Four-byte response records are inclusive distance
bands. Ten-byte matrices contain macro IDs; weights live separately at 0xE6764.
"""
import json
import os
import sys

MATCHUPS, MATCHUP_NAMES, WEIGHTS = 0xE6444, 0xE65D4, 0xE6764
CODES = ('rr', 'bb', 'tt', 'vv', 'ss', 'dd', 'cc')


def descriptors(mem):
    result = []
    for c in range(7):
        macro_switch = mem.u32(0x47470 + c * 4)
        matrix_switch = mem.u32(0x472F8 + c * 4)
        assert mem.u8(macro_switch + 5) == 0xBD
        assert mem.u8(matrix_switch) == 0xBD and mem.u8(matrix_switch + 5) == 0xBF
        names, macros = mem.u32(macro_switch + 1), mem.u32(macro_switch + 6)
        matrices, matrix_names = mem.u32(matrix_switch + 1), mem.u32(matrix_switch + 6)
        nmacros = (matrix_names - names) // 4
        nmatrices = 0
        while mem.cstr(mem.u32(matrix_names + nmatrices * 4)).startswith('mvm_'):
            nmatrices += 1
            assert nmatrices < 256
        result.append(dict(code=CODES[c], macros=macros, matrices=matrices,
                           macro_names=names, matrix_names=matrix_names,
                           nmacros=nmacros, nmatrices=nmatrices))
    return result


def extract_ai(mem):
    result = {}
    for character, d in enumerate(descriptors(mem)):
        macros, matrices, responses, matchups = [], [], {}, []
        for i in range(d['nmacros']):
            addr = mem.u32(d['macros'] + 4 * i)
            steps = []
            for step in range(256):
                p = addr + step * 8
                pattern = mem.u32(p)
                if pattern == 0xFFFF:
                    break
                steps.append(dict(pattern=pattern, edge=pattern & 255,
                                  held=(pattern >> 8) & 255,
                                  toward_edge=bool(pattern & 0x10000),
                                  toward_held=bool(pattern & 0x20000),
                                  away_edge=bool(pattern & 0x40000),
                                  away_held=bool(pattern & 0x80000),
                                  ticks=mem.s16(p + 4)))
            else:
                raise ValueError('Unterminated macro at %x' % addr)
            macros.append(dict(id=i, name=mem.cstr(mem.u32(d['macro_names'] + 4 * i)),
                               addr=hex(addr), steps=steps))
        for i in range(d['nmatrices']):
            addr = mem.u32(d['matrices'] + 4 * i)
            ids = list(mem.rd(addr, 10))
            assert all(k < len(macros) for k in ids)
            matrices.append(dict(id=i, name=mem.cstr(mem.u32(d['matrix_names'] + 4 * i)),
                                 addr=hex(addr), macro_ids=ids))
        for opponent in range(7):
            pair = character * 10 + opponent
            table = mem.u32(MATCHUPS + pair * 4)
            name_table = mem.u32(MATCHUP_NAMES + pair * 4)
            rows = []
            for context in range(139):
                addr = mem.u32(table + 4 * context)
                name_ptr = mem.u32(name_table + 4 * context)
                try:
                    name = mem.cstr(name_ptr) if name_ptr else None
                except IndexError:
                    name = None  # unused native debug-name slots are not all populated
                rows.append(dict(context=context, name=name, response=hex(addr)))
                if hex(addr) in responses:
                    continue
                bands = []
                for j in range(20):
                    matrix, profile, lo, hi = mem.rd(addr + 4 * j, 4)
                    bands.append(dict(matrix=matrix, weight_profile=profile, lo=lo, hi=hi))
                # The VM does not stop at hi=255: gaps can scan into a following
                # table. Keep the entire 20-record native search window as well.
                natural = next((j + 1 for j, b in enumerate(bands) if b['hi'] == 255), 20)
                responses[hex(addr)] = dict(bands=bands[:natural], search_window=bands)
            matchups.append(dict(opponent=opponent, table=hex(table), rows=rows))
        result[d['code']] = dict(character=character,
            tables={k: hex(v) for k, v in d.items() if k.endswith('names') or k in ('macros', 'matrices')},
            macros=macros, matrices=matrices, responses=responses, matchups=matchups)
    return result


def weights(mem):
    return [[list(mem.rd(WEIGHTS + profile * 80 + difficulty * 10, 10))
             for difficulty in range(8)] for profile in range(3)]


if __name__ == '__main__':
    from extract_scripts import Mem
    game = sys.argv[1] if len(sys.argv) > 1 else 'primalrage'
    mem = Mem(os.path.join(game, 'PRAGE.EXE'))
    data = dict(ai=extract_ai(mem), ai_weights=weights(mem))
    if len(sys.argv) > 2:
        with open(sys.argv[2], 'w') as f:
            json.dump(data, f, indent=2)
    for code, a in data['ai'].items():
        print('%s: %d macros, %d matrices, %d response tables, 7 x 139 matchup rows' %
              (code, len(a['macros']), len(a['matrices']), len(a['responses'])))
