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
SPRITE_HEIGHT = 184  # 92 DOS world units, two PNG pixels per unit.


def figure_mask(image):
    """Keep the connected figure, excluding faint and detached atlas residue."""
    w, h = image.size
    remaining = bytearray(v > 127 for v in image.getchannel("A").tobytes())
    largest = []
    for seed in range(w * h):
        if not remaining[seed]:
            continue
        remaining[seed] = 0
        pending, component = [seed], []
        while pending:
            p = pending.pop()
            component.append(p)
            x, y = p % w, p // w
            neighbors = ([p - 1] if x else []) + ([p + 1] if x + 1 < w else [])
            neighbors += ([p - w] if y else []) + ([p + w] if y + 1 < h else [])
            for n in neighbors:
                if remaining[n]:
                    remaining[n] = 0
                    pending.append(n)
        if len(component) > len(largest):
            largest = component
    if not largest:
        raise ValueError("frame has no opaque figure")
    mask = bytearray(w * h)
    for p in largest:
        mask[p] = 255
    return Image.frombytes("L", image.size, bytes(mask))


def figure_geometry(image):
    """Return the main figure's exclusive bounds and foot pivot in pixels."""
    w, h = image.size
    mask = figure_mask(image)
    largest = [i for i, value in enumerate(mask.tobytes()) if value]
    xs, ys = [p % w for p in largest], [p // w for p in largest]
    box = min(xs), min(ys), max(xs) + 1, max(ys) + 1
    bottom = box[3] - 1
    feet = [p % w for p in largest if p // w >= bottom - max(2, (box[3] - box[1]) // 16)]
    return box, {"x": sum(feet) // len(feet), "y": bottom}


def load_frames(source, atlas):
    """Validate explicit rectangles and pivots before slicing any photo files."""
    if source.size != (atlas["width"], atlas["height"]):
        raise ValueError(f"{atlas['key']}: image dimensions do not match the manifest")
    frames = []
    for index, frame in enumerate(atlas["frames"]):
        x, y, w, h = (frame[k] for k in ("x", "y", "w", "h"))
        if frame["index"] != index or min(x, y) < 0 or min(w, h) <= 0 or x + w > source.width or y + h > source.height:
            raise ValueError(f"{atlas['key']} frame {index}: invalid rectangle or index")
        crop = source.crop((x, y, x + w, y + h)).convert("RGBA")
        box, pivot = figure_geometry(crop)
        if box[0] == 0 or box[1] == 0 or box[2] == w or box[3] == h:
            raise ValueError(f"{atlas['key']} frame {index}: figure touches a crop edge; check for a clipped limb")
        if frame["pivot"] != pivot:
            raise ValueError(f"{atlas['key']} frame {index}: foot pivot does not match the figure")
        frames.append(crop)
    return frames


def photo_layout(frames, crops):
    """Tightly fit the figures around one shared foot pivot before scaling."""
    boxes = [figure_geometry(c)[0] for c in crops]
    px = 8 + max(f["pivot"]["x"] - b[0] for f, b in zip(frames, boxes))
    py = 8 + max(f["pivot"]["y"] - b[1] for f, b in zip(frames, boxes))
    size = (px + max(b[2] - f["pivot"]["x"] for f, b in zip(frames, boxes)) + 8,
            py + max(b[3] - f["pivot"]["y"] for f, b in zip(frames, boxes)) + 8)
    offsets = [(px - f["pivot"]["x"], py - f["pivot"]["y"]) for f in frames]
    return size, offsets


def export_frames(atlas, crops):
    canvas, offsets = photo_layout(atlas["frames"], crops)
    box, _ = figure_geometry(crops[0])
    scale = SPRITE_HEIGHT / (box[3] - box[1])
    size = tuple(round(n * scale) for n in canvas)
    result = []
    for crop, offset in zip(crops, offsets):
        crop = crop.copy()
        crop.putalpha(figure_mask(crop))
        sprite = Image.new("RGBA", canvas)
        sprite.alpha_composite(crop, offset)
        sprite = sprite.resize(size, Image.Resampling.LANCZOS)
        # The indexed game sprites have hard transparency. Never composite
        # onto paper: that bakes white into their antialiased boundary colours.
        sprite.putalpha(sprite.getchannel("A").point(lambda a: 255 if a >= 128 else 0))
        clean = Image.new("RGBA", size)
        clean.paste(sprite, mask=sprite.getchannel("A"))
        result.append(clean)
    return result


def main():
    manifest = json.loads((SOURCE / "animations.json").read_text())
    for atlas in manifest["atlases"]:
        with Image.open(SOURCE / atlas["image"]) as source:
            crops = load_frames(source, atlas)
        sprites = export_frames(atlas, crops)
        dest = ROOT / "creatures" / atlas["key"]
        photos = dest / "photos"
        photos.mkdir(parents=True, exist_ok=True)
        for frame, sprite in zip(atlas["frames"], sprites):
            sprite.save(photos / f"{frame['index']:02d}.png")
        (photos / "backdrop.png").unlink(missing_ok=True)

        lines = ["# Generated concept poses; rebuild with tools/import_concepts.py.",
                 f"name {NAMES[atlas['key']]}", "art photo right", "height 92",
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
