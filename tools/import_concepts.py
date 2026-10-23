#!/usr/bin/env python3
"""Slice the saved clay action sheets into playable photo-creature prototypes.

Requires Pillow. Run from any directory; sources and outputs live in the repo.
These are generated pose studies, so only idle, punch, kick, block and hit are
authored. Other reactions use the existing creature loader's fallbacks.
"""

import json
from pathlib import Path

from PIL import Image


ROOT = Path(__file__).resolve().parents[1]
SOURCE = ROOT / "assets/concepts/clay-kids-actions-2026-10-23"
NAMES = {"red-boxer": "BOXER", "green-lizard": "LIZARD",
         "purple-cyclops": "CYCLOPS", "yellow-crab": "CRAB"}


def main():
    manifest = json.loads((SOURCE / "animations.json").read_text())
    for atlas in manifest["atlases"]:
        dest = ROOT / "creatures" / atlas["key"]
        photos = dest / "photos"
        photos.mkdir(parents=True, exist_ok=True)
        canvas = (max(f["w"] for f in atlas["frames"]) + 16,
                  max(f["h"] for f in atlas["frames"]) + 16)
        Image.new("RGB", canvas, "white").save(photos / "backdrop.png")
        with Image.open(SOURCE / atlas["image"]) as source:
            for frame in atlas["frames"]:
                x, y, w, h = (frame[k] for k in ("x", "y", "w", "h"))
                crop = source.crop((x, y, x + w, y + h)).convert("RGBA")
                # The photo pipeline segments opaque paper backgrounds.
                paper = Image.new("RGBA", canvas, "white")
                paper.alpha_composite(crop, (8, 8))
                paper.convert("RGB").save(photos / f"{frame['index']:02d}.png")

        lines = ["# Generated concept poses; rebuild with tools/import_concepts.py.",
                 f"name {NAMES[atlas['key']]}", "art photo right", "height 92",
                 "backdrop photos/backdrop.png",
                 "speed 1.4 1.1 7.5", "width 25", ""]

        def anim(name, frames, tics, mode=""):
            lines.append(f"anim {name} {mode}".rstrip())
            lines.extend(f"  photos/{frame:02d}.png {tic}"
                         for frame, tic in zip(frames, tics))
            lines.extend(["end", ""])

        anim("stand", [0], [12], "hold")
        anim("block", [9, 10], [6, 12], "hold")
        anim("hit", [13, 14, 15], [6, 10, 8])
        anim("win", [0], [30], "hold")
        for name in ("punch", "kick"):
            spec = atlas["animations"][name]
            anim(name, spec["frames"], [max(1, round(ms * 60 / 1000))
                                       for ms in spec["durations_ms"]])
        for button, action, damage, level in (("hq", "punch", 6, "high"),
                                               ("hf", "punch", 11, "high"),
                                               ("lq", "kick", 7, "low"),
                                               ("lf", "kick", 11, "low")):
            lines.extend([f"move stand.{button} {action} damage {damage} "
                          f"stun 18 strength 2 level {level}", "end", ""])
        (dest / f"{atlas['key']}.txt").write_text("\n".join(lines))
        print(f"{NAMES[atlas['key']]}: {len(atlas['frames'])} poses -> {dest}")


if __name__ == "__main__":
    main()
