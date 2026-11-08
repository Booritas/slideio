"""OpenSlide 4.0.1 reference harness for the slideio MRXS driver.

Run by hand, never by CI. Needs openslide-python with openslide-bin 4.0.1.x and
Pillow (see auxfiles/mrxs-fixtures/README.md for the environment).

Two commands:

  check-fixtures [data-dir]
      Opens every synthetic fixture written by auxfiles/mrxs-fixtures/generate.py
      with OpenSlide and compares level sizes, downsamples, bounds, background and
      every probe pixel with the fixture's .expected.json. This checks the
      generator itself against an independent reader. It also reports how OpenSlide
      resolves a repeated INI key and a repeated INI group (fixture ini-duplicates),
      which is the GKeyFile behaviour the driver's INI reader must follow.

  references <images-root> <out-dir>
      For each integration bundle under <images-root>/mirax/ (the nine listed in
      design section 9), writes regions read by OpenSlide at every stored level,
      flattened onto the slide's background colour, as lossless PNGs, plus
      manifest.json recording the regions, OpenSlide's level geometry and bounds,
      and the library versions.
"""
import json
import math
import os
import sys

import importlib.util

import openslide
from PIL import Image

# The renderer prototype's bundle reader and geometry, used only to place reference regions
# on content; the references themselves come from OpenSlide.
_spec = importlib.util.spec_from_file_location(
    "mrxs_prototype", os.path.join(os.path.dirname(os.path.abspath(__file__)), "mrxs-renderer-prototype.py"))
proto = importlib.util.module_from_spec(_spec)
_spec.loader.exec_module(proto)

INTEGRATION_BUNDLES = [
    "openslide/Mirax2-Fluorescence-1/Mirax2-Fluorescence-1.mrxs",
    "openslide/Mirax2-Fluorescence-2/Mirax2-Fluorescence-2.mrxs",
    "openslide/CMU-1-Saved-1_2/CMU-1-Saved-1_2.mrxs",
    "openslide/CMU-1-Saved-1_16/CMU-1-Saved-1_16.mrxs",
    "openslide/Mirax2.2-4-PNG/Mirax2.2-4-PNG.mrxs",
    "openslide/Mirax2.2-4-BMP/Mirax2.2-4-BMP.mrxs",
    "zenodo/20240920152150628/20240920152150628.mrxs",
    "zenodo/PDX_H5097_ID_2053_LG_HE/PDX_H5097_ID_2053_LG_HE.mrxs",
    "zenodo/2401582-38_mihc/2401582-38.mrxs",
]


def background_rgb(slide):
    value = slide.properties.get(openslide.PROPERTY_NAME_BACKGROUND_COLOR, "FFFFFF")
    return tuple(int(value[i:i + 2], 16) for i in (0, 2, 4))


def read_flat(slide, level, x, y, w, h, background):
    """Reads a region given in the level's own pixel coordinates, flattened onto background."""
    ds = slide.level_downsamples[level]
    rgba = slide.read_region((int(round(x * ds)), int(round(y * ds))), level, (w, h))
    flat = Image.new("RGB", rgba.size, background)
    flat.paste(rgba, mask=rgba.getchannel("A"))
    return flat


def bounds(slide):
    p = slide.properties
    keys = [openslide.PROPERTY_NAME_BOUNDS_X, openslide.PROPERTY_NAME_BOUNDS_Y,
            openslide.PROPERTY_NAME_BOUNDS_WIDTH, openslide.PROPERTY_NAME_BOUNDS_HEIGHT]
    if not all(k in p for k in keys):
        return None
    return [int(p[k]) for k in keys]


def check_fixtures(data_dir):
    failures = 0
    names = json.load(open(os.path.join(data_dir, "manifest.json")))["fixtures"]
    for name in names:
        expected = json.load(open(os.path.join(data_dir, name + ".expected.json")))
        path = os.path.join(data_dir, name + ".mrxs")
        problems = []
        try:
            slide = openslide.OpenSlide(path)
        except Exception as e:  # OpenSlide refused the bundle
            if "error" in expected:
                print("%-22s expected %s (%s): OpenSlide refuses it too -- %s"
                      % (name, expected["error"], expected["error_on"], str(e).splitlines()[0][:70]))
            else:
                print("%-22s FAIL: OpenSlide cannot open a valid fixture: %s" % (name, e))
                failures += 1
            continue
        if expected.get("openslide_incompatible"):
            print("%-22s skipped: %s" % (name, expected["openslide_incompatible"]))
            slide.close()
            continue
        if "error" in expected:
            # Several errors are this driver's policy, stricter than OpenSlide, or only
            # surface when the bad payload is read.
            note = "OpenSlide opens it"
            if expected["error_on"] == "read":
                try:
                    lv = 0
                    slide.read_region((0, 0), lv, slide.level_dimensions[lv])
                    note += "; and reads level 0 without error"
                except Exception as e:
                    note += "; reading level 0 fails -- %s" % str(e).splitlines()[0][:60]
            print("%-22s expected %s (%s): %s" % (name, expected["error"], expected["error_on"], note))
            slide.close()
            continue

        levels = expected["levels"]
        if slide.level_count != len(levels):
            problems.append("level count %d != %d" % (slide.level_count, len(levels)))
        for l, lv in enumerate(levels[:slide.level_count]):
            if tuple(slide.level_dimensions[l]) != (lv["width"], lv["height"]):
                problems.append("level %d size %s != %s" % (l, slide.level_dimensions[l], (lv["width"], lv["height"])))
            if abs(slide.level_downsamples[l] - lv["downsample"]) > 1e-9:
                problems.append("level %d downsample %r != %r" % (l, slide.level_downsamples[l], lv["downsample"]))
        bg = background_rgb(slide)
        if list(bg) != expected["background_rgb"]:
            problems.append("background %s != %s" % (bg, expected["background_rgb"]))
        ob = bounds(slide)
        eb = expected["bounds_level0"]
        if ob is not None and eb is not None:
            # OpenSlide publishes the union of the level-0 drawables, unclipped -- a
            # negative origin stays negative -- rounded outwards to whole pixels.
            ex0, ey0 = math.floor(eb[0]), math.floor(eb[1])
            ex1, ey1 = math.ceil(eb[0] + eb[2]), math.ceil(eb[1] + eb[3])
            if ob != [ex0, ey0, ex1 - ex0, ey1 - ey0]:
                problems.append("bounds %s vs expected float bounds %s" % (ob, eb))
        tol = expected["pixel_tolerance"]
        checked = 0
        for l, lv in enumerate(levels[:slide.level_count]):
            for probe in lv["probes"] + lv["holes"]:
                got = read_flat(slide, l, probe["x"], probe["y"], 1, 1, bg).getpixel((0, 0))
                if max(abs(a - b) for a, b in zip(got, probe["rgb"])) > tol:
                    problems.append("level %d pixel (%d,%d) = %s, expected %s"
                                    % (l, probe["x"], probe["y"], got, probe["rgb"]))
                checked += 1
        for public, info in expected["associated"].items():
            key = {"Macro": "macro", "Label": "label", "Thumbnail": "thumbnail"}[public]
            image = slide.associated_images.get(key)
            if image is None or image.size != (info["width"], info["height"]):
                problems.append("associated %s: %s" % (key, None if image is None else image.size))
        if name == "ini-duplicates":
            p = slide.properties
            print("%-22s GKeyFile: SLIDE_NAME=%r; EXTRA_GROUP KEY_A=%r KEY_B=%r"
                  % (name, p.get("mirax.GENERAL.SLIDE_NAME"), p.get("mirax.EXTRA_GROUP.KEY_A"),
                     p.get("mirax.EXTRA_GROUP.KEY_B")))
        if problems:
            failures += 1
            print("%-22s FAIL" % name)
            for problem in problems:
                print("    " + problem)
        else:
            print("%-22s ok: %d levels, bounds %s, %d pixels" % (name, slide.level_count, ob, checked))
        slide.close()
    return failures


def references(images_root, out_dir):
    os.makedirs(out_dir, exist_ok=True)
    manifest = {"openslide": openslide.__library_version__, "openslide_python": openslide.__version__,
                "bundles": {}}
    for rel in INTEGRATION_BUNDLES:
        path = os.path.join(images_root, "mirax", rel)
        if not os.path.exists(path):
            print("missing: %s" % path)
            continue
        slide = openslide.OpenSlide(path)
        bg = background_rgb(slide)
        b = bounds(slide) or [0, 0] + list(slide.dimensions)
        stem = os.path.splitext(os.path.basename(rel))[0]
        entry = {"levels": [list(d) for d in slide.level_dimensions],
                 "downsamples": list(slide.level_downsamples), "bounds": b,
                 "background_rgb": list(bg), "associated": {k: list(v.size) for k, v in slide.associated_images.items()},
                 "regions": []}
        # Regions are placed on actual drawables. Centring them on the populated bounds put a
        # fifth of them in holes of sparse tissue, where an exact match proves nothing.
        drawables = proto.drawables(proto.Bundle(path))
        for level in range(slide.level_count):
            ds = slide.level_downsamples[level]
            lw, lh = slide.level_dimensions[level]
            size = 256
            tiles = drawables[level]
            # The drawable nearest the centre of the populated bounds, at the region's centre;
            # and the top-left-most drawable's corner at the region's centre, so the region
            # straddles the edge of the content.
            bx, by = (b[0] + b[2] / 2) / ds, (b[1] + b[3] / 2) / ds
            near = min(tiles, key=lambda t: (t[0] + t[2] / 2 - bx) ** 2 + (t[1] + t[3] / 2 - by) ** 2)
            corner = min(tiles, key=lambda t: t[0] + t[1])
            cx, cy = int(near[0] + near[2] / 2) - size // 2, int(near[1] + near[3] / 2) - size // 2
            ex, ey = int(corner[0]) - size // 2, int(corner[1]) - size // 2
            for tag, x, y in (("centre", cx, cy), ("edge", ex, ey)):
                x, y = max(0, min(x, max(0, lw - 1))), max(0, min(y, max(0, lh - 1)))
                w, h = min(size, lw - x), min(size, lh - y)
                if w <= 0 or h <= 0:
                    continue
                file_name = "%s_L%d_%s.png" % (stem, level, tag)
                read_flat(slide, level, x, y, w, h, bg).save(os.path.join(out_dir, file_name), optimize=False)
                entry["regions"].append({"level": level, "x": x, "y": y, "width": w, "height": h, "file": file_name})
        manifest["bundles"][rel] = entry
        print("%s: %d regions" % (rel, len(entry["regions"])))
        slide.close()
    with open(os.path.join(out_dir, "manifest.json"), "w", newline="\n") as f:
        json.dump(manifest, f, indent=1, sort_keys=True)
        f.write("\n")


def main():
    if len(sys.argv) >= 2 and sys.argv[1] == "check-fixtures":
        here = os.path.dirname(os.path.abspath(__file__))
        data = sys.argv[2] if len(sys.argv) > 2 else os.path.join(here, "..", "src", "tests", "mrxs", "data")
        failures = check_fixtures(os.path.abspath(data))
        print("%d fixture(s) disagree with OpenSlide" % failures)
        sys.exit(1 if failures else 0)
    if len(sys.argv) == 4 and sys.argv[1] == "references":
        references(sys.argv[2], sys.argv[3])
        return
    sys.exit(__doc__)


if __name__ == "__main__":
    main()
