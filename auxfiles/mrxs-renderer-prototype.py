"""Renderer prototype for the slideio MRXS driver (plan step 3).

A Python model of the renderer the design specifies (section 6): a float,
premultiplied RGBA accumulator; each drawable extracted through a transparent
ceil(tile_width) x ceil(tile_height) intermediate with a fractional source
translation; placed at its fractional destination; composited in descending
grid row and column order with the saturating operator; flattened onto the
background colour last. Translations use bilinear sampling with transparent
outside the image, which is what Cairo's default filter does for them.

It reads real bundles itself -- INI, index, position map, payloads -- following
the format description, and compares its output with the OpenSlide references
written by auxfiles/mrxs-openslide-reference.py. The numbers it reports are what
the pixel tolerances of the C++ comparison tests are chosen from. It is a
measuring instrument, not shipping code, and is run by hand:

    python -I mrxs-renderer-prototype.py <images-root>/mirax/reference
"""
import io
import json
import math
import os
import struct
import sys
import zlib

import numpy as np
from PIL import Image


# ---------------------------------------------------------------------------
# Reading a bundle

def read_ini(path):
    data = open(path, "rb").read()
    if data.startswith(b"\xef\xbb\xbf"):
        data = data[3:]
    groups, group = {}, None
    for line in data.decode("utf-8").splitlines():
        s = line.strip()
        if not s or s.startswith("#") or s.startswith("="):
            continue
        if s.startswith("[") and s.endswith("]"):
            group = groups.setdefault(s[1:-1], {})
            continue
        key, _, value = s.partition("=")
        group[key.strip()] = value.strip()          # a repeated key keeps its last value
    return groups


class Bundle:
    def __init__(self, entry):
        self.dir = os.path.splitext(entry)[0]
        self.ini = ini = read_ini(os.path.join(self.dir, "Slidedat.ini"))
        g, h, df = ini["GENERAL"], ini["HIERARCHICAL"], ini["DATAFILE"]
        self.nx, self.ny = int(g["IMAGENUMBER_X"]), int(g["IMAGENUMBER_Y"])
        self.d = int(g.get("CameraImageDivisionsPerSide", "1"))
        self.files = [os.path.join(self.dir, df["FILE_%d" % i]) for i in range(int(df["FILE_COUNT"]))]
        self.index = open(os.path.join(self.dir, h["INDEXFILE"]), "rb").read()
        assert h["HIER_0_NAME"] == "Slide zoom level"
        nlevels = int(h["HIER_0_COUNT"])
        self.levels = []
        e = 0
        for l in range(nlevels):
            sec = ini[h["HIER_0_VAL_%d_SECTION" % l]]
            e += int(sec["IMAGE_CONCAT_FACTOR"])
            self.levels.append(dict(concat=1 << e, W=int(sec["DIGITIZER_WIDTH"]), H=int(sec["DIGITIZER_HEIGHT"]),
                                    ox=float(sec["OVERLAP_X"]), oy=float(sec["OVERLAP_Y"]),
                                    fill=int(sec["IMAGE_FILL_COLOR_BGR"])))
        sid = g["SLIDE_ID"].encode("utf-8")
        assert self.index[:5] == b"01.02" and self.index[5:5 + len(sid)] == sid
        self.hier_table = self.i32(5 + len(sid))
        self.nonhier_table = self.i32(9 + len(sid))
        self.records = [self.level_records(l) for l in range(nlevels)]
        self.positions_raw = self.read_positions(h)

    def i32(self, off):
        return struct.unpack_from("<i", self.index, off)[0]

    def level_records(self, l):
        head = self.i32(self.hier_table + 4 * l)
        page = self.i32(head + 4)
        out, seen = [], set()
        while page:
            assert page not in seen
            seen.add(page)
            n, nxt = self.i32(page), self.i32(page + 4)
            for k in range(n):
                idx, off, length, fileno = struct.unpack_from("<iiii", self.index, page + 8 + 16 * k)
                out.append((idx % self.nx, idx // self.nx, fileno, off, length))
            page = nxt
        return out

    def nonhier_first(self, h, family_name):
        ordinal = 0
        for i in range(int(h["NONHIER_COUNT"])):
            count = int(h["NONHIER_%d_COUNT" % i])
            if h["NONHIER_%d_NAME" % i] == family_name:
                head = self.i32(self.nonhier_table + 4 * ordinal)
                page = self.i32(head + 4)
                if not page:
                    return None
                _, _, off, length, fileno = struct.unpack_from("<iiiii", self.index, page + 8)
                with open(self.files[fileno], "rb") as f:
                    f.seek(off)
                    return f.read(length)
            ordinal += count
        return None

    def read_positions(self, h):
        raw = self.nonhier_first(h, "VIMSLIDE_POSITION_BUFFER")
        if raw is None:
            packed = self.nonhier_first(h, "StitchingIntensityLayer")
            raw = zlib.decompress(packed) if packed is not None else None
        if raw is None:
            return None
        px, py = self.nx // self.d, self.ny // self.d
        assert len(raw) == 9 * px * py
        return [struct.unpack_from("<Bii", raw, 9 * i)[1:] for i in range(px * py)]

    def payload(self, fileno, off, length):
        with open(self.files[fileno], "rb") as f:
            f.seek(off)
            return f.read(length)


# ---------------------------------------------------------------------------
# Geometry (format description sections 8-10)

def drawables(b):
    d, nx, ny = b.d, b.nx, b.ny
    px, py = nx // d, ny // d
    c0 = b.levels[0]["concat"]
    w0, h0, ox0, oy0 = b.levels[0]["W"], b.levels[0]["H"], b.levels[0]["ox"], b.levels[0]["oy"]
    if b.positions_raw is not None:
        pos = [(x * c0, y * c0) for (x, y) in b.positions_raw]
    else:
        pos = [(math.trunc(xp * (w0 * d - ox0)), math.trunc(yp * (h0 * d - oy0)))
               for yp in range(py) for xp in range(px)]
    aware = b.positions_raw is not None or ox0 != 0 or oy0 != 0
    active = set()
    per_level = []
    for l, lv in enumerate(b.levels):
        cl = lv["concat"]
        ppi = max(1, cl // d)
        divisor, tiles, q = (min(cl, d), ppi, 1) if aware else (cl, 1, ppi)
        tw, th = lv["W"] / tiles, lv["H"] / tiles
        out = []
        for (x, y, fileno, off, length) in b.records[l]:
            for yi in range(tiles):
                yy = y + yi * d
                if yy >= ny:
                    break
                for xi in range(tiles):
                    xx = x + xi * d
                    if xx >= nx:
                        break
                    xp, yp = xx // d, yy // d
                    cp = yp * px + xp
                    p0x = pos[cp][0] + w0 * (xx - xp * d)
                    p0y = pos[cp][1] + h0 * (yy - yp * d)
                    if l == 0:
                        if pos[cp] == (0, 0) and (xp, yp) != (0, 0):
                            continue
                        active.add(cp)
                    elif not any((cy * px + cx) in active for cy in range(yp, min(yp + q, py))
                                 for cx in range(xp, min(xp + q, px))):
                        continue
                    out.append((p0x / cl, p0y / cl, tw, th, xi * tw, yi * th,
                                y // divisor + yi, x // divisor + xi, (fileno, off, length)))
        per_level.append(out)
    return per_level


# ---------------------------------------------------------------------------
# Rendering

def translate(src, tx, ty, out_h, out_w):
    """Bilinear translation of a premultiplied float image; transparent outside it."""
    h, w = src.shape[:2]
    pad = np.zeros((h + 2, w + 2, 4), np.float64)
    pad[1:-1, 1:-1] = src
    u = np.arange(out_w) - tx            # source column whose centre this pixel's centre maps to
    v = np.arange(out_h) - ty
    u0, v0 = np.floor(u).astype(int), np.floor(v).astype(int)
    a, c = (u - u0)[None, :, None], (v - v0)[:, None, None]
    cu0, cu1 = np.clip(u0 + 1, 0, w + 1), np.clip(u0 + 2, 0, w + 1)
    cv0, cv1 = np.clip(v0 + 1, 0, h + 1), np.clip(v0 + 2, 0, h + 1)
    p00 = pad[cv0][:, cu0]; p01 = pad[cv0][:, cu1]; p10 = pad[cv1][:, cu0]; p11 = pad[cv1][:, cu1]
    return (1 - c) * ((1 - a) * p00 + a * p01) + c * ((1 - a) * p10 + a * p11)


def saturate(dst, src):
    As, Ad = src[..., 3:4], dst[..., 3:4]
    with np.errstate(divide="ignore", invalid="ignore"):
        fs = np.where(As > 0, np.minimum(1.0, (1.0 - Ad) / np.where(As > 0, As, 1)), 1.0)
    dst[..., :3] = src[..., :3] * fs + dst[..., :3]
    dst[..., 3:4] = np.minimum(As, 1.0 - Ad) + Ad


class Renderer:
    def __init__(self, bundle):
        self.b = bundle
        self.levels = drawables(bundle)
        self.cache = {}
        fill = bundle.levels[0]["fill"]
        self.bg = np.array([fill & 255, (fill >> 8) & 255, (fill >> 16) & 255], np.float64) / 255

    def decoded(self, ref):
        if ref not in self.cache:
            img = Image.open(io.BytesIO(self.b.payload(*ref))).convert("RGB")
            rgb = np.asarray(img, np.float64) / 255
            self.cache[ref] = np.concatenate([rgb, np.ones(rgb.shape[:2] + (1,))], axis=2)
        return self.cache[ref]

    def render(self, level, x, y, w, h):
        acc = np.zeros((h, w, 4), np.float64)
        hits = [dr for dr in self.levels[level]
                if dr[0] < x + w + 1 and dr[0] + dr[2] > x - 1 and dr[1] < y + h + 1 and dr[1] + dr[3] > y - 1]
        hits.sort(key=lambda dr: (dr[6], dr[7]), reverse=True)
        for (dx, dy, tw, th, sx, sy, _, _, ref) in hits:
            image = self.decoded(ref)
            if image.shape[1] > tw or image.shape[0] > th:
                tile = translate(image, -sx, -sy, math.ceil(th), math.ceil(tw))
            else:
                tile = image
            saturate(acc, translate(tile, dx - x, dy - y, h, w))
        rgb = acc[..., :3] + (1 - acc[..., 3:4]) * self.bg
        return np.clip(np.rint(rgb * 255), 0, 255).astype(np.uint8)


def main():
    ref_dir = sys.argv[1]
    images_root = os.path.dirname(os.path.dirname(os.path.abspath(ref_dir)))
    manifest = json.load(open(os.path.join(ref_dir, "manifest.json")))
    print("%-30s %5s %8s %7s %7s %9s" % ("bundle", "level", "maxdiff", "mean", ">2 (%)", "geometry"))
    summary = []
    for rel, entry in manifest["bundles"].items():
        bundle = Bundle(os.path.join(images_root, "mirax", rel))
        r = Renderer(bundle)
        name = rel.split("/")[1][:30]
        for region in entry["regions"]:
            ref = np.asarray(Image.open(os.path.join(ref_dir, region["file"])).convert("RGB"), np.int16)
            got = r.render(region["level"], region["x"], region["y"], region["width"], region["height"]).astype(np.int16)
            diff = np.abs(got - ref)
            row = (name, region["level"], int(diff.max()), float(diff.mean()), 100.0 * float((diff > 2).any(axis=2).mean()))
            summary.append(row)
            print("%-30s %5d %8d %7.3f %7.3f" % row)
        r.cache.clear()
    worst = max(summary, key=lambda s: s[2])
    print("\nregions: %d; worst max diff %d (%s level %d); worst mean %.3f"
          % (len(summary), worst[2], worst[0], worst[1], max(s[3] for s in summary)))


if __name__ == "__main__":
    main()
