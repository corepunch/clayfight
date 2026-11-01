# How Primal Rage drives its fighters, and what Clayfight takes from it

The active implementation runs **arcade 2.3 bytecode directly** for the seven
retail fighters. The 65×10 source table is at program address `0x6d964`;
the existing 62 logical rows skip three arcade-only entries. Move tables live
at `0xfbba0`, `0x6fca4`, `0x72ea4`; boxes at `0x7d328` and `0x49f7c`.
Native breathing and crouch cycles have 30 and 20 source frames respectively.
`p_script.c` reads big-endian ROM data, uses original RLE pivots and native
combat units, and maps relevant native routines to the port's simulation.
Photo creatures continue to use authored creature states.

Walking is a native-controlled exception: rows 7 and 10 contain indexed frame
tables, not complete executable scripts. Native routines `0x1a45c`/`0x1a712`
wrap both directions using the character counts at `0x6affe`: 24, 22, 16, 30,
20, 24 and 22 frames in roster order. `PS_StartRow` makes that distinction for
both combat and sampled previews. The port retains its three-tick walking
cadence and controller-driven movement. Reading past these bounds previously
entered unrelated animation words; Talon's backward table falls straight into
jump code, which displayed torso-pivot poses at ground height. The walk cycle
now wraps without executing those words or returning through idle.

The focused `animation_retail_walking` case checks three cycles in each
direction and facing for every original fighter, reversal, idle/crouch/attack
exits, preview bounds and collision registration. `animation_retail_air` checks
walk-to-jump transitions and airborne registration for all seven. Run them
alongside the existing 280-slot combat audit in one process:

```sh
mkdir -p build/walk-review/after
build/test-combat --retail animation_retail combat_retail --captures build/walk-review/after
```

Reviewed renderer captures: [before](review/animation/walk-before.png),
[after](review/animation/walk-after.png) at Talon's first backward wrap,
[complete backward cycle](review/animation/talon-walk.gif), and
[both directions across the roster](review/animation/roster-walking.jpg).
These reuse existing ROM artwork, palettes and authored pivots; no visual
assets or states were added. Tails may extend below the foot pivot, so the
correction does not realign frames by their lowest pixel.

Walking-pass verification on 2026-11-01: `make CFLAGS='-O2 -g -Werror'` took
7.99 s; `make test` took 3.31 s; the combined focused ROM tests and captures
took 2.17 s (48,935 checks, plus the 280-slot audit); `make lint` took 0.18 s.

## Jump, attack and recovery audit

The expanded audit checks the seven arcade fighters and all five loaded photo
fighters. It covers 1,104 complete jump/air-attack/landing sequences in both
facings and all three movement directions, with attacks started during windup,
ascent, near the apex and descent. Another 326 cases cover grounded attacks and
knockdown/recovery, and 88 photo animation/facing cases exercise every available
authored animation. Sparse photo fighters keep their existing standing-pose
jump fallback; a missing landing animation now returns to standing safely.

The audit corrected these shared runtime behaviors:

- An animation entered by the controller displays its first frame before the
  fighter update advances its timer. One-tick windup frames are no longer
  skipped. Animation-only ticking still advances explicitly for audits.
- Landing has a recovery state, so the controller lets its six/eight ROM frames
  finish before selecting neutral movement. The same rule applies to photos.
- Finite air attacks return to the descent animation and clear their attack
  state. Source air moves that explicitly loop keep looping until touchdown.
- Each arcade fighter's nominal air pivot comes from the takeoff script's first
  positive `MOVE_Y`. Starting an early air attack and returning from an air
  attack restore that pivot, instead of accumulating offsets from another pose.
- Jump physics wait for the takeoff pivot change; the grounded windup no longer
  rises into the air. Existing jump speed, gravity and fixed camera are retained.
- Signed `MOVE_Y` applies to grounded attacks and reactions as well as jumps.
  Armadon's low standing power uses a negative pivot adjustment. Drawing and
  combat boxes share it. Airborne falling poses use the touchdown floor guard.
- ROM root X follows the source artwork's facing: the original sprites face
  left, so negative native X is forward. Both world facings are checked.
- Falling waits for physical touchdown before entering the down state. The
  native ordinary recovery dispatcher at `0x1960c` selects scripts through the
  switch table at `0x19782`; the port holds their first floor pose for its
  existing 40-tick down timer, then plays the full recovery. Core row 52 is
  another reaction sequence, not a resting floor pose. Get-up previously
  reused the jump landing row. Native `0x195d6` completes ordinary recovery.

Raw script checks cover all 280 standard/move entries, including expected finite
ends and intentional cycles. Invalid frame selection clears the stale picture
and marks the interpreter failed; unsupported natives/opcodes and instruction
runaways also fail the audit. Native `0x2de96` is explicitly identified as an
auxiliary visual-effect spawn; it does not transfer animation control.

```sh
make CFLAGS='-O2 -g -Werror'
make test
build/test-combat --retail animation_ combat_retail
mkdir -p build/animation-review/after
build/test-combat --retail animation_retail_scripts animation_landing \
  animation_attack_recovery animation_root_registration animation_sequences \
  animation_ground_recovery --captures build/animation-review/after
make lint
```

The capture command exports a 280-entry `scripts.csv` with frame counts,
termination, source mirroring, silhouette bounds and cumulative vertical motion.
The [script report](review/animation/scripts.csv),
[jump/landing/recovery review](review/animation/sequences.jpg),
[attack review](review/animation/attacks.jpg), and
[photo review](review/animation/photos.jpg) catalogue the existing artwork used
by these states. No new artwork was authored; the existing photo dizzy gap
remains tracked in [issue #60](https://github.com/corepunch/nile-archive/issues/60).
Armadon's first affected low-power frame is also preserved
[before](review/animation/armadon-root-before.png) and
[after](review/animation/armadon-root-after.png), showing the corrected signed
vertical registration and forward root movement.

Verification on 2026-11-01: the full native rebuild/link with `-Werror` took
9.11 s; `make test` took 8.75 s; the combined animation/combat cases took
0.80 s (455,210 checks, 10 cases passed); the focused capture/export run took
8.48 s; `make lint` took 4.52 s. Build outputs and BMP intermediates are ignored;
the checked-in review images and CSV are the compact deliverables.

`make test-full` validates 280 standard animations and move slots,
their rendered frames and palettes, plus airborne registration and game flow.
The current fighter controller, corrected 24-slot mapping, combat-anchor
geometry, brain gauge and CPU planner are documented in
[Primal Rage combat](primal-rage-combat.md).

## Runtime division

`src/game/p_script.c` executes the supported arcade frame words, timing,
branches and native-call bindings. `p_mobj.c` selects standard rows and applies
motion; `p_user.c` selects the correct normal/power slot. `p_inter.c` applies
contact, guarding, damage, recovery and delayed dizziness. The arcade CPU
policy is port-authored and is documented separately from recovered ROM data.
Custom photographs use explicit local state chains rather than arcade bytecode.

Collision anchors are independent of RLE drawing pivots. Their table at
`0xfd148`, shape units and guard classes are documented in the combat guide.
No executable loader, converted retail creature files or DOS data is used.
