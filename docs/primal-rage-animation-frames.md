# Primal Rage animation frame counts and clay capture targets

For a first fighter, the current capture workshop uses **15 body photos plus
one portrait**. For a fuller animation set, the suggested shot list below uses
**132 body photos before reuse**. To approach the amount of art in Clayfight's
converted Primal Rage core set, budget roughly **170–200 unique body images**.
These are different levels of ambition; a playable creature does not need the
original game's entire animation library.

## What these counts cover

The measured counts come from the project's installed **Primal Rage PC 1.0**
data, converted from `PRAGE.EXE` and the `S04*.GRA` archives by
[`tools/pr2creature.py`](../tools/pr2creature.py). They cover all seven fighters,
with **16 standard animations and 24 attack animations per fighter** in
`creatures/primalrage/*.txt`.

This is the **converted core set**, not a complete census of every animation
in Primal Rage. Specials, projectiles, throws handled outside these rows,
fatalities, other code-referenced sequences, portraits and character-select art
are outside the counts. Counts are specific to this installed PC data; they
should not be presented as arcade or console-port counts.

Animation names are Clayfight's mappings of the original script rows. Their
interpretation, especially the `power` group, is partly inferred. See
[the script documentation](primal-rage-scripts.md) for the conversion's limits.

## What “frame” means for making a creature

Every table entry below counts **distinct source images used by that animation**:
one unique `ARCHIVE#frame_index` reference counts once. This is the useful
measure when planning photographs.

- Reusing a photo, holding it longer, looping it or mirroring it needs no new shot.
- A playback step can repeat an image with different timing or movement.
  Playback-step counts therefore often exceed the number of photos needed.
- For example, Sauron's `win` has **34 playback steps but 13 source images**;
  his `down` has **14 steps but 6 images**. Sauron's `stand` has 10 images,
  each held for 6 engine tics in the converted definition.
- The same image can appear in several animations. Adding the rows overstates
  the total artwork needed; the totals section deduplicates across animations.
- Counts identify source records, not a judgment that every pose looks visibly
  different. The counted records also have distinct pixel-data locations in
  this installed data; no visual-similarity merging was applied.

The engine runs at 60 tics per second. An image held for 6 tics stays visible
for 0.1 seconds; it does not require six photographs. Timing, loops and shared
photos are configured in the [creature format](creature-format.md).

## Movement, defence and reactions: measured source images

| Animation | Sauron | Blizzard | Talon | Vertigo | Armadon | Diablo | Chaos |
|---|---:|---:|---:|---:|---:|---:|---:|
| Ready / breathing (`stand`) | 10 | 7 | 18 | 18 | 11 | 10 | 14 |
| Crouch / crouched breathing (`crouch`) | 8 | 7 | 8 | 8 | 6 | 8 | 7 |
| Walk forward (`walkf`) | 11 | 11 | 7 | 12 | 10 | 11 | 11 |
| Walk backward (`walkb`) | 15 | 14 | 12 | 15 | 14 | 15 | 14 |
| Jump (`jump`) | 4 | 4 | 3 | 4 | 4 | 4 | 4 |
| Land (`land`) | 3 | 4 | 4 | 4 | 3 | 3 | 4 |
| Standing block (`block`) | 3 | 4 | 6 | 4 | 4 | 3 | 4 |
| Crouching block (`crouchblock`) | 3 | 4 | 5 | 4 | 4 | 3 | 4 |
| Standing hit reaction (`hit`) | 5 | 5 | 6 | 7 | 5 | 5 | 5 |
| Crouching hit reaction (`crouchhit`) | 3 | 5 | 5 | 5 | 5 | 3 | 5 |
| Heavy hit reaction (`hithard`) | 5 | 6 | 6 | 7 | 6 | 5 | 6 |
| Fall / knockdown (`fall`) | 8 | 8 | 6 | 7 | 7 | 8 | 8 |
| On the ground (`down`) | 6 | 6 | 13 | 5 | 5 | 6 | 6 |
| Get up (`getup`) | 3 | 4 | 4 | 4 | 3 | 3 | 4 |
| Dizzy (`dizzy`) | 3 | 5 | 4 | 5 | 5 | 3 | 5 |
| Victory (`win`) | 13 | 10 | 6 | 17 | 15 | 31 | 10 |

`land` and `getup` currently map to the same original script row (13), so their
images are shared. Some converted sequences include transitions as well as the
main motion; for example, `walkb` includes images also used by `jump`. Treat a
row as the complete converted sequence, not necessarily a clean looping cycle.

`down` includes animated movement while grounded; a custom fighter can instead
hold a single lying-down photo. Victory sequences vary especially widely:
Diablo uses 31 source images while Talon uses 6.

## Attacks: measured source images

Buttons are **HQ** = high quick, **HF** = high fierce, **LQ** = low quick and
**LF** = low fierce. These are button labels, not universal punch/kick names or
guaranteed high/low hit properties. Choose suitable attacks for your creature.

- `stand`: standing at normal range.
- `close`: standing near the opponent.
- `crouch`: attacking while crouched.
- `jump`: attacking during a straight jump.
- `air`: the project's mapping for attacks during a moving jump.
- `power`: another original move group, whose trigger is not established;
  Clayfight does not currently trigger it. It is included for reference only.

| Animation | Sauron | Blizzard | Talon | Vertigo | Armadon | Diablo | Chaos |
|---|---:|---:|---:|---:|---:|---:|---:|
| Standing HQ (`stand_hq`) | 4 | 3 | 4 | 4 | 3 | 4 | 3 |
| Standing HF (`stand_hf`) | 7 | 5 | 5 | 5 | 6 | 7 | 5 |
| Standing LQ (`stand_lq`) | 5 | 3 | 6 | 5 | 6 | 5 | 3 |
| Standing LF (`stand_lf`) | 7 | 7 | 7 | 5 | 6 | 7 | 6 |
| Close HQ (`close_hq`) | 4 | 4 | 4 | 4 | 3 | 4 | 4 |
| Close HF (`close_hf`) | 5 | 4 | 4 | 5 | 6 | 9 | 4 |
| Close LQ (`close_lq`) | 5 | 3 | 6 | 5 | 4 | 5 | 3 |
| Close LF (`close_lf`) | 7 | 7 | 7 | 5 | 4 | 7 | 6 |
| Crouching HQ (`crouch_hq`) | 3 | 4 | 3 | 3 | 3 | 3 | 4 |
| Crouching HF (`crouch_hf`) | 6 | 6 | 7 | 6 | 7 | 6 | 6 |
| Crouching LQ (`crouch_lq`) | 5 | 6 | 6 | 5 | 7 | 5 | 6 |
| Crouching LF (`crouch_lf`) | 6 | 5 | 5 | 5 | 8 | 7 | 5 |
| Straight jump HQ (`jump_hq`) | 3 | 2 | 5 | 3 | 5 | 3 | 2 |
| Straight jump HF (`jump_hf`) | 6 | 7 | 8 | 4 | 5 | 6 | 7 |
| Straight jump LQ (`jump_lq`) | 3 | 5 | 7 | 2 | 4 | 3 | 5 |
| Straight jump LF (`jump_lf`) | 5 | 6 | 7 | 5 | 8 | 6 | 6 |
| Moving jump HQ (`air_hq`) | 8 | 7 | 7 | 6 | 7 | 8 | 7 |
| Moving jump HF (`air_hf`) | 7 | 8 | 5 | 8 | 8 | 7 | 8 |
| Moving jump LQ (`air_lq`) | 6 | 7 | 8 | 4 | 5 | 6 | 7 |
| Moving jump LF (`air_lf`) | 7 | 6 | 7 | 4 | 6 | 6 | 6 |
| Power HQ (`power_hq`) | 10 | 7 | 7 | 5 | 9 | 9 | 7 |
| Power HF (`power_hf`) | 8 | 7 | 7 | 8 | 8 | 8 | 7 |
| Power LQ (`power_lq`) | 5 | 3 | 7 | 5 | 6 | 9 | 3 |
| Power LF (`power_lf`) | 7 | 7 | 7 | 5 | 5 | 7 | 7 |

Different move slots can share images even when they have different damage or
timing. For example, Sauron's standing and close HQ attacks use the same four
source images. His `jump_hf` and `air_lq` also share their six images.

## Total artwork in this converted set

These counts deduplicate image references **within each column**, including
reuse between animations. The body and attack columns overlap, so their sum
can exceed the final column. All 24 attacks, including `power`, are included.

| Character | Movement, defence and reactions (16 animations) | Attacks (24 animations) | Unique images across all 40 animations |
|---|---:|---:|---:|
| Sauron | 93 | 94 | **186** |
| Blizzard | 94 | 100 | **190** |
| Talon | 98 | 83 | **181** |
| Vertigo | 109 | 57 | **166** |
| Armadon | 94 | 80 | **172** |
| Diablo | 102 | 96 | **197** |
| Chaos | 94 | 93 | **183** |

These totals are a reference for production effort, not minimum requirements
and not the total number of images stored for each character in the game.

## Suggested capture plan for a custom fighter

The following numbers are **authoring recommendations**, not measured
Primal Rage counts. They favour clear poses and manageable shooting sessions.
The current workshop captures a small starter set; the expanded set requires
additional photos and animations authored in a creature file.

### Starter: the existing workshop

Capture 2 ready/breathing poses, 4 walking poses, 2 punch poses (windup and
contact), then 1 each for kick, crouch, jump, block, hit, down and victory.
That is **15 body photos**, plus **1 separate portrait**. This is enough to
start with reused poses and fallback animations; it is not a full set of
independently photographed moves.

### Fuller set: a practical shot list

| Animation or group | New poses to plan | Capture notes |
|---|---:|---|
| Ready / breathing | 8 | Small changes in chest, head and tail. |
| Crouch | 4 | Lower into the crouch and add a little breathing motion. |
| Walk forward | 8 | Include both feet passing and both contact positions. |
| Walk backward | 8 | A separate cycle if the posture differs; otherwise reuse walking images. |
| Jump | 4 | Compress, extend, tuck, prepare to land; the engine supplies height. |
| Land / get up | 3 shared | Share the sequence initially, as the converter does. |
| Standing block | 3 | Enter guard, brace, settle. |
| Crouching block | 3 | The same idea at crouched height. |
| Standing hit | 4 | Impact, recoil and recovery. |
| Crouching hit | 3 | A separate low reaction. |
| Heavy hit | 5 | A stronger recoil with a clear silhouette. |
| Fall | 6 | Show loss of balance and ground contact. |
| Down | 1 | Hold the lying pose; add breathing later if desired. |
| Dizzy | 4 | A short sway loop. |
| Victory | 10 | One readable celebration that can loop or hold. |
| Standing attacks: HQ / HF / LQ / LF | 4 / 6 / 4 / 6 = 20 | Four separate attacks. |
| Crouching attacks: HQ / HF / LQ / LF | 4 / 6 / 4 / 6 = 20 | Four separate low-posture attacks. |
| Jump attacks: HQ / HF / LQ / LF | 3 / 5 / 4 / 6 = 18 | Reuse these for moving jumps initially. |
| **Total body photos before further reuse** | **132** | Plus a separate portrait and an empty-backdrop shot. |

The 132-photo budget already shares `land`/`getup`; it does not assume any
other shared images. Reusing the forward walk for backward walking reduces
it to 124. Reusing ready or recovery poses between attacks can reduce it further.

Start close-range attacks by pointing their move entries at the standing
animations, and moving-jump attacks at the jump animations. The engine also
provides these fallbacks when those move slots are omitted. Add distinct
close/air variants when they improve the creature's personality or reach.
There is no need to photograph the untriggered `power` group for normal play.

For each attack, cover **anticipation → contact → follow-through → recovery**.
A quick attack can use fewer images; a heavy attack benefits from more windup
and recoil. Spend extra shots on readable contact poses and smooth movement
before making long victory sequences. Tune image durations to get the intended
speed: more photos alone do not determine how fast an attack plays.

Photograph the creature facing one direction; the engine mirrors it for the
other player. Keep camera scale, lighting and the foot baseline consistent.
See [shooting tips](creature-format.md#shooting-tips) and the
[native capture workflow](menu-and-capture.md#photographing-a-fighter).

## Reproducing the measured counts

The source of truth is the local game data, not an online sprite-sheet estimate.
From the repository root, regenerate the creature files with:

```sh
python3 tools/pr2creature.py primalrage creatures/primalrage
```

Then run this standard-library-only counter. It prints the same per-animation
counts and the final deduplicated totals. Original data and generated creature
files remain local; this guide contains counts, not sprites or frame sequences.

```sh
python3 - <<'PYCOUNT'
from pathlib import Path

characters = ['sauron', 'blizzard', 'talon', 'vertigo',
              'armadon', 'diablo', 'chaos']
data = {}
for character in characters:
    animations = {}
    current = None
    path = Path('creatures/primalrage') / (character + '.txt')
    for line in path.read_text().splitlines():
        fields = line.split()
        if fields[:1] == ['anim']:
            current = fields[1]
            animations[current] = set()
        elif fields[:1] == ['end']:
            current = None
        elif current and fields and not fields[0].startswith('#'):
            animations[current].add(fields[0])
    data[character] = animations

print('animation | ' + ' | '.join(characters))
for name in data[characters[0]]:
    print(name + ' | ' + ' | '.join(
        str(len(data[c][name])) for c in characters))
for character in characters:
    unique = set().union(*data[character].values())
    print(character, 'total unique images:', len(unique))
PYCOUNT
```

For extraction details, see [`tools/extract_scripts.py`](../tools/extract_scripts.py),
[the original script analysis](primal-rage-scripts.md), and
[the creature-file format](creature-format.md).
