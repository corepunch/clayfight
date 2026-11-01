# Return to the standalone repository

The arcade runtime and artwork were extracted from `corepunch/nile-archive`
commit `5de9dcf1478df4107a6108164efbb88f0f2e5041`. The destination is the existing
`corepunch/clayfight` repository, not a new unrelated history. The migration
branch descends from standalone commit `dae2799`. Existing Git history is
retained; the active project is arcade-only. DOS loaders, converters, x86 AI
and oracles, old comparison tools and unused DOS reference data are removed.

The active game is the current arcade implementation. Its C source, state
mapping, physics, AI, ROM data interpretation, CAGE decoder and photo pipeline
carry over. SDL restores the standalone desktop window/input/audio host; UIKit
restores the standalone native capture/game host. The Nile Lua scheduler, app
extension, GPU upload bridge, platform submodule and multi-game engine are not
dependencies of this repository.

`src/support/raster.c` and `photo.c` are the exact CPU algorithms extracted from
Nile's engine, with a small local header. `image.c` carries its stb loader. The
same photograph palettes, alpha, density, sprite pivots and 32-bit framebuffer
are retained. Desktop audio remains a 16-voice 44.1 kHz stereo SDL mixer. Native
iOS uses AVAudioEngine. The original standalone native touch layout is restored;
the removed Nile Lua desktop touch overlay is not required for SDL keyboard or
controller play. Native capture still supports photos, video, media import,
source retention, drafts and published fighters.

## Local sources and reproducibility

- `data/primrage/`: local arcade chips, ignored by Git.
- `reference/original-history.bundle`: the original historical Git safety
  bundle, preserved from Nile's ignored reference folder. Current repository
  history is retained independently; this archive is not a runtime input.
- `art/reference/`: existing local graphics extraction/review material.
- `build/migrated-review/`: existing reproducible test/capture outputs moved
  from Nile's game-local build directory; not source masters.

Recorded photos and masters stay in their existing `sources/` folders or native
Application Support storage. Nothing in the extraction deletes device storage
or user-created masters. No ROMs or private source recordings are published.

To reproduce the standalone result, install SDL2, supply the supported chips,
then run `make`, `make test`, `make test-retail` and `make lint`. Complete arcade
art/audio captures are deliberate `make test-full` work. Native iOS builds run
in cloud CI to avoid creating a large local DerivedData tree; compilation in CI
can omit private ROMs, while actual gameplay needs the user's local data.

The before/after screenshots in `docs/review/extraction/` show tick 240 of the
same Sauron/Talon CPU setup. All three sampled frames (ticks 0, 120, 240) were
compared as decoded RGBA/RGB pixels and were identical after moving to the SDL
host. The focused combat suite and real publication checks also pass.

The earlier combat documentation's measurements are retained as historical
records for Nile's integration. Current build and test commands are in the
root README; paths referencing that old integration are provenance, not runtime
dependencies. The photo dizzy artwork follow-up remains
[corepunch/nile-archive#60](https://github.com/corepunch/nile-archive/issues/60).
