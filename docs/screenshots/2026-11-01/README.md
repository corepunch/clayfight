# README screenshots — 2026-11-01

Six reviewed, unmodified 672×480 framebuffer captures from the native C runtime.
PNG encoding is lossless; decoded RGB pixels match the original BMP captures.
The fights use CPU players at difficulty 2. Fighter select shows Sauron and
Talon on the original temple podiums. Existing ROM and photo artwork is reused.

| File | View | Simulation tick |
| --- | --- | --- |
| `fighter-select.png` | Arcade fighter select | 120 select ticks |
| `sauron-talon-cove.png` | Sauron vs. Talon, The Cove | 201 |
| `blizzard-chaos-inferno.png` | Blizzard vs. Chaos, The Inferno | 261 |
| `vertigo-armadon-tomb.png` | Vertigo vs. Armadon, The Tomb | 141 |
| `diablo-sauron-ruins.png` | Diablo vs. Sauron, The Ruins | 261 |
| `boxer-lizard-hollows.png` | Custom Boxer vs. Lizard, The Hollows | 141 |

The source is the local working tree including the shared animation fixes;
its baseline commit and image SHA-256 hashes are recorded in `captures.json`.
No ROM chips, source photographs or recording masters are included.

Reproduce all candidates in one process:

```sh
make CFLAGS='-O2 -g -Werror' build/test-combat
mkdir -p build/readme-captures
build/test-combat --retail readme_screenshots --captures build/readme-captures
```

Select the BMP at tick index one less than the fight tick above, then encode
as RGB PNG without resizing. The select capture is `fighter-select.bmp`.
Measured build: 1.09 s; capture run: 0.90 s, including resource initialization.
