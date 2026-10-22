# Clayfight

A feasibility prototype for a fighting game where kids photograph their own
stop-motion clay creatures and make them fight, the way Primal Rage digitised
stop-motion puppets. Step one is to rebuild Primal Rage's own fights and
character select on a small Doom-style engine (C99 + SDL2), running on the
original DOS game's data.

## Build and run

Put the Primal Rage PC files (`INDEX`, `S04*.GRA`, `PRAGE.EXE`, ...) in `primalrage/`, then:

```bash
make
./clayfight
```

Controls: left player uses WASD to move and R T / F G for high quick/fierce
and low quick/fierce; right player uses the arrow keys and O P / L ;. Enter
starts, Esc pauses, the backtick key shows hit boxes. Game controllers work too.
Press a right-player button on the select screen to join; otherwise you fight
the CPU.

Developer switches (Doom style): `-data <dir>`, `-creatures <dir>`,
`-fight sauron sauron [-cpu] [-stage n]`, `-warp select`, `-script`, `-boxes`,
`-checkanims`, `-sheet NAME`, `-nosound`, and a headless mode that writes BMP screenshots:
`-shots out/f -every 60 -tics 3000`.

## Layout (Doom → Clayfight)

| Doom | here | role |
|---|---|---|
| `doomdef.h` | `pr_def.h` | constants, buttons, game states |
| `i_system/i_video/i_sound` | same names | SDL2 platform layer (window, input, mixer) |
| `w_wad.c` | `w_gra.c`, `w_exe.c`, `w_png.c`, `w_photo.c` | INDEX directory + `.GRA` archives (RLE frames, palettes, PCM); tables inside `PRAGE.EXE`; photographed frames turned into an in-memory sprite file |
| `v_video.c` | `v_video.c` | 320x200 framebuffer, patches, original bitmap fonts |
| `info.c` (states, mobjinfo) | `info.c`, `p_creature.c`, `p_script.c` | creature files loaded into Doom-style `state_t` chains; or (`-script`) Primal Rage's own bytecode, with native calls bound like action functions; `p_check.c` compares the two |
| `p_mobj/p_user/p_map/p_inter/p_enemy/p_tick` | same names | fighter state machine, ticcmd control, hit boxes, damage, CPU AI, round flow |
| `r_main/r_things/r_sky` | `r_main.c`, `r_stage.c` | fighters, shadows, parallax arena |
| `st_stuff/hu_stuff` | `st_stuff.c` | health bars, clock, announcements |
| `wi_stuff/m_menu` | `sel_main.c`, `m_menu.c` | character select, title |
| `g_game/d_main/d_event/m_argv/m_random` | same names | game flow, fixed 60Hz tic loop, event queue |

As in Doom, input becomes a `ticcmd_t` each tic; the CPU opponent fills in the
same `ticcmd_t`, so humans and AI share every rule.

## Creature files and Primal Rage's own scripts

Fighters are plain-text creature files ([docs/creature-format.md](docs/creature-format.md)),
loaded at startup from `creatures/` and its subfolders. A creature can be made
from photographs: `creatures/gloop/` is a test creature built from 15 shots
(rendered stand-ins for now, see `tools/makeclay.py`). The photo pipeline
removes the background, scales every frame consistently, reduces the set to
one 31-colour palette and puts the hotspot at the feet (`w_png.c`, `w_photo.c`).
The Primal Rage fighters live in `creatures/primalrage/`. `make` builds them from your
`primalrage/PRAGE.EXE` with `tools/pr2creature.py`. The converter flattens
the game's own move scripts and tables: frame order and timing, root motion,
damage, stun, attack boxes, palettes.

The game's bytecode interpreter is kept as a reference: `-script` plays from
`PRAGE.EXE` directly (`w_exe.c`, `p_script.c`). `make check` compares the two
tic by tic, and all 266 animations and moves match. How the original works,
and why we convert instead of following it, is in
[docs/primal-rage-scripts.md](docs/primal-rage-scripts.md).
`tools/extract_scripts.py` dumps all the tables to JSON.

## What comes from the original data

* `.GRA` format, fully decoded: chunk chain (2 = RLE pixels, 5 = palettes,
  6 = frame table w/h/hotspot/offset), per-row RLE (literal / run / skip),
  1-based sub-palette indices, unsigned 8-bit 11kHz PCM in the `SD` files.
* All sprites, palettes (including each fighter's alternate colours), arenas,
  select-screen portraits and name plates, title art, both fonts, sounds.
* From `PRAGE.EXE`, via the converter: animation scripts, move table, attack
  boxes, fighter palettes.
* Still rebuilt by Clayfight: hurt boxes (from the sprite silhouettes), stage
  layouts, select-screen palettes (chosen automatically as the smoothest
  result), sound triggers, CPU AI.
