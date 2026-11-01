#!/usr/bin/env python3
"""makeclay.py -- render stand-in "photos" of a clay creature for testing.

  python3 tools/makeclay.py [OUT_DIR]        (default: creatures/gloop/photos)

There are no real photos yet, so this ray-traces a lumpy clay monster
("Gloop") on a paper backdrop, the way a kid would shoot one on a desk:
soft shadows on the paper, clay texture, a slightly different camera
position and exposure for every shot, sensor noise and vignetting. It also
renders the empty backdrop. That's enough to exercise Clayfight's photo
pipeline (background removal, scaling, palette reduction, hotspot at the
feet); real phone photos remain the real test. Stdlib only; renders the
shots in parallel.
"""
import math
import os
import random
import struct
import sys
import zlib
from multiprocessing import Pool

W, H = 360, 270
OUT = sys.argv[1] if len(sys.argv) > 1 else os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', 'data', 'creatures', 'gloop', 'photos')

# --- vector helpers --------------------------------------------------------


def add(a, b): return (a[0] + b[0], a[1] + b[1], a[2] + b[2])
def sub(a, b): return (a[0] - b[0], a[1] - b[1], a[2] - b[2])
def mul(a, s): return (a[0] * s, a[1] * s, a[2] * s)
def dot(a, b): return a[0] * b[0] + a[1] * b[1] + a[2] * b[2]


def norm(a):
    n = math.sqrt(dot(a, a)) or 1.0
    return (a[0] / n, a[1] / n, a[2] / n)


def rotz(p, ang, pivot=(0, 0, 0)):
    c, s = math.cos(ang), math.sin(ang)
    x, y = p[0] - pivot[0], p[1] - pivot[1]
    return (pivot[0] + c * x - s * y, pivot[1] + s * x + c * y, p[2])

# --- primitives: sphere / axis-aligned-in-local-frame ellipsoid / capsule ---


class Ellipsoid:
    def __init__(self, c, r, color, ang=0.0):
        self.c, self.r, self.color, self.ang = c, r, color, ang
        self.bound = max(r)

    def hit(self, o, d):
        # into the ellipsoid's frame: rotate by -ang about z, scale to a unit sphere
        oc = rotz(sub(o, self.c), -self.ang)
        dd = rotz(d, -self.ang)
        oc = (oc[0] / self.r[0], oc[1] / self.r[1], oc[2] / self.r[2])
        dd = (dd[0] / self.r[0], dd[1] / self.r[1], dd[2] / self.r[2])
        a = dot(dd, dd)
        b = dot(oc, dd)
        cc = dot(oc, oc) - 1
        disc = b * b - a * cc
        if disc < 0:
            return None
        t = (-b - math.sqrt(disc)) / a
        if t < 1e-3:
            return None
        return t

    def normal(self, p):
        q = rotz(sub(p, self.c), -self.ang)
        n = (q[0] / self.r[0] ** 2, q[1] / self.r[1] ** 2, q[2] / self.r[2] ** 2)
        return norm(rotz(n, self.ang))


class Capsule:
    def __init__(self, a, b, r, color):
        self.a, self.b, self.r, self.color = a, b, r, color
        self.c = mul(add(a, b), 0.5)
        self.bound = math.sqrt(dot(sub(b, a), sub(b, a))) / 2 + r

    def hit(self, ro, rd):  # Inigo Quilez's ray-capsule intersection
        ba, oa = sub(self.b, self.a), sub(ro, self.a)
        baba, bard, baoa = dot(ba, ba), dot(ba, rd), dot(ba, oa)
        rdoa, oaoa = dot(rd, oa), dot(oa, oa)
        a = baba - bard * bard
        b = baba * rdoa - baoa * bard
        c = baba * oaoa - baoa * baoa - self.r * self.r * baba
        h = b * b - a * c
        if h >= 0 and a > 1e-9:
            t = (-b - math.sqrt(h)) / a
            y = baoa + t * bard
            if 0 < y < baba and t > 1e-3:
                return t
            oc = oa if y <= 0 else sub(ro, self.b)
            b = dot(rd, oc)
            c = dot(oc, oc) - self.r * self.r
            h = b * b - c
            if h > 0:
                t = -b - math.sqrt(h)
                if t > 1e-3:
                    return t
        return None

    def normal(self, p):
        ba, pa = sub(self.b, self.a), sub(p, self.a)
        h = max(0.0, min(1.0, dot(pa, ba) / dot(ba, ba)))
        return norm(sub(pa, mul(ba, h)))

# --- the creature ------------------------------------------------------------


CLAY = (214, 92, 52)  # orange-red clay
BELLY = (236, 184, 96)
HORN = (240, 226, 196)
WHITE = (245, 245, 240)
BLACK = (25, 22, 24)


def gloop(pose):
    """Primitives for a pose. The creature faces +x (to the right)."""
    p = dict(lean=0, bob=0, head=0, armf=(-0.4, 0.9), armb=(0.3, 1.0), legf=0, legb=0,
             tail=0, squash=1.0, spin=0, mouth=0)
    p.update(pose)
    sq = p['squash']
    hip = (0, 26 * sq + p['bob'], 0)
    prims = []

    def body_pt(x, y, z):  # a point on the torso, leaning about the hip
        return rotz((x, hip[1] + y * sq, z), p['lean'], hip)

    # legs: thigh + shin capsules and a flat foot, swinging about the hip
    for side, swing in ((7, p['legf']), (-7, p['legb'])):
        knee = add(hip, (math.sin(swing) * 13, -math.cos(swing) * 13 * sq, side))
        ankle = add(knee, (math.sin(swing * 0.4) * 11, -11 * sq, 0))
        prims.append(Capsule(add(hip, (0, 0, side)), knee, 6.5, CLAY))
        prims.append(Capsule(knee, ankle, 5.2, CLAY))
        prims.append(Ellipsoid(add(ankle, (3, -1.5, 0)), (8, 3.2, 5), CLAY))
    # torso, belly, tail
    prims.append(Ellipsoid(body_pt(0, 14, 0), (15, 19 * sq, 13), CLAY, p['lean']))
    prims.append(Ellipsoid(body_pt(5, 12, 0), (10, 15 * sq, 10.5), BELLY, p['lean']))
    tailbase = body_pt(-12, 6, 0)
    for i in range(6):
        t = i / 5
        x = -14 - i * 7
        y = 6 - i * 2.5 + math.sin(t * 3 + p['tail']) * 3
        c = rotz(body_pt(x, y, math.sin(t * 2 + p['tail']) * 4), p['spin'] * 0.9, tailbase)  # lies flat when down
        prims.append(Ellipsoid(c, (7 - i * 0.8, 6 - i * 0.7, 6 - i * 0.7), CLAY))
    # arms: shoulder -> elbow -> fist, angles relative to "straight down"
    shoulder = body_pt(4, 26, 0)
    for side, (a1, a2), col in ((11, p['armf'], CLAY), (-11, p['armb'], CLAY)):
        s = add(shoulder, (0, 0, side))
        elbow = add(s, (math.sin(a1 + p['lean']) * 11, -math.cos(a1 + p['lean']) * 11, 0))
        hand = add(elbow, (math.sin(a2 + p['lean']) * 10, -math.cos(a2 + p['lean']) * 10, 0))
        prims.append(Capsule(s, elbow, 4.2, col))
        prims.append(Capsule(elbow, hand, 3.8, col))
        prims.append(Ellipsoid(hand, (4.8, 4.5, 4.5), col))
    # head with snout, horns, eyes
    neck = body_pt(6, 33, 0)
    hang = p['lean'] + p['head']
    hc = add(neck, rotz((5, 8, 0), hang))
    prims.append(Ellipsoid(hc, (12, 10.5, 11), CLAY, hang))
    prims.append(Ellipsoid(add(hc, rotz((10, -3 - p['mouth'], 0), hang)), (9, 5.5 + p['mouth'] * 0.5, 7.5), CLAY, hang))
    for z in (-5, 5):
        base = add(hc, rotz((-3, 8, z), hang))
        tip = add(hc, rotz((-8, 16, z * 1.3), hang))
        prims.append(Capsule(base, tip, 2.0, HORN))
        eye = add(hc, rotz((6, 4, z * 1.2), hang))
        prims.append(Ellipsoid(eye, (3.6, 3.6, 3.6), WHITE))
        prims.append(Ellipsoid(add(eye, rotz((2.6, 0.3, z * 0.12), hang)), (1.6, 1.8, 1.6), BLACK))
    if p['spin']:  # lying down / falling: rotate everything about the feet
        pivot = (0, 0, 0)
        for pr in prims:
            if isinstance(pr, Capsule):
                pr.a, pr.b = rotz(pr.a, p['spin'], pivot), rotz(pr.b, p['spin'], pivot)
                pr.c = mul(add(pr.a, pr.b), 0.5)
            else:
                pr.c = rotz(pr.c, p['spin'], pivot)
                pr.ang += p['spin']
        def lowest(pr):
            if isinstance(pr, Capsule):
                return min(pr.a[1], pr.b[1]) - pr.r
            return pr.c[1] - max(pr.r[0], pr.r[1])
        shift = (22, -min(lowest(pr) for pr in prims) + 0.5, 0)  # back on the table, in frame
        for pr in prims:
            if isinstance(pr, Capsule):
                pr.a, pr.b = add(pr.a, shift), add(pr.b, shift)
                pr.c = mul(add(pr.a, pr.b), 0.5)
            else:
                pr.c = add(pr.c, shift)
    return prims


POSES = {
    'stand1': {},
    'stand2': dict(bob=1.2, armf=(-0.35, 0.8), tail=0.6),
    'walk1': dict(legf=0.45, legb=-0.35, armf=(-0.7, 0.5), armb=(0.6, 1.2), bob=-0.5),
    'walk2': dict(legf=0.1, legb=0.05, bob=1.0, tail=0.5),
    'walk3': dict(legf=-0.35, legb=0.45, armf=(0.2, 1.2), armb=(-0.5, 0.6), bob=-0.5, tail=1.0),
    'walk4': dict(legf=0.0, legb=0.1, bob=1.0, tail=1.5),
    'punch1': dict(lean=0.1, armf=(-1.4, -2.2), legf=-0.2, legb=0.3),
    'punch2': dict(lean=-0.25, armf=(1.45, 1.55), head=0.1, legf=0.35, legb=-0.3, mouth=2),
    'kick': dict(lean=0.35, legf=1.35, legb=-0.15, armf=(-0.9, -0.5), armb=(-0.6, 0.2), tail=1.2),
    'bite': dict(lean=-0.45, head=-0.15, mouth=3, armf=(0.6, 1.2), legf=0.3, legb=-0.4),
    'hit': dict(lean=0.45, head=0.3, armf=(-1.2, -0.6), armb=(-1.5, -0.9), mouth=2, legf=-0.25, legb=0.2),
    'crouch': dict(squash=0.68, armf=(0.5, 1.6), lean=-0.1),
    'down': dict(spin=1.45, armf=(-2.4, -2.0), legf=0.5, mouth=1),
    'win': dict(armf=(-2.9, -3.0), armb=(-2.7, -2.9), head=0.25, mouth=3, bob=1.5),
}

# --- scene ---------------------------------------------------------------------

LIGHT = norm((-0.55, 0.85, 0.5))  # key light from the upper left, in front


def hash3(x, y, z):
    h = (x * 374761393 + y * 668265263 + z * 2147483647) & 0xffffffff
    h = ((h ^ (h >> 13)) * 1274126177) & 0xffffffff
    return (h & 0xffff) / 65535.0


def noise3(p):  # value noise for the clay surface
    xi, yi, zi = int(math.floor(p[0])), int(math.floor(p[1])), int(math.floor(p[2]))
    xf, yf, zf = p[0] - xi, p[1] - yi, p[2] - zi
    u, v, w = xf * xf * (3 - 2 * xf), yf * yf * (3 - 2 * yf), zf * zf * (3 - 2 * zf)
    acc = 0.0
    for dx in (0, 1):
        for dy in (0, 1):
            for dz in (0, 1):
                wgt = (u if dx else 1 - u) * (v if dy else 1 - v) * (w if dz else 1 - w)
                acc += wgt * hash3(xi + dx, yi + dy, zi + dz)
    return acc


def trace(prims, o, d):
    best, hit = 1e9, None
    for pr in prims:
        t = pr.hit(o, d)
        if t is not None and t < best:
            best, hit = t, pr
    return best, hit


def shadow(prims, p, rng):
    lit = 0.0
    samples = 3
    for _ in range(samples):  # a small area light: soft shadow edges
        ld = norm(add(LIGHT, ((rng.random() - 0.5) * 0.18, (rng.random() - 0.5) * 0.18, (rng.random() - 0.5) * 0.18)))
        t, h = trace(prims, p, ld)
        lit += 0.0 if h else 1.0
    return lit / samples


def paper(p, wall):
    # one sheet of paper sweeping from the table up the wall: the colour
    # drifts with the light falling off, plus a faint grain
    k = 1.0 if wall else max(0.0, min(1.0, (-p[2] - 40) / 80))
    base = [a * (1 - k) + b * k for a, b in zip((224, 219, 209), (204, 207, 212))]
    g = (noise3(mul(p, 0.9)) - 0.5) * 6
    return tuple(min(255, max(0, c + g)) for c in base)


def render(job):
    name, pose, seed = job
    rng = random.Random(seed)
    prims = gloop(pose) if pose is not None else []
    # every shot: the phone moved a little, exposure changed a little
    cam = (rng.uniform(-4, 4), 58 + rng.uniform(-3, 3), 175 + rng.uniform(-8, 8))
    target = (rng.uniform(-3, 3), 34, 0)
    fwd = norm(sub(target, cam))
    right = norm((fwd[2], 0, -fwd[0]))
    right = (-right[0], -right[1], -right[2]) if right[0] < 0 else right
    up = (right[1] * fwd[2] - right[2] * fwd[1], right[2] * fwd[0] - right[0] * fwd[2], right[0] * fwd[1] - right[1] * fwd[0])
    exposure = rng.uniform(0.93, 1.06)
    tint = (rng.uniform(0.98, 1.03), 1.0, rng.uniform(0.95, 1.01))
    fov = math.tan(math.radians(17))
    # screen-space bound of the creature, to skip rays that can't hit it
    rows = bytearray()
    for j in range(H):
        rows.append(0)
        for i in range(W):
            sx = (2 * (i + 0.5) / W - 1) * fov * W / H
            sy = (1 - 2 * (j + 0.5) / H) * fov
            d = norm(add(fwd, add(mul(right, sx), mul(up, sy))))
            t, pr = trace(prims, cam, d)
            if pr:
                p = add(cam, mul(d, t))
                n = pr.normal(p)
                bump = noise3(mul(p, 0.45)) - 0.5
                n = norm(add(n, (bump * 0.25, (noise3(mul(p, 0.5)) - 0.5) * 0.25, bump * 0.2)))
                diff = max(0.0, dot(n, LIGHT)) * shadow(prims, add(p, mul(n, 0.3)), rng)
                h = norm(sub(LIGHT, d))
                spec = max(0.0, dot(n, h)) ** 24 * 0.18
                ao = 0.75 + 0.25 * max(0.0, n[1])
                shade = (0.32 * ao + 0.78 * diff)
                grain = 1 + (noise3(mul(p, 2.2)) - 0.5) * 0.12
                col = [min(255.0, c * shade * grain + 255 * spec) for c in pr.color]
            else:
                # the backdrop: table (y=0) in front, paper wall curving up behind
                if d[1] < 0:
                    t = -cam[1] / d[1]
                    p = add(cam, mul(d, t))
                    wall = False
                else:
                    wall, p = True, add(cam, mul(d, 300))
                col = list(paper(p, wall))
                if not wall and prims:
                    lit = shadow(prims, add(p, (0, 0.2, 0)), rng)
                    col = [c * (0.62 + 0.38 * lit) for c in col]
            # camera: exposure, white balance, vignette, sensor noise
            vig = 1 - 0.22 * ((i / W - 0.5) ** 2 + (j / H - 0.5) ** 2) * 2
            for k in range(3):
                v = col[k] * exposure * tint[k] * vig + rng.gauss(0, 2.2)
                rows.append(max(0, min(255, int(v))))
    raw = zlib.compress(bytes(rows), 9)

    def chunk(t, b):
        return struct.pack('>I', len(b)) + t + b + struct.pack('>I', zlib.crc32(t + b) & 0xffffffff)
    png = b'\x89PNG\r\n\x1a\n' + chunk(b'IHDR', struct.pack('>IIBBBBB', W, H, 8, 2, 0, 0, 0)) + chunk(b'IDAT', raw) + chunk(b'IEND', b'')
    path = os.path.join(OUT, name + '.png')
    with open(path, 'wb') as fh:
        fh.write(png)
    return path


def main():
    os.makedirs(OUT, exist_ok=True)
    only = sys.argv[2:]  # optional: just these shots
    jobs = [('backdrop', None, 1)] + [(name, pose, 100 + k) for k, (name, pose) in enumerate(POSES.items())]
    jobs = [j for j in jobs if not only or j[0] in only]
    with Pool() as pool:
        for path in pool.imap_unordered(render, jobs):
            print('wrote', path, flush=True)


if __name__ == '__main__':
    main()
