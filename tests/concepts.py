"""Asset regression checks; run after tools/import_concepts.py (requires Pillow)."""

import copy
import importlib.util
import json
import unittest
from pathlib import Path

from PIL import Image


ROOT = Path(__file__).resolve().parents[1]
SPEC = importlib.util.spec_from_file_location("import_concepts", ROOT / "tools/import_concepts.py")
CONCEPTS = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(CONCEPTS)


class ConceptAssets(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.atlases = json.loads((CONCEPTS.SOURCE / "animations.json").read_text())["atlases"]

    def test_all_64_poses_fit_their_rectangles(self):
        for atlas in self.atlases:
            with self.subTest(fighter=atlas["key"]), Image.open(CONCEPTS.SOURCE / atlas["image"]) as source:
                crops = CONCEPTS.load_frames(source, atlas)
                self.assertEqual(len(crops), 16)

    def test_legacy_grid_is_rejected_for_clipping_the_punch(self):
        for atlas in self.atlases:
            old = copy.deepcopy(atlas)
            old["frames"][2]["w"] = 313  # The old right edge was x=940.
            with self.subTest(fighter=atlas["key"]), Image.open(CONCEPTS.SOURCE / atlas["image"]) as source:
                with self.assertRaisesRegex(ValueError, "clipped limb"):
                    CONCEPTS.load_frames(source, old)

    def test_wrong_image_dimensions_are_rejected(self):
        atlas = self.atlases[0]
        with Image.new("RGBA", (10, 10)) as source:
            with self.assertRaisesRegex(ValueError, "dimensions"):
                CONCEPTS.load_frames(source, atlas)

    def test_exported_photos_share_a_foot_baseline_and_keep_margins(self):
        for atlas in self.atlases:
            size, baseline = None, None
            for frame in atlas["frames"]:
                with self.subTest(fighter=atlas["key"], frame=frame["index"]):
                    path = ROOT / "data/creatures" / atlas["key"] / "photos" / f"{frame['index']:02d}.png"
                    with Image.open(path) as photo:
                        self.assertEqual(photo.mode, "RGBA")
                        if size is None: size = photo.size
                        self.assertEqual(photo.size, size)
                        self.assertLess(max(size), 320)
                        self.assertEqual(set(photo.getchannel("A").tobytes()), {0, 255})
                        box = photo.getchannel("A").getbbox()
                        if baseline is None:
                            baseline = box[3] - 1
                            self.assertLessEqual(abs(box[3] - box[1] - 184), 1)
                        self.assertGreaterEqual(min(box[0], box[1]), 2)
                        self.assertLessEqual(box[2], size[0] - 2)
                        self.assertLessEqual(box[3], size[1] - 2)
                        self.assertLessEqual(abs(box[3] - 1 - baseline), 2)
                        # Fully transparent pixels contain no white paper RGB.
                        pixels = photo.tobytes()
                        for i in range(0, len(pixels), 4):
                            if not pixels[i + 3]: self.assertEqual(pixels[i:i + 3], b'\0\0\0')


if __name__ == "__main__":
    unittest.main()
