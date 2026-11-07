"""Measures how much slideio's JPEG decoder differs from OpenSlide's, per reference region.

slideio decodes JPEG with libjpeg 9f; OpenSlide (and Pillow) use libjpeg-turbo. They
reconstruct subsampled chroma differently, so on chroma-subsampled JPEG slides the two
renderers see slightly different tiles. The MRXS corpus render test adds this measured,
decoder-only difference to its tolerance (design section 9) instead of a guessed margin.

Two halves, run by hand after auxfiles/mrxs-openslide-reference.py:

  1. MRXS_TILE_DUMP_DIR=<dir> slideio_mrxs_tests --gtest_also_run_disabled_tests
         --gtest_filter=MRXSCorpusRender.DISABLED_dumpDecodedTilesOfEveryRegion
     writes slideio's decode of every JPEG tile each region uses, plus regions.json.
  2. python -I mrxs-decoder-difference.py <images-root> <dir>
     decodes the same payloads with Pillow and writes, for each region, the largest
     per-channel difference over its tiles to
     <images-root>/mirax/reference/decoder-difference.json.
"""
import io
import json
import os
import sys

import numpy as np
from PIL import Image, features


def read_ini_files(bundle_dir):
    """The ordered data-file names from Slidedat.ini (enough of it for this tool)."""
    names = {}
    with open(os.path.join(bundle_dir, "Slidedat.ini"), "rb") as f:
        text = f.read().decode("utf-8-sig")
    group = None
    for line in text.splitlines():
        s = line.strip()
        if s.startswith("[") and s.endswith("]"):
            group = s[1:-1]
        elif group == "DATAFILE" and "=" in s and not s.startswith("="):
            key, _, value = s.partition("=")
            if key.strip().startswith("FILE_") and key.strip() != "FILE_COUNT":
                names[int(key.strip()[5:])] = value.strip()
    return names


def main():
    images_root, dump_dir = sys.argv[1], sys.argv[2]
    regions = json.load(open(os.path.join(dump_dir, "regions.json")))
    files_of = {}
    result = {"pillow_libjpeg_turbo": features.version_feature("libjpeg_turbo"), "regions": {}}
    worst = 0
    for region, info in sorted(regions.items()):
        bundle_dir = os.path.splitext(os.path.join(images_root, "mirax", info["bundle"]))[0]
        if bundle_dir not in files_of:
            files_of[bundle_dir] = read_ini_files(bundle_dir)
        largest = 0
        for tile in info["tiles"]:
            path = os.path.join(bundle_dir, files_of[bundle_dir][tile["file"]])
            with open(path, "rb") as f:
                f.seek(tile["offset"])
                payload = f.read(tile["length"])
            turbo = np.asarray(Image.open(io.BytesIO(payload)).convert("RGB"), np.int16)
            ours = np.fromfile(os.path.join(dump_dir, tile["dump"]), np.uint8).reshape(
                tile["height"], tile["width"], 3).astype(np.int16)
            largest = max(largest, int(np.abs(turbo - ours).max()))
        result["regions"][region] = largest
        worst = max(worst, largest)
        print("%-40s %3d tiles  decoder difference %d" % (region, len(info["tiles"]), largest))
    out = os.path.join(images_root, "mirax", "reference", "decoder-difference.json")
    with open(out, "w", newline="\n") as f:
        json.dump(result, f, indent=1, sort_keys=True)
        f.write("\n")
    print("worst %d; wrote %s" % (worst, out))


if __name__ == "__main__":
    main()
