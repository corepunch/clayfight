# How Primal Rage drives its fighters, and what Clayfight takes from it

Status: done the recommended way. `tools/pr2creature.py` converts the game's
move scripts and tables into creature files ([creature-format.md](creature-format.md)),
and those files are what Clayfight plays. The game's bytecode still runs
directly with `-script` (`src/engine/w_exe.c`, `src/game/p_script.c`) as a reference.
`make check` plays all 266 animations and moves both ways and compares
pictures, timing, root motion and attack boxes tic by tic: they match.

## What the original does

**Art and behaviour are split, Doom style.** The `.GRA` files hold pictures:
frames with a hotspot, 31-colour sub-palettes, PCM sounds. Everything else is
compiled into the executable as data tables, plus native code.

| Piece | Where | Shape |
|---|---|---|
| Frame ids | `0xF8A74`, 18,232 entries | `file id << 23 \| frame record offset`. One global namespace for every picture. Ids repeat an image to stretch timing (Sauron's stance has 3 ids per image). |
| Frame anchors | `0x124A0C` | 2 bytes per frame id, in arcade units. The reference point for hit boxes. |
| Animation scripts | 62 core rows × 7 fighters at `0x118298`, plus ~30 code-referenced scripts each (specials, fatalities, victories) | 16-bit bytecode, 47 opcodes. |
| Move table | 64 moves per fighter, 3 parallel tables | Damage, stun (`A[1]`), strength (`A[4]`), hit class (`C[2]`), score, link to a row script or to a native handler for specials. |
| Attack boxes | 5 per move | (step, box, active tics); boxes are (x, y, w, h) in 3-pixel units, placed from the frame anchor. |
| Special inputs | 32 per fighter | 8-step patterns: require / reject / abort / time window, facing-relative. |
| AI | per fighter and opponent | Distance bands (`atmn_*`) select ten-slot macro matrices (`mvm_*`); separate difficulty weights select a joystick macro (`mv_*`). See [the AI specification](primal-rage-ai.md). |

**The bytecode is a mini-language, not a scripting language.** Words below
0x8000 show a frame. The opcodes cover:
- Timing: per-script duration, per-frame timing tables, delays, random delays.
- Flow: goto, counted loops, switch, inc/dec-and-branch.
- Variables: six per object, plus a global array.
- Motion: root-motion tables (`MOVE_X` / `MOVE_Y` indexed by a loop variable), velocities.
- Other: spawns, sounds.
- Native calls: `CALL`, `CALL_ARG`, `CALL_YIELD`.

There is no general arithmetic and no access to game state except through
variables that native code sets.

**Native code owns the meaning.** Scripts constantly call into the game:
- `0x38294` drifts the stance's frame index randomly back and forth ("breathing").
- `0x3a214` stretches a hit reaction to the attacker's stun value.
- `0x370C0` hands control back to the game ("back to neutral").
- Specials and fatalities start from native handlers, not from the tables.

So half of what a move *is* lives in x86 code. That's the same division as
Doom's `info.c` states plus `A_*` action functions, and Quake 2's `mframe_t`
plus think functions, but with a much richer frame-level language.

## What Clayfight takes from the game

Through the converted creature files (and the same in `-script` mode):
- every animation's frames, order, timing, loops and breathing idle;
- root motion for lunges;
- damage, stun-scaled hit reactions, strength (heavy moves knock down);
- the game's own attack boxes, keyed by animation step;
- each fighter's palette (sub-palette 1 for Vertigo, 3 for the other six,
  read from the game's player object descriptors).
- original CPU matchup/distance dispatch, difficulty weights and input macros,
  read directly from `PRAGE.EXE`; [checked against original x86](primal-rage-ai.md).

Native routines are bound by address to C functions in `p_script.c` (12 so
far; no unknown calls remain in the core rows). The converter turns the ones
that matter into format features: `wander`, `stun N` with `Ns` tics, and a
fixed duration.

Still Clayfight's own logic:
- which row plays when (walk, block, fall, get up);
- jump physics;
- hurt boxes (from sprite silhouettes);
- the adapter from Clayfight's fighter states/positions to native AI observations,
  and deterministic randomness replacing the original hardware-timer entropy;
- rounds, stage layout, unmapped menu palettes, sound triggers.

Not wired yet: special moves (input tables are decoded; the handlers are
native code), fatalities and victories that spawn objects, projectiles
(`SPAWN`).

Inferred rather than read, so verify in play:
- names of the 62 rows;
- what the six move groups mean (0–3 stand, 4–7 stand close, 8–11 crouch,
  12–15 jump straight, 20–23 jump across; 16–19 is a 1.5× damage variant of
  unknown trigger);
- hit class → high/low (4 and 5 treated as low);
- knockdown = strength 3;
- the tick rate (60 Hz assumed; their durations of 2–3 tics per frame look
  right at 60).

## Evaluation

### What their model does better than our hand tables

1. **Frame order and timing are authored, not implied.** Their scripts play
   files out of order (Sauron's lunge is `P1#10,6,7,8,9,0,1`), reuse frames,
   ping-pong and hold frames for odd lengths. My hand mapping assumed file
   order and got most moves visibly wrong. Any format we keep must express an
   explicit frame list with per-frame durations.
2. **Hit boxes are separate from the art and keyed to animation steps.**
   Designers tune when a hit is live without touching frames.
3. **Hit properties sit on the move, not the animation.** Damage, stun,
   strength and class live in a move table that points at an animation. Close
   and far versions reuse one animation with different numbers.
4. **The data-driven parts are a good template for kids' input:** special
   moves as require/reject/window input patterns, and AI as joystick macros.

### What not to copy

1. **Behaviour split across bytecode and native code.** You can't understand
   or author a move without reading x86 handlers. For kids' content,
   everything about a move must be in the data.
2. **The opaque global frame-id namespace,** with duplicated ids standing in
   for timing.
3. **Boxes in arcade units relative to anchors** that only exist for their art.
4. **Variables, branches and native calls as the extension mechanism.** It's
   powerful, but nobody outside the engine team could author it.

### Recommendation: keep their ideas, convert to our format

Use the Doom-style state table as the one animation system, extended with the
Primal Rage concepts that matter. Ship it as a per-creature text file loaded
at runtime, so a new clay figure is photos plus one file.

| Their concept | In our format |
|---|---|
| Frame words + durations + timing tables | explicit frame list, per-frame tics |
| Breathing idle (`0x38294`) | an `idle: wander` flag on an animation |
| Stun-scaled reactions (`0x3a214`) | `tics: stun/N` on reaction animations |
| `MOVE_X` root motion | optional per-frame dx |
| Move table A/B/C | move entries: animation, damage, stun, strength, level |
| Attack boxes per step | per-frame boxes; derived from silhouettes by default (no work for kids), overridable |
| Special-input patterns | the same 8-step require/reject/window format, written as text (`down, toward+HF`) |
| AI macros | the same idea, as text |

Then convert Primal Rage into that format with a one-time tool (done:
`tools/pr2creature.py`). The
extractor's linear playback already flattens all 434 core sequences to plain
(frame, tics) lists that end normally, and the converter would carry the move
and box data across. Keep the VM (`p_script.c`) as a reference mode until the
converted data plays identically side by side (it now does: `make check`),
then retire it once nothing new needs checking against the original. The result is
one code path, with Primal Rage as the first content pack in the format the
kids' creatures will use.

Following their lead instead (a bytecode VM plus native hooks as our core)
would keep perfect fidelity for Primal Rage. But it would make the kids' side
much harder, since every behaviour new art needs would mean new native
routines.
