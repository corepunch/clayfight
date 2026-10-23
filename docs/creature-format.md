# Creature files

One plain-text file per fighter. Clayfight loads every `*.txt` in `creatures/`
and its subfolders at startup (`-creatures <dir>` picks another folder).
The format is Doom's state tables (frames, tics, what comes next) plus the
ideas worth keeping from Primal Rage's move scripts; see
[primal-rage-scripts.md](primal-rage-scripts.md) for where each one comes from.

`tools/pr2creature.py` writes the seven Primal Rage fighters in this format
from your copy of `PRAGE.EXE` (`make creatures`). `make check` runs
`clayfight -checkanims`, which plays every animation and move from the files
and through the game's own script interpreter and compares them tic by tic.
All 266 match; the two breathing idles are left out because they're random.

## Example

```
# lines starting with # are comments (a # inside a word, as in REX#37, is not)
name SAURON
art REX left            # art files S04REX*.GRA; the drawings face left
prage 0                 # character number in PRAGE.EXE (only for -script)
palette 3 alt 0         # sub-palette of S04REX.GRA; "alt" for a mirror match
select 130 138          # S04SELMO.GRA portrait (5 frames) and name plate
speed 1.6 1.2 7.5       # walk, walk back, jump
width 44                # body width for pushing

anim stand wander       # breathing idle: drifts randomly through its frames
  REX#37 6
  REX#34 6
end

anim hit stun 10        # an "Ns" frame lasts N x (attacker's stun / 10) tics
  REXR1#1 6
  REXR1#2 6
  REXR1#3 2s
  REXR1#4 2s
end

anim stand_hf
  REXP1#10 2
  REXP1#6 2
  REXP1#7 2 dx 3.2      # root motion: move 3.2 px forward on this frame
end

move stand.hf stand_hf damage 10 stun 20 strength 2 level high
  box 3 1  -66 -85 -24 -49    # from tic 3, for 1 tic: x0 y0 x1 y1
  box 4 3  -125 -79 -86 -34
end
```

## Commands

| Line | Meaning |
|---|---|
| `name NAME` | Shown in the HUD and select screen. |
| `art PREFIX [left\|right]` | Art lives in `S04<PREFIX>*.GRA`, the voice in `S04<PREFIX>SD.GRA`. Which way the drawings face. |
| `palette N [alt M]` | Sub-palette of the base art file; `alt` for a mirror match. |
| `select PORTRAIT LABEL` | Select-screen frames in `S04SELMO.GRA`. |
| `speed WALK BACK JUMP` | Pixels per tic walking forward and back; jump take-off speed. |
| `width N` | Body width used to keep fighters apart. |
| `prage N` | Character number in `PRAGE.EXE`, used by `-script` and `-checkanims`. |

## Animations

```
anim NAME [hold] [loop [from N]] [wander [from N]] [stun N] [next NAME]
  FRAME TICS [flip] [dx PIXELS]
  ...
end
```

- **FRAME** is `PREFIX#index` (meaning `S04<PREFIX>.GRA`, frame `index`) or
  `FILE.GRA#index`.
- **TICS** is how long the frame shows, at 60 tics a second; fractions are
  fine. `Ns` means N shares of the attacker's stun (see `stun`).
- **flip** mirrors the frame. **dx** moves the fighter forward by that many
  pixels when the frame starts (backward if negative). It's ignored in the
  air, where jump physics takes over.

How an animation ends:

- **default**: standard animations follow the engine's rules (a `hit` goes
  back to `stand`, a `fall` goes to `down`, idles loop). Move animations hand
  back to whatever the move says.
- **`hold`** stays on the last frame.
- **`loop [from N]`** goes back to frame N (counting from 0).
- **`wander [from N]`** plays up to frame N, then drifts back and forth through
  the rest at random. This is Primal Rage's breathing idle.
- **`next NAME`** names the animation that follows.

**`stun N`** makes every `Ns` frame last *N shares* of the stun of the hit
that caused it, where one share is the attacker's stun ÷ N. Reactions to hard
hits play slower. Primal Rage does this in its routine `0x3A214`.

Standard animation names the engine uses: `stand walkf walkb crouch jump
land block crouchblock hit crouchhit hithard fall down getup dizzy win`. Only
`stand` is required; a missing one is skipped or falls back to `stand`. Any
other name defines an animation for a move.

## Moves

```
move GROUP.BUTTON ANIM damage N stun N strength N level high|low [knockdown] [push N]
  box TIC TICS X0 Y0 X1 Y1
end
```

- **Buttons:** `hq hf lq lf` (high quick, high fierce, low quick, low fierce).
- **Groups:** `stand` at range, `close` up close, `crouch`, `jump` straight
  up, `air` jumping across, and `power`, which Primal Rage has but Clayfight
  doesn't trigger yet. A missing `close` or `power` move falls back to `stand`,
  and a missing `air` move to `jump`.
- **Move options:**
  - `damage`: health taken.
  - `stun`: how long the victim's reaction lasts, in tics.
  - `strength`: 1 to 3, and sets the default push.
  - `level low`: must be blocked crouching.
  - `knockdown`: puts the victim on the floor.
- **box** is an attack area, live from `TIC` (counted from the start of the
  move) for `TICS` tics. Coordinates are pixels around the frame's hotspot as
  drawn (x right, y down), so mirroring is automatic.
- **No boxes at all?** The move then attacks with whatever part of the drawing
  sticks out in front of the stand pose, on the frames that reach furthest.
  New art needs no hand-made boxes this way.

Hurt boxes always come from the drawing itself.

## Photographed creatures

A creature can be made from photos instead of `.GRA` art: any frame ending in
`.png` is a photo, with the path relative to the creature file. See
`creatures/gloop/gloop.txt`.

```
name GLOOP
art photo right               # photos; the figure faces right in them
height 84                     # the first stand photo is scaled to 84 px tall
backdrop photos/backdrop.png  # optional: a shot of the empty scene

anim stand wander
  photos/stand1.png 10
  photos/stand2.png 10
end
```

When the file loads, every photo is processed together (`src/engine/w_photo.c`):

1. **Background removed.** With a `backdrop` shot, the photo is compared
   against it; without one, a smooth colour model is fitted to the photo's
   border. Colour differences count fully. Pixels darker than the paper count
   only a little, so the figure's shadow stays background. Confident pixels
   seed the figure, which then grows into weaker differences touching it, so
   white eyes and pale parts survive. Only the main blob is kept, and small
   holes are filled.
2. **One scale for everything.** Set so the first `stand` photo's figure is
   `height` world units tall (twice that many screen pixels at 640×480).
   Consistent framing between shots therefore matters. A second sample from
   each source supplies the display pixels; the original silhouette still
   determines gameplay bounds and reach.
3. **One palette.** The figure pixels of all photos are reduced to 31 colours
   (median cut), like a Primal Rage fighter. `palette 1` and `palette 2` are
   colour-swapped versions; the second player uses `alt` (default 1) in a
   mirror match.
4. **Hotspot at the feet:** the bottom of the figure, centred on its lowest
   rows.

Moves can leave out `box` lines. They then attack with whatever part of the
photo sticks out furthest, which is usually all a photographed creature needs.
`clayfight -sheet NAME -shots out/` saves every processed frame of a creature
to `out/sheet.bmp`, with hotspots marked, to check how the photos came out.

### Shooting tips

- Phone on a stand, the same distance and zoom for every shot. Take one shot
  of the empty scene first, for `backdrop`.
- Plain paper behind and under the figure, in a colour the figure doesn't
  use. Coloured paper keys more reliably than white.
- Even light, no harsh second shadows. The figure's own shadow on the paper
  is fine.
- Keep the whole figure in the frame, standing on the paper, always facing
  the same way.
- Phones save JPEG or HEIC. Convert with
  `sips -s format png IMG_0001.HEIC --out stand1.png` (macOS).
  Non-interlaced 8- and 16-bit PNGs are read directly.

`creatures/gloop/photos/` are rendered stand-ins (`tools/makeclay.py`), not
real photos. They test the pipeline, but real phone shots of real clay are
still to come.

## Not in the format yet

- Special moves: Primal Rage's input patterns are decoded but not wired up.
- Sounds per frame (photo creatures have no voice yet).
- Projectiles.
- JPEG and HEIC photos directly.
