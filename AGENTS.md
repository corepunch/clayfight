# Clayfight coding instructions

- Keep the native C game and its SDL/UIKit hosts independent of Nile Archive,
  Lua, and sibling checkouts. Use arcade ROMs only; do not restore DOS loaders, conversion tools or x86 AI
  paths. See docs/MIGRATION.md.
- Build with `make CFLAGS='-O2 -g -Werror'`. Use `make test`, then focused
  `build/test-combat --retail <filters>` when ROM behavior is affected. Run
  `make lint`. `make test-full` is the explicit bulk audit/capture target.
- Run selected combat cases in one test process. A failed test ends that case,
  reports file/line, and does not stop later cases. Use the existing TEST/CHECK
  runner and capture options; avoid per-screen game startup loops.
- Keep ROMs, source photos, recordings, drafts and reference masters ignored
  by Git. Preserve irreplaceable masters in persistent storage. Catalogue every
  visual/state addition and link an issue for any remaining artwork gap.
- Generate each icon set and all its normal/selected/pressed/hover/disabled
  states together in one ImageGen atlas. Preserve authored alpha and states;
  never manufacture them by tinting or procedural glyph overlays.
- Include reviewed in-game screenshots in PRs that affect visible output.
  Keep captures representative and cite measured build, test/capture and lint
  durations separately. Fixed-framebuffer relocation changes can compare the
  same decoded pixels before/after.
- Estimate additional disk use before downloads/jobs. Jobs expected to add
  over 500 MB and able to run in the cloud belong in the cloud. Keep caches and
  intermediates there; bring back final deliverables. If cloud is unavailable
  or local hardware is required, explain that before exceeding the limit.
