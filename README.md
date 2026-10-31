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

The game renders at **640×400**, exactly twice the DOS version's **320×200**
frame, with square pixels and nearest-neighbor presentation. Drawing and
simulation use that 320×200 coordinate space; legacy DOS sprites, scenery and
fonts enlarge 2×. Custom fighter PNGs use two pixels per world unit: the bundled
clay figures stand about 184 pixels tall, with tightly fitted canvases shared
across their poses. Transparent cutouts retain their alpha mask without a white
paper backing. The game keeps the same 31-colour palette, hotspot and simulation
silhouette.

The four saved clay action sheets are playable prototypes: `BOXER`, `LIZARD`,
`CYCLOPS`, and `CRAB`. Try `./clayfight -fight boxer lizard`. Their stand pose is
184 screen pixels tall; punch, kick, block and pain poses come from the concept
sheets. Walking and other missing reactions use fallback poses, so this is a
resolution and art experiment, not a finished animation set. Rebuild the sliced
photos and creature files with `python3 tools/import_concepts.py` (needs Pillow).
The atlas manifest stores explicit pixel rectangles and a foot `pivot` relative
to each rectangle. The four columns describe pose order; their crop widths vary
to keep extended fists and feet intact. Align each pivot to the same destination
point when displaying the atlas directly. The importer exports every pose at
the final 2× sprite size on a shared transparent canvas with aligned feet,
and validates crop edges and pivots. After
rebuilding, run `python3 tests/concepts.py` to check all 64 poses and exports.

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
| `src/engine/` | `engine.h` | `w_gra w_exe w_png w_photo v_video m_random m_argv` | `.GRA` archives (Doom: `w_wad`), tables inside `PRAGE.EXE`, PNG photos turned into sprite files; 640×400 framebuffer and fonts; random numbers; command line |
| `src/game/` | `game.h` | `info p_creature p_script p_check` | fighter state tables (Doom: `info`) loaded from creature files; Primal Rage's bytecode interpreter (`-script`) and the check against it |
| | | `p_mobj p_user p_map p_inter p_enemy p_effect p_tick` | play simulation: state machine, ticcmd control, hit boxes, damage, CPU AI, particles, round flow |
| | | `r_main r_stage st_stuff s_sound` | the fight scene and arenas, HUD and announcements, sound effects |
| | | `g_game sel_main m_menu d_main` | game flow and bindings, character select (Doom: `wi_stuff`), title, startup and the 60Hz loop |

As in Doom, input becomes a `ticcmd_t` each tic; the CPU opponent fills in the
same `ticcmd_t`, so humans and AI share every rule. CPUs use Primal Rage's
original per-fighter matchup, distance and difficulty tables, playing its
joystick macros with separate pressed and held input bits. Original fighters
inherit their own style; photographed fighters default to Sauron, overridable
with `ai 0..6` in their creature file. See [the AI decoding and verification](docs/primal-rage-ai.md).

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

For planning your own photos, see the
[Primal Rage animation frame counts and capture targets](docs/primal-rage-animation-frames.md):
per-animation counts for all seven PC fighters, shared-image totals, and a
suggested shot list for a custom creature.

The game's bytecode interpreter is kept as a reference: `-script` plays from
`PRAGE.EXE` directly (`src/engine/w_exe.c`, `src/game/p_script.c`). `make check` compares the two
tic by tic, and all 266 animations and moves match. How the original works,
and why we convert instead of following it, is in
[docs/primal-rage-scripts.md](docs/primal-rage-scripts.md).
`tools/extract_scripts.py` dumps all the tables to JSON.
`tools/primal_ai.py` also exports just the fully decoded AI tables. These
remain derived game data, read from your local `PRAGE.EXE`, and are not committed.

`make checkai` tests runtime AI integration across all 49 original matchups
and three menu difficulties. `make checkai-original AI_PYTHON=/path/to/venv/bin/python`
compares the production C implementation with the original x86 routines in
Unicorn; install the small `tests/requirements-ai.txt` into that venv first.
It checks decisions and macro inputs, while Clayfight's physics and unavailable
special-move handlers remain separate fidelity limits.

`make check` also tests legacy sprite enlargement, native photo detail,
clipping, hotspots, mirroring, scaling and hit flashes. `make checkvideo` runs
these renderer checks independently of the original game data.

## What comes from the original data

* `.GRA` format, fully decoded: chunk chain (2 = RLE pixels, 5 = palettes,
  6 = frame table w/h/hotspot/offset), per-row RLE (literal / run / skip),
  1-based sub-palette indices, unsigned 8-bit 11kHz PCM in the `SD` files.
* All sprites, palettes (including each fighter's alternate colours), arenas,
  select-screen portraits and name plates, title art, both fonts, sounds.
* From `PRAGE.EXE`, via the converter: animation scripts, move table, attack
  boxes, fighter palettes.
* Original CPU matchup/distance bands, ten-slot macro matrices, difficulty
  weights and joystick macro timing, read from `PRAGE.EXE` at runtime.
* Still rebuilt by Clayfight: hurt boxes (from the sprite silhouettes), stage
  layouts, palettes for unmapped artwork (chosen automatically as the smoothest
  result), sound triggers, physics and the adapter from game state to AI observations.
  The AI uses deterministic host randomness in place of the DOS hardware timer.
  The select temple and large portraits use
  explicit palette bindings; original fighter palettes come from PRAGE.EXE.
