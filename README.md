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

## Layout

Three folders, one header each; each layer only uses the ones below it. File
names follow Doom's, so its source is a map to this one.

| Folder | Header | Files | Role |
|---|---|---|---|
| `src/platform/` | `platform.h` | `i_system i_video i_sound d_event` | SDL2: window, input, sound mixer, timing, event queue; basic types |
| `src/engine/` | `engine.h` | `w_gra w_exe w_png w_photo v_video m_random m_argv` | `.GRA` archives (Doom: `w_wad`), tables inside `PRAGE.EXE`, PNG photos turned into sprite files; 320x200 framebuffer and fonts; random numbers; command line |
| `src/game/` | `game.h` | `info p_creature p_script p_check` | fighter state tables (Doom: `info`) loaded from creature files; Primal Rage's bytecode interpreter (`-script`) and the check against it |
| | | `p_mobj p_user p_map p_inter p_enemy p_effect p_tick` | play simulation: state machine, ticcmd control, hit boxes, damage, CPU AI, particles, round flow |
| | | `r_main r_stage st_stuff s_sound` | the fight scene and arenas, HUD and announcements, sound effects |
| | | `g_game sel_main m_menu d_main` | game flow and bindings, character select (Doom: `wi_stuff`), title, startup and the 60Hz loop |

As in Doom, input becomes a `ticcmd_t` each tic; the CPU opponent fills in the
same `ticcmd_t`, so humans and AI share every rule.

## Creature files and Primal Rage's own scripts

Fighters are plain-text creature files ([docs/creature-format.md](docs/creature-format.md)),
loaded at startup from `creatures/` and its subfolders. A creature can be made
from photographs: `creatures/gloop/` is a test creature built from 15 shots
(rendered stand-ins for now, see `tools/makeclay.py`). The photo pipeline
removes the background, scales every frame consistently, reduces the set to
one 31-colour palette and puts the hotspot at the feet (`src/engine/w_photo.c`).
The Primal Rage fighters live in `creatures/primalrage/`. `make` builds them from your
`primalrage/PRAGE.EXE` with `tools/pr2creature.py`. The converter flattens
the game's own move scripts and tables: frame order and timing, root motion,
damage, stun, attack boxes, palettes.

The game's bytecode interpreter is kept as a reference: `-script` plays from
`PRAGE.EXE` directly (`src/engine/w_exe.c`, `src/game/p_script.c`). `make check` compares the two
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
