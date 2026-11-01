# How Primal Rage drives its fighters, and what Clayfight takes from it

The active implementation runs **arcade 2.3 bytecode directly** for the seven
retail fighters. The 65×10 source table is at program address `0x6d964`;
the existing 62 logical rows skip three arcade-only entries. Move tables live
at `0xfbba0`, `0x6fca4`, `0x72ea4`; boxes at `0x7d328` and `0x49f7c`.
Native breathing and crouch cycles have 30 and 20 source frames respectively.
`p_script.c` reads big-endian ROM data, uses original RLE pivots and native
combat units, and maps relevant native routines to the port's simulation.
Photo creatures continue to use authored creature states.

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
