#!/usr/bin/env python3
"""grasheet.py -- render Primal Rage .GRA frames to a labelled PNG contact sheet.

Stdlib only. Used to map animations to frame numbers when writing creature files.

  python3 tools/grasheet.py OUT.png FILE [FILE...] [--pal PALFILE[:INDEX]] [--scale S] [--width W]

Without --pal each frame gets its own palette guessed like the engine does
(W_GuessPalette). Character move files have no palettes: pass the base file,
e.g. --pal S04REX.GRA:3. Frame numbers are drawn above each frame and the
hotspot is marked green. Data dir: $PRDATA or ./primalrage.
"""
import os, struct, sys, zlib

DATA = os.environ.get('PRDATA', 'primalrage')


def load(name):
    b = open(os.path.join(DATA, name.upper()), 'rb').read()
    p = 0
    pals = []
    while True:
        t, m, nxt = struct.unpack_from('<HHI', b, p)
        if m != 0x3334:
            raise ValueError('%s: bad chunk at %d' % (name, p))
        if t == 5:
            q = p + 8
            while q + 4 <= nxt:
                n = struct.unpack_from('<I', b, q)[0]
                q += 4
                pals.append([tuple(b[q + i * 4:q + i * 4 + 3]) for i in range(n)])
                q += n * 4
        if t == 6:
            break
        p = nxt
    p += 8
    frames = []
    while p + 12 <= len(b):
        w, h, x, y, o = struct.unpack_from('<HHhhI', b, p)
        frames.append((w, h, x, y, ((o >> 16) & 0x7f) << 16 | (o & 0xffff)))
        p += 12
    return b, frames, pals


def decode(b, f):
    w, h, _, _, p = f
    if not w or not h or w > 1024 or h > 1024:
        return None
    rows = []
    for _ in range(h):
        row = []
        while len(row) < w:
            c = b[p]; p += 1
            if c < 0x80:
                row += b[p:p + c]; p += c
            elif c < 0xc0:
                row += [b[p]] * (c & 0x3f); p += 1
            else:
                row += [0] * (c & 0x3f)
        rows.append(row[:w])
    return rows


def roughness(im, pal):
    s = n = 0
    for r0, r1 in zip(im, im[1:] + [None]):
        for i, a in enumerate(r0):
            if not a:
                continue
            for c in (r0[i + 1] if i + 1 < len(r0) else 0, r1[i] if r1 else 0):
                if c and a <= len(pal) and c <= len(pal):
                    A, C = pal[a - 1], pal[c - 1]
                    s += abs(A[0] - C[0]) + abs(A[1] - C[1]) + abs(A[2] - C[2]); n += 1
    return s / n if n else 1e9


def guess(im, pals):
    mx = max(max(r) for r in im)
    fit = [len(p) for p in pals if len(p) >= mx]
    if not fit:
        return pals[0] if pals else None
    return min((p for p in pals if len(p) == min(fit)), key=lambda p: roughness(im, p))


DIGITS = {'0': '111101101101111', '1': '010110010010111', '2': '111001111100111', '3': '111001111001111',
          '4': '101101111001001', '5': '111100111001111', '6': '111100111101111', '7': '111001001001001',
          '8': '111101111101111', '9': '111101111001111'}


def png(path, W, H, px):
    raw = b''.join(b'\0' + bytes(px[y * W * 4:(y + 1) * W * 4]) for y in range(H))
    def chunk(t, d):
        return struct.pack('>I', len(d)) + t + d + struct.pack('>I', zlib.crc32(t + d) & 0xffffffff)
    open(path, 'wb').write(b'\x89PNG\r\n\x1a\n' + chunk(b'IHDR', struct.pack('>IIBBBBB', W, H, 8, 6, 0, 0, 0))
                           + chunk(b'IDAT', zlib.compress(raw)) + chunk(b'IEND', b''))


def main():
    args = sys.argv[1:]
    opts = {'--pal': None, '--scale': '1', '--width': '1600'}
    files = []
    while args:
        a = args.pop(0)
        if a in opts:
            opts[a] = args.pop(0)
        else:
            files.append(a)
    if len(files) < 2:
        sys.exit(__doc__)
    out, files = files[0], files[1:]
    sc, W = float(opts['--scale']), int(opts['--width'])
    fixed = None
    if opts['--pal']:
        name, _, idx = opts['--pal'].partition(':')
        fixed = load(name)[2][int(idx or 0)]

    place, x, y, rowh = [], 0, 0, 0
    for fn in files:
        b, frames, pals = load(fn)
        x, y, rowh = 0, y + rowh + 4, 0
        for i, f in enumerate(frames):
            im = decode(b, f) or [[0]]
            w, h = max(int(len(im[0]) * sc), 14) + 3, int(len(im) * sc) + 8
            if x + w > W:
                x, y, rowh = 0, y + rowh + 2, 0
            place.append((im, x, y, i, f, fixed or (guess(im, pals) if im != [[0]] else None)))
            x += w; rowh = max(rowh, h)
    H = y + rowh + 2
    px = bytearray([30, 30, 45, 255] * W * H)

    def put(x, y, c):
        if 0 <= x < W and 0 <= y < H:
            o = (y * W + x) * 4; px[o:o + 4] = bytes([*c, 255])

    for im, ox, oy, i, f, pal in place:
        for k, ch in enumerate(str(i)):
            for j, bit in enumerate(DIGITS[ch]):
                if bit == '1':
                    put(ox + k * 4 + j % 3, oy + j // 3, (255, 255, 255))
        oy += 7
        for yy in range(int(len(im) * sc)):
            r = im[int(yy / sc)]
            for xx in range(int(len(r) * sc)):
                v = r[int(xx / sc)]
                if v:
                    put(ox + xx, oy + yy, pal[v - 1] if pal and v <= len(pal) else (255, 0, 255))
        for d in range(-2, 3):
            put(ox + int(f[2] * sc) + d, oy + int(f[3] * sc), (0, 255, 0))
            put(ox + int(f[2] * sc), oy + int(f[3] * sc) + d, (0, 255, 0))
    png(out, W, H, px)


if __name__ == '__main__':
    main()
