"""Generate synthetic MRXS (MIRAX) bundles and their expected geometry.

Writes the fixtures the slideio MRXS driver tests read, into src/tests/mrxs/data/
by default. Everything here is derived from the MIRAX/MRXS format description
(design: software-docs/specs/2026-11-06-mrxs-driver-design.md, section 9), and
deliberately shares no code with the C++ driver: a geometry mistake in the driver
must not also be baked into the fixtures that test it.

For each fixture <name> it writes:
  <name>.mrxs            empty entry file
  <name>/Slidedat.ini    metadata
  <name>/<index file>    hierarchical and non-hierarchical index
  <name>/<data files>    encoded payloads
  <name>.expected.json   what a correct reader reports

Valid fixtures' JSON holds level sizes, every drawable rectangle per level,
populated bounds, background colour, associated-image sizes and probe pixels.
Invalid fixtures' JSON holds the error category and when it must surface.

PNG and BMP payloads are encoded by hand (zlib from the standard library) so
regeneration is byte-identical regardless of the Pillow version; only JPEG
goes through Pillow, whose version is recorded in manifest.json.

Run with the Python interpreter of the documented environment (README.md):
    python -I generate.py [output-directory]
"""
import json
import math
import os
import shutil
import struct
import sys
import zlib
from io import BytesIO

GENERATOR_VERSION = 1


# ---------------------------------------------------------------------------
# Encoders

def _png_chunk(kind, data):
    return (struct.pack(">I", len(data)) + kind + data
            + struct.pack(">I", zlib.crc32(kind + data) & 0xFFFFFFFF))


def encode_png(pixels, width, height, channels=3):
    """pixels: rows top-down of channel-interleaved bytes."""
    colour_type = {1: 0, 3: 2, 4: 6}[channels]
    raw = b"".join(b"\x00" + bytes(row) for row in pixels)
    return (b"\x89PNG\r\n\x1a\n"
            + _png_chunk(b"IHDR", struct.pack(">IIBBBBB", width, height, 8, colour_type, 0, 0, 0))
            + _png_chunk(b"IDAT", zlib.compress(raw, 9))
            + _png_chunk(b"IEND", b""))


def encode_bmp24(pixels, width, height):
    """The documented BMP24 profile: 40-byte DIB, BI_RGB, bottom-up, BGR, 4-byte rows."""
    stride = 4 * math.ceil(3 * width / 4)
    body = bytearray()
    for row in reversed(pixels):
        line = bytearray()
        for x in range(width):
            r, g, b = row[3 * x:3 * x + 3]
            line += bytes((b, g, r))
        body += line + b"\x00" * (stride - len(line))
    header = struct.pack("<2sIHHI", b"BM", 54 + len(body), 0, 0, 54)
    dib = struct.pack("<IiiHHIIiiII", 40, width, height, 1, 24, 0, len(body), 2835, 2835, 0, 0)
    return header + dib + bytes(body)


def encode_bmp8(width, height, value):
    """An 8-bit palette BMP: outside the BMP24 profile, for rejection tests."""
    stride = 4 * math.ceil(width / 4)
    body = (bytes([value] * width) + b"\x00" * (stride - width)) * height
    palette = b"".join(bytes((i, i, i, 0)) for i in range(256))
    offset = 54 + len(palette)
    header = struct.pack("<2sIHHI", b"BM", offset + len(body), 0, 0, offset)
    dib = struct.pack("<IiiHHIIiiII", 40, width, height, 1, 8, 0, len(body), 2835, 2835, 256, 0)
    return header + dib + palette + body


def encode_jpeg(pixels, width, height):
    from PIL import Image
    image = Image.frombytes("RGB", (width, height), b"".join(bytes(r) for r in pixels))
    out = BytesIO()
    # 4:4:4 and high quality: uniform blocks survive close to exactly.
    image.save(out, format="JPEG", quality=95, subsampling=0, optimize=False)
    return out.getvalue()


def encode(fmt, pixels, width, height):
    if fmt == "PNG":
        return encode_png(pixels, width, height)
    if fmt == "BMP24":
        return encode_bmp24(pixels, width, height)
    if fmt == "JPEG":
        return encode_jpeg(pixels, width, height)
    raise ValueError(fmt)


# ---------------------------------------------------------------------------
# Content

def block_colour(level, x, y, xi, yi):
    """A deterministic, distinct mid-range colour per subtile block."""
    h = (level * 7919 + x * 104729 + y * 1299709 + xi * 15485863 + yi * 32452843) & 0xFFFFFFFF
    h = (h ^ (h >> 13)) * 0x5BD1E995 & 0xFFFFFFFF
    return (40 + h % 176, 40 + (h >> 8) % 176, 40 + (h >> 16) % 176)


def uniform_pixels(width, height, colour):
    return [list(colour) * width for _ in range(height)]


def block_pixels(width, height, blocks, colour_of):
    """blocks x blocks uniform regions; block (xi, yi) spans [xi*w/blocks, (xi+1)*w/blocks)."""
    rows = []
    for py in range(height):
        yi = min(blocks - 1, int(py * blocks / height))
        row = []
        for px in range(width):
            xi = min(blocks - 1, int(px * blocks / width))
            row += colour_of(xi, yi)
        rows.append(row)
    return rows


# ---------------------------------------------------------------------------
# Geometry, straight from the format description (sections 8-10)

def geometry(spec):
    nx, ny, d = spec["Nx"], spec["Ny"], spec["D"]
    levels = spec["levels"]
    px, py = nx // d, ny // d
    cum = []
    e = 0
    for lv in levels:
        e += lv["exp"]
        cum.append(1 << e)
    c0 = cum[0]
    w0, h0 = levels[0]["W"], levels[0]["H"]
    ox0, oy0 = levels[0]["overlap_x"], levels[0]["overlap_y"]

    def nominal(n, size, overlap):
        total = 0
        for i in range(n):
            if i % d == d - 1 and i != n - 1:
                total = math.trunc(total + size - overlap)
            else:
                total = total + size
        return total

    bwidth, bheight = nominal(nx, w0, ox0), nominal(ny, h0, oy0)

    pos = spec.get("positions")
    if pos is not None:
        positions = [(rec[1] * c0, rec[2] * c0) for rec in pos["records"]]
    else:
        positions = [(math.trunc(xp * (w0 * d - ox0)), math.trunc(yp * (h0 * d - oy0)))
                     for yp in range(py) for xp in range(px)]
    position_aware = pos is not None or ox0 != 0 or oy0 != 0

    present0 = set(spec["images0"])
    active = set()
    out_levels = []
    for l, lv in enumerate(levels):
        cl = cum[l]
        ppi = max(1, cl // d)
        ipp = max(1, d // cl)
        if position_aware:
            divisor, tiles, q = min(cl, d), ppi, 1
        else:
            divisor, tiles, q = cl, 1, ppi
        tw, th = lv["W"] / tiles, lv["H"] / tiles
        images = sorted({(x - x % cl, y - y % cl) for (x, y) in present0},
                        key=lambda p: (p[1], p[0]))
        drawables = []
        for (x, y) in images:
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
                    p0x = positions[cp][0] + w0 * (xx - xp * d)
                    p0y = positions[cp][1] + h0 * (yy - yp * d)
                    if l == 0:
                        if positions[cp] == (0, 0) and (xp, yp) != (0, 0):
                            continue
                        active.add(cp)
                    else:
                        found = any((cy * px + cx) in active
                                    for cy in range(yp, min(yp + q, py))
                                    for cx in range(xp, min(xp + q, px)))
                        if not found:
                            continue
                    drawables.append({
                        "image": [x, y], "subtile": [xi, yi],
                        "source": [xi * tw, yi * th, tw, th],
                        "destination": [p0x / cl, p0y / cl, tw, th],
                        "grid": [y // divisor + yi, x // divisor + xi],
                        "colour": list(block_colour(l, x, y, xi, yi)),
                    })
        out_levels.append({
            "concat": cl, "downsample": cl // c0, "tiles_per_image": tiles,
            "width": bwidth // cl, "height": bheight // cl,
            "image_size": [lv["W"], lv["H"]], "format": lv["format"],
            "images": images, "drawables": drawables,
        })
    return {
        "position_aware": position_aware, "nominal_lattice": [bwidth, bheight],
        "camera_grid": [px, py], "levels": out_levels,
    }


def bounds(level):
    rects = [dr["destination"] for dr in level["drawables"]]
    if not rects:
        return None
    x0 = min(r[0] for r in rects)
    y0 = min(r[1] for r in rects)
    x1 = max(r[0] + r[2] for r in rects)
    y1 = max(r[1] + r[3] for r in rects)
    return [x0, y0, x1 - x0, y1 - y0]


def probes(level, limit=6, margin=1.5):
    """Pixels covered by exactly one drawable, well inside it: their colour is exact."""
    width, height = level["width"], level["height"]
    rects = [dr["destination"] for dr in level["drawables"]]
    found = []
    for i, dr in enumerate(level["drawables"]):
        x, y, w, h = dr["destination"]
        cx, cy = int(math.floor(x + w / 2)), int(math.floor(y + h / 2))
        if not (x + margin <= cx and cx + 1 <= x + w - margin and y + margin <= cy and cy + 1 <= y + h - margin):
            continue
        if not (0 <= cx < width and 0 <= cy < height):
            continue
        clash = False
        for j, (ox, oy, ow, oh) in enumerate(rects):
            if j != i and ox - margin < cx + 1 and cx < ox + ow + margin and oy - margin < cy + 1 and cy < oy + oh + margin:
                clash = True
                break
        if not clash:
            found.append({"x": cx, "y": cy, "rgb": dr["colour"]})
        if len(found) >= limit:
            break
    return found


def hole_probes(level, background, limit=2, margin=2.0):
    """Canvas pixels no drawable comes near: they take the background colour."""
    rects = [dr["destination"] for dr in level["drawables"]]
    found = []
    for cy in range(level["height"]):
        for cx in range(level["width"]):
            if all(not (ox - margin < cx + 1 and cx < ox + ow + margin and oy - margin < cy + 1 and cy < oy + oh + margin)
                   for (ox, oy, ow, oh) in rects):
                found.append({"x": cx, "y": cy, "rgb": list(background)})
                if len(found) >= limit:
                    return found
    return found


def dump_compact(value, indent=0):
    """JSON with one line per drawable, probe or scalar entry: reviewable, and small."""
    pad = " " * indent
    if isinstance(value, dict) and any(isinstance(v, (dict, list)) and v and isinstance(v, dict) or
                                       (isinstance(v, list) and v and isinstance(v[0], dict))
                                       for v in value.values()) or (isinstance(value, dict) and indent == 0):
        items = ['%s "%s": %s' % (pad, k, dump_compact(value[k], indent + 1).lstrip()) for k in sorted(value)]
        return "{\n" + ",\n".join(items) + "\n" + pad + "}" + ("\n" if indent == 0 else "")
    if isinstance(value, list) and value and isinstance(value[0], dict):
        items = [pad + " " + dump_compact(v, indent + 1).lstrip() for v in value]
        return "[\n" + ",\n".join(items) + "\n" + pad + "]"
    return json.dumps(value, sort_keys=True, separators=(",", ":"))


# ---------------------------------------------------------------------------
# Bundle writer

class Bundle:
    def __init__(self, spec):
        self.spec = spec
        self.data = [bytearray() for _ in spec["data_files"]]
        self.next_file = 0
        self.offsets = {}

    def store(self, payload, file_number=None):
        if file_number is None:
            file_number = self.next_file
            self.next_file = (self.next_file + 1) % len(self.data)
        buf = self.data[file_number]
        offset = len(buf)
        buf += payload
        return (offset, len(payload), file_number)


def build_index(spec, geo, bundle, assoc_refs, position_ref, extra_nonhier):
    sid = spec["slide_id"].encode("utf-8")
    index = bytearray(b"01.02" + sid + b"\x00" * 8)
    hier_ptr_at, nonhier_ptr_at = 5 + len(sid), 9 + len(sid)

    nlevels = len(spec["levels"])
    hier_slots = nlevels * spec.get("hier_slot_multiplier", 1)
    hier_table = len(index)
    index += b"\x00" * (4 * hier_slots)
    nonhier = spec["_nonhier"]
    nonhier_slots = sum(len(f["values"]) for f in nonhier)
    nonhier_table = len(index)
    index += b"\x00" * (4 * nonhier_slots)
    struct.pack_into("<i", index, hier_ptr_at, hier_table)
    struct.pack_into("<i", index, nonhier_ptr_at, nonhier_table)
    bundle.offsets["hier_table"] = hier_table

    page_size = spec.get("page_size")
    for l, lv in enumerate(geo["levels"]):
        entries = spec["_records"][l]
        head = len(index)
        index += struct.pack("<ii", 0, 0)
        struct.pack_into("<i", index, hier_table + 4 * l, head)
        chunks = [entries[i:i + page_size] for i in range(0, len(entries), page_size)] if page_size else [entries]
        # Pages are written in reverse so the chain is not contiguous in the file.
        order = list(reversed(range(len(chunks)))) if spec.get("reverse_pages") else list(range(len(chunks)))
        page_at = {}
        for k in order:
            page_at[k] = len(index)
            index += struct.pack("<ii", len(chunks[k]), 0)
            for (image_index, offset, length, file_number) in chunks[k]:
                index += struct.pack("<iiii", image_index, offset, length, file_number)
        struct.pack_into("<i", index, head + 4, page_at[0])
        for k in range(len(chunks) - 1):
            struct.pack_into("<i", index, page_at[k] + 4, page_at[k + 1])
        bundle.offsets.setdefault("level_first_page", []).append(page_at[0])

    ordinal = 0
    for fam in nonhier:
        for value in fam["values"]:
            head = len(index)
            struct.pack_into("<i", index, nonhier_table + 4 * ordinal, head)
            entries = value.get("entries")
            if not entries:
                index += struct.pack("<ii", 0, 0)
            else:
                index += struct.pack("<ii", 0, head + 8)
                page = len(index)
                index += struct.pack("<ii", len(entries), 0)
                for (t0, t1, (offset, length, file_number)) in entries:
                    index += struct.pack("<iiiii", t0, t1, offset, length, file_number)
                if value.get("next_page"):
                    # A nonzero next-page pointer to a second page the reader must not need.
                    nxt = len(index)
                    struct.pack_into("<i", index, page + 4, nxt)
                    t0, t1, ref = value["next_page"]
                    index += struct.pack("<ii", 1, 0) + struct.pack("<iiiii", t0, t1, *ref)
                bundle.offsets[value["name"] + "_page"] = page
            ordinal += 1
    return index


def ini_text(spec, geo):
    lines = []

    def group(name):
        lines.append("[%s]" % name)

    def kv(key, value):
        lines.append("%s = %s" % (key, value))

    group("GENERAL")
    kv("SLIDE_VERSION", "01.03")
    kv("SLIDE_NAME", spec["name"])
    kv("SLIDE_ID", spec["slide_id"])
    kv("IMAGENUMBER_X", spec["Nx"])
    kv("IMAGENUMBER_Y", spec["Ny"])
    kv("CURRENT_SLIDE_VERSION", "2.2")
    kv("OBJECTIVE_MAGNIFICATION", spec.get("objective", 20))
    kv("SLIDE_TYPE", "SLIDE_TYPE_BRIGHTFIELD")
    kv("CameraImageDivisionsPerSide", spec["D"])
    for extra in spec.get("ini_general_extra", []):
        lines.append(extra)

    group("HIERARCHICAL")
    kv("INDEXFILE", spec["index_file"])
    kv("PAGEELEMENTCOUNT", spec.get("page_element_count", 128))
    kv("PAGELENGTH", 2048)
    hier = [("Slide zoom level", ["ZoomLevel_%d" % l for l in range(len(spec["levels"]))], "LAYER_0_LEVEL_%d_SECTION")]
    if spec.get("hier_slot_multiplier", 1) > 1:
        hier.append(("Slide filter level",
                     ["FilterLevel_%d" % j for j in range(spec["hier_slot_multiplier"])], "LAYER_1_LEVEL_%d_SECTION"))
    kv("HIER_COUNT", len(hier))
    kv("NONHIER_COUNT", len(spec["_nonhier"]))
    for i, (name, values, section) in enumerate(hier):
        kv("HIER_%d_NAME" % i, name)
        kv("HIER_%d_COUNT" % i, len(values))
        for j, value in enumerate(values):
            kv("HIER_%d_VAL_%d" % (i, j), value)
            kv("HIER_%d_VAL_%d_SECTION" % (i, j), section % j)
    for i, fam in enumerate(spec["_nonhier"]):
        kv("NONHIER_%d_NAME" % i, fam["name"])
        kv("NONHIER_%d_COUNT" % i, len(fam["values"]))
        for j, value in enumerate(fam["values"]):
            kv("NONHIER_%d_VAL_%d" % (i, j), value["name"])
            kv("NONHIER_%d_VAL_%d_SECTION" % (i, j), "NONHIERLAYER_%d_LEVEL_%d_SECTION" % (i, j))

    group("DATAFILE")
    kv("FILE_COUNT", len(spec["data_files"]))
    for i, name in enumerate(spec["data_files"]):
        kv("FILE_%d" % i, name)

    for l, lv in enumerate(spec["levels"]):
        group("LAYER_0_LEVEL_%d_SECTION" % l)
        kv("IMAGE_FILL_COLOR_BGR", spec["fill_bgr"])
        kv("MICROMETER_PER_PIXEL_X", repr(spec["mpp"][0] * geo["levels"][l]["downsample"]))
        kv("MICROMETER_PER_PIXEL_Y", repr(spec["mpp"][1] * geo["levels"][l]["downsample"]))
        kv("DIGITIZER_WIDTH", lv["W"])
        kv("DIGITIZER_HEIGHT", lv["H"])
        kv("OVERLAP_X", repr(float(lv["overlap_x"])))
        kv("OVERLAP_Y", repr(float(lv["overlap_y"])))
        kv("IMAGE_CONCAT_FACTOR", lv["exp"])
        kv("IMAGE_FORMAT", lv.get("declared_format", lv["format"]))
    if spec.get("hier_slot_multiplier", 1) > 1:
        for j in range(spec["hier_slot_multiplier"]):
            group("LAYER_1_LEVEL_%d_SECTION" % j)
            kv("FILTER_NAME", "Filter%d" % j)
            # Which stored image group holds this filter (format description 12.4).
            kv("DATA_IN_THIS_FILTER_LEVEL", spec.get("filter_groups", ["FilterLevel_0"] * spec["hier_slot_multiplier"])[j])
            kv("STORING_CHANNEL_NUMBER", j % 3)
    for i, fam in enumerate(spec["_nonhier"]):
        for j, value in enumerate(fam["values"]):
            group("NONHIERLAYER_%d_LEVEL_%d_SECTION" % (i, j))
            for key, val in value.get("section", {}).items():
                kv(key, val)
    for extra in spec.get("ini_tail", []):
        lines.append(extra)

    eol = "\r\n" if spec.get("crlf") else "\n"
    text = eol.join(lines) + eol
    data = text.encode("utf-8")
    if spec.get("bom"):
        data = b"\xef\xbb\xbf" + data
    return data


ASSOC = [("ScanDataLayer_SlideThumbnail", "THUMBNAIL", "Macro", (30, 70), (90, 110, 160)),
         ("ScanDataLayer_SlideBarcode", "BARCODE", "Label", (24, 20), (200, 190, 60)),
         ("ScanDataLayer_SlidePreview", "PREVIEW", "Thumbnail", (18, 40), (60, 170, 90))]


def write_fixture(spec, root):
    name = spec["name"]
    geo = geometry(spec)
    bundle = Bundle(spec)

    # Pyramid payloads, one record per encoded image.
    records = []
    for l, lv in enumerate(geo["levels"]):
        level_spec = spec["levels"][l]
        level_records = []
        tiles = lv["tiles_per_image"]
        for (x, y) in lv["images"]:
            w, h = level_spec["W"], level_spec["H"]
            pixels = block_pixels(w, h, tiles, lambda xi, yi, x=x, y=y: list(block_colour(l, x, y, xi, yi)))
            payload = spec.get("payload_override", {}).get((l, x, y))
            if payload is None:
                payload = encode(level_spec["format"], pixels, w, h)
            ref = bundle.store(payload)
            level_records.append((y * spec["Nx"] + x, ref[0], ref[1], ref[2]))
        records.append(level_records)
    spec["_records"] = records

    # Non-hierarchical families.
    nonhier = []
    assoc_expected = {}
    if spec.get("associated", True):
        values = []
        for value_name, key, public, (w, h), colour in ASSOC:
            ref = bundle.store(encode_jpeg(uniform_pixels(w, h, colour), w, h))
            values.append({"name": value_name, "entries": [(0, 0, ref)],
                           "section": {key + "_IMAGE_TYPE": "JPEG", key + "_IMAGE_WIDTH": w, key + "_IMAGE_HEIGHT": h},
                           **({"next_page": (0, 0, ref)} if spec.get("nonhier_next_page") else {})})
            assoc_expected[public] = {"width": w, "height": h, "rgb": list(colour)}
        values.append({"name": "ScanDataLayer_Empty", "entries": []})
        nonhier.append({"name": "Scan data layer", "values": values})
    if spec.get("positions") is not None:
        pos = spec["positions"]
        raw = b"".join(struct.pack("<Bii", flag, x, y) for (flag, x, y) in pos["records"])
        raw = raw[:len(raw) - pos.get("truncate", 0)]
        family = "VIMSLIDE_POSITION_BUFFER" if pos["kind"] == "raw" else "StitchingIntensityLayer"
        payload = raw if pos["kind"] == "raw" else zlib.compress(raw, 9)
        payload = payload[:len(payload) - pos.get("truncate_stream", 0)]
        value = "default" if pos["kind"] == "raw" else "StitchingIntensityLevel"
        nonhier.append({"name": family, "values": [{"name": value, "entries": [(0, 0, bundle.store(payload))]}]})
    if spec.get("tagged_family"):
        ref = bundle.store(b"<scaninfo/>")
        nonhier.append({"name": "Scan info data layer", "values": [
            {"name": "ScanInfoDataLevel", "entries": [(0, 1, ref), (1, 6, ref), (2, 6, ref)]}]})
    spec["_nonhier"] = nonhier

    index = build_index(spec, geo, bundle, None, None, None)
    ini = ini_text(spec, geo)
    for mutate in spec.get("mutations", []):
        index, ini = mutate(spec, bundle, index, ini)

    directory = os.path.join(root, name)
    os.makedirs(directory)
    with open(os.path.join(root, name + ".mrxs"), "wb"):
        pass
    with open(os.path.join(directory, "Slidedat.ini"), "wb") as f:
        f.write(ini)
    with open(os.path.join(directory, spec["index_file"]), "wb") as f:
        f.write(index)
    for i, file_name in enumerate(spec["data_files"]):
        if file_name in spec.get("omit_files", []):
            continue
        with open(os.path.join(directory, file_name), "wb") as f:
            f.write(bundle.data[i])

    fill = spec["fill_bgr"]
    background = [fill & 255, (fill >> 8) & 255, (fill >> 16) & 255]
    if "error" in spec:
        expected = {"name": name, "error": spec["error"], "error_on": spec.get("error_on", "open"),
                    "reason": spec["reason"]}
    else:
        levels = []
        for l, lv in enumerate(geo["levels"]):
            levels.append({
                # Every hierarchical record of the level: [image index, file, offset, length].
                "records": sorted([idx, fileno, off, length] for (idx, off, length, fileno) in spec["_records"][l]),
                "width": lv["width"], "height": lv["height"], "downsample": lv["downsample"],
                "concat": lv["concat"], "format": lv["format"], "image_size": lv["image_size"],
                "drawables": [{k: dr[k] for k in ("image", "subtile", "source", "destination", "grid")}
                              for dr in lv["drawables"]],
                "probes": probes(lv), "holes": hole_probes(lv, background),
            })
        expected = {
            "name": name, "slide_id": spec["slide_id"], "image_grid": [spec["Nx"], spec["Ny"], spec["D"]],
            "data_files": len(spec["data_files"]),
            "positions": spec["positions"]["kind"] if spec.get("positions") else None,
            "position_aware": geo["position_aware"],
            "nominal_lattice": geo["nominal_lattice"], "camera_grid": geo["camera_grid"],
            "background_rgb": background, "mpp_um": spec["mpp"], "objective": spec.get("objective", 20),
            "bounds_level0": bounds(geo["levels"][0]), "levels": levels,
            "associated": assoc_expected,
            "pixel_tolerance": 0 if all(l["format"] != "JPEG" for l in spec["levels"]) else 6,
        }
        if spec.get("openslide_incompatible"):
            expected["openslide_incompatible"] = spec["openslide_incompatible"]
    with open(os.path.join(root, name + ".expected.json"), "w", newline="\n") as f:
        f.write(dump_compact(expected))


# ---------------------------------------------------------------------------
# Fixture definitions

def grid(nx, ny, step, missing=()):
    return [(x, y) for y in range(0, ny, step) for x in range(0, nx, step) if (x, y) not in missing]


def lattice_positions(px, py, w, h, d, overlap, jitter=None, overrides=None):
    records = []
    for yp in range(py):
        for xp in range(px):
            jx, jy = (jitter(xp, yp) if jitter else (0, 0))
            records.append([1 if (xp + yp) % 2 else 0,
                            int(xp * (w * d - overlap)) + jx, int(yp * (h * d - overlap)) + jy])
    for (xp, yp), rec in (overrides or {}).items():
        records[yp * px + xp] = list(rec)
    return records


def base_specs():
    specs = []

    # D=1, no map, zero overlap: the fast path, Q > 1 at coarser levels, a sparse hole.
    specs.append({
        "name": "nomap-d1", "slide_id": "0123456789ABCDEF0123456789ABCDEF",
        "Nx": 6, "Ny": 4, "D": 1, "mpp": [0.25, 0.26], "fill_bgr": 0x00336699,
        "levels": [dict(exp=0, W=8, H=8, overlap_x=0, overlap_y=0, format="PNG"),
                   dict(exp=1, W=8, H=8, overlap_x=0, overlap_y=0, format="PNG"),
                   dict(exp=1, W=8, H=8, overlap_x=0, overlap_y=0, format="PNG")],
        "images0": grid(6, 4, 1, missing={(5, 3), (4, 3), (0, 3)}),
        "index_file": "Index.dat", "data_files": ["Data0000.dat", "Data0001.dat"],
    })

    # D=2 with a raw map: jittered photos, a negative origin, an inactive camera with
    # garbage coordinates, and a populated camera whose coordinates are zero.
    px, py, w, d, ov = 4, 3, 12, 2, 2
    rawmap = lattice_positions(px, py, w, w, d, ov, jitter=lambda x, y: ((x * 3 + y) % 3 - 1, (y * 2 + x) % 3 - 1),
                               overrides={(0, 0): (1, -3, -2), (3, 2): (0, 777, -555), (1, 2): (0, 0, 0)})
    specs.append({
        "name": "rawmap-d2", "slide_id": "00112233445566778899AABBCCDDEEFF",
        "Nx": 8, "Ny": 6, "D": 2, "mpp": [0.5, 0.5], "fill_bgr": 0x00F0F0F0,
        "levels": [dict(exp=0, W=12, H=12, overlap_x=2, overlap_y=2, format="PNG"),
                   dict(exp=1, W=12, H=12, overlap_x=1, overlap_y=1, format="PNG"),
                   dict(exp=1, W=12, H=12, overlap_x=0.5, overlap_y=0.5, format="PNG"),
                   dict(exp=1, W=12, H=12, overlap_x=0.25, overlap_y=0.25, format="PNG")],
        "images0": grid(8, 6, 1, missing={(6, 4), (7, 4), (6, 5), (7, 5)}),
        "positions": {"kind": "raw", "records": rawmap},
        "index_file": "Index.dat", "data_files": ["Data0000.dat", "Data0001.dat", "Data0002.dat"],
    })

    # The same slide with a zlib-compressed map and JPEG payloads.
    specs.append({**specs[-1], "name": "zlibmap-d2-jpeg", "slide_id": "FFEEDDCCBBAA99887766554433221100",
                  "levels": [{**lv, "format": "JPEG"} for lv in specs[-1]["levels"]],
                  "positions": {"kind": "zlib", "records": rawmap}})

    # A saved slide: exponents [2,1,2], so C = [4, 8, 32] with C_0 > D.
    px, py = 8, 4
    savedmap = lattice_positions(px, py, 16, 16, 2, 3)
    specs.append({
        "name": "saved-c0-4", "slide_id": "A1B2C3D4E5F60718293A4B5C6D7E8F90",
        "Nx": 16, "Ny": 8, "D": 2, "mpp": [1.0, 1.0], "fill_bgr": 0x00FFFFFF,
        "levels": [dict(exp=2, W=16, H=16, overlap_x=3, overlap_y=3, format="PNG"),
                   dict(exp=1, W=16, H=16, overlap_x=1.5, overlap_y=1.5, format="PNG"),
                   dict(exp=2, W=16, H=16, overlap_x=0.375, overlap_y=0.375, format="PNG")],
        "images0": grid(16, 8, 4, missing={(12, 4)}),
        # Stored coordinates are in stored-level-0 pixels; the reader multiplies them by C_0.
        "positions": {"kind": "raw", "records": [[f, x // 4, y // 4] for (f, x, y) in savedmap]},
        "index_file": "Index.dat", "data_files": ["Data0000.dat"],
    })

    # No map but a fractional overlap: synthesized positions on the position-aware path,
    # with subtiles 2.5 pixels wide at the coarsest level.
    specs.append({
        "name": "nomap-overlap-d2", "slide_id": "5A5A5A5A5A5A5A5A5A5A5A5A5A5A5A5A",
        "Nx": 8, "Ny": 4, "D": 2, "mpp": [0.3, 0.3], "fill_bgr": 0x00202020,
        "levels": [dict(exp=0, W=10, H=10, overlap_x=2.5, overlap_y=2.5, format="PNG"),
                   dict(exp=1, W=10, H=10, overlap_x=1.25, overlap_y=1.25, format="PNG"),
                   dict(exp=1, W=10, H=10, overlap_x=0.625, overlap_y=0.625, format="PNG"),
                   dict(exp=1, W=10, H=10, overlap_x=0.3125, overlap_y=0.3125, format="PNG")],
        "images0": grid(8, 4, 1),
        "index_file": "Index.dat", "data_files": ["Data0000.dat", "Data0001.dat"],
    })

    # Layout variants that must not change the result: a 36-byte identifier, nonstandard
    # file names, BOM, CRLF, an empty-key line, several pages per level each longer than
    # PAGEELEMENTCOUNT, a second hierarchical family with null slots, a nonzero
    # non-hierarchical next-page pointer, and a tagged family the reader must ignore.
    specs.append({
        "name": "layout-variants", "slide_id": "6f1c2a3b-4d5e-4f60-8a7b-9c0d1e2f3a4b",
        "Nx": 5, "Ny": 4, "D": 1, "mpp": [0.25, 0.25], "fill_bgr": 0x00C08040,
        "levels": [dict(exp=0, W=8, H=6, overlap_x=0, overlap_y=0, format="PNG"),
                   dict(exp=1, W=8, H=6, overlap_x=0, overlap_y=0, format="PNG")],
        "images0": grid(5, 4, 1, missing={(2, 1)}),
        "index_file": "slide.idx", "data_files": ["chunk_a.bin", "chunk_b.bin", "chunk_c.bin"],
        "bom": True, "crlf": True, "ini_general_extra": ["= value without a key"],
        "page_size": 3, "page_element_count": 2,
        "hier_slot_multiplier": 3, "nonhier_next_page": True, "tagged_family": True,
    })

    # BMP24 payloads 13 pixels wide: rows padded to a multiple of four bytes. No associated
    # images, so no Scan data layer at all.
    specs.append({
        "name": "bmp24", "slide_id": "B0B0B0B0B0B0B0B0B0B0B0B0B0B0B0B0", "associated": False,
        "Nx": 4, "Ny": 2, "D": 1, "mpp": [0.5, 0.5], "fill_bgr": 0x00FFFFFF,
        "levels": [dict(exp=0, W=13, H=7, overlap_x=0, overlap_y=0, format="BMP24"),
                   dict(exp=1, W=13, H=7, overlap_x=0, overlap_y=0, format="BMP24")],
        "images0": grid(4, 2, 1),
        "index_file": "Index.dat", "data_files": ["Data0000.dat"],
    })

    # A page chain written out of file order. The format description says to follow
    # each page's next pointer; OpenSlide 4.0.1 instead reads the next page directly
    # after the current one and only tests the pointer for zero, so it misreads this
    # bundle. Real bundles write pages contiguously, where the two agree. The driver
    # follows the pointers; the reference harness skips the OpenSlide comparison here.
    specs.append({**specs[0], "name": "pages-out-of-order", "slide_id": "0E0E0E0E0E0E0E0E0E0E0E0E0E0E0E0E",
                  "page_size": 3, "reverse_pages": True, "openslide_incompatible":
                  "OpenSlide reads hierarchical pages sequentially instead of following next pointers"})

    # Levels in different codecs: compression has no single answer.
    specs.append({**specs[0], "name": "mixed-codecs", "slide_id": "3C3C3C3C3C3C3C3C3C3C3C3C3C3C3C3C",
                  "levels": [specs[0]["levels"][0], {**specs[0]["levels"][1], "format": "JPEG"},
                             {**specs[0]["levels"][2], "format": "BMP24"}]})

    # Repeated key and repeated group: the reader must follow GKeyFile. Which value
    # wins is established against OpenSlide by the reference harness.
    specs.append({
        **specs[0], "name": "ini-duplicates", "slide_id": "D0D0D0D0D0D0D0D0D0D0D0D0D0D0D0D0",
        "ini_general_extra": ["SLIDE_NAME = first-value", "SLIDE_NAME = last-value"],
        "ini_tail": ["[EXTRA_GROUP]", "KEY_A = a", "[EXTRA_GROUP]", "KEY_B = b"],
    })
    return specs


def _patch_i32(index, offset, value):
    index = bytearray(index)
    struct.pack_into("<i", index, offset, value)
    return index


def invalid_specs(valid):
    by_name = {s["name"]: s for s in valid}
    base, mapped, bmp = by_name["nomap-d1"], by_name["rawmap-d2"], by_name["bmp24"]
    specs = []

    def variant(src, name, error, reason, **extra):
        spec = {k: v for k, v in src.items() if not k.startswith("_")}
        spec.update(name=name, error=error, reason=reason, **extra)
        specs.append(spec)

    variant(base, "bad-slide-id", "corrupt", "index slide identifier differs from SLIDE_ID",
            mutations=[lambda s, b, idx, ini: (idx[:5] + b"X" + idx[6:], ini)])
    variant(base, "bad-page-cycle", "corrupt", "level-0 page chain points back to itself",
            page_size=4, mutations=[lambda s, b, idx, ini:
                                    (_patch_i32(idx, b.offsets["level_first_page"][0] + 4,
                                                b.offsets["level_first_page"][0]), ini)])
    variant(base, "bad-payload-range", "corrupt", "a level-0 payload extends past the end of its data file",
            mutations=[lambda s, b, idx, ini:
                       (_patch_i32(idx, b.offsets["level_first_page"][0] + 8 + 8, 1 << 20), ini)])
    variant(base, "bad-negative-pointer", "corrupt", "the hierarchical table pointer is negative",
            mutations=[lambda s, b, idx, ini: (_patch_i32(idx, 5 + len(s["slide_id"]), -8), ini)])
    variant(base, "missing-data-file", "corrupt", "a data file listed in [DATAFILE] does not exist",
            omit_files=["Data0001.dat"])
    variant(mapped, "bad-map-length", "corrupt", "the raw position map is one record short",
            positions={"kind": "raw", "records": mapped["positions"]["records"], "truncate": 9})
    variant(mapped, "bad-map-flag", "corrupt", "a position record has flag 2 (only 0 and 1 are valid)",
            positions={"kind": "raw", "records": [[2, x, y] if i == 5 else [f, x, y]
                                                  for i, (f, x, y) in enumerate(mapped["positions"]["records"])]})
    variant(mapped, "bad-zlib-map", "corrupt", "the compressed position map is a truncated zlib stream",
            positions={"kind": "zlib", "records": mapped["positions"]["records"], "truncate_stream": 12})
    variant(base, "codec-mismatch", "corrupt", "IMAGE_FORMAT says PNG but one level-0 payload is a BMP",
            error_on="read", payload_override={(0, 1, 0): encode_bmp24(uniform_pixels(8, 8, (9, 9, 9)), 8, 8)})
    variant(base, "wrong-image-size", "corrupt", "a level-0 PNG is 9 x 8 pixels where DIGITIZER_WIDTH says 8",
            error_on="read", payload_override={(0, 1, 0): encode_png(uniform_pixels(9, 8, (9, 9, 9)), 9, 8)})
    variant(bmp, "bmp-not-24bit", "corrupt", "a BMP24 level holds an 8-bit palette BMP",
            error_on="read", payload_override={(0, 1, 0): encode_bmp8(13, 7, 128)})
    variant(base, "bad-version", "unsupported", "the index version is 01.03, not 01.02",
            mutations=[lambda s, b, idx, ini: (b"01.03" + idx[5:], ini)])
    variant(base, "duplicate-record", "corrupt", "two level-0 records share an image index",
            mutations=[lambda s, b, idx, ini:
                       (_patch_i32(idx, b.offsets["level_first_page"][0] + 8 + 16,
                                   struct.unpack_from("<i", idx, b.offsets["level_first_page"][0] + 8)[0]), ini)])
    variant(base, "malformed-divisions", "corrupt", "CameraImageDivisionsPerSide is not a number",
            mutations=[lambda s, b, idx, ini:
                       (idx, ini.replace(b"CameraImageDivisionsPerSide = 1", b"CameraImageDivisionsPerSide = one"))])
    variant(base, "truncated-index", "corrupt", "the index file ends in the middle of level 0's page",
            mutations=[lambda s, b, idx, ini: (idx[:b.offsets["level_first_page"][0] + 8 + 16 * 3], ini)])
    variant(base, "two-image-groups", "unsupported",
            "the filter sections reference two stored image groups (a multichannel bundle)",
            hier_slot_multiplier=4, filter_groups=["FilterLevel_0", "FilterLevel_0", "FilterLevel_0", "FilterLevel_1"])
    variant(base, "bad-exponent", "corrupt", "a level after the first has IMAGE_CONCAT_FACTOR 0",
            levels=[base["levels"][0], {**base["levels"][1], "exp": 0}, base["levels"][2]])
    variant(base, "unsupported-format", "unsupported", "IMAGE_FORMAT is JPEG2000",
            levels=[{**lv, "declared_format": "JPEG2000"} for lv in base["levels"]])
    variant(base, "data-file-escape", "corrupt", "a [DATAFILE] name leads outside the bundle directory",
            data_files=["Data0000.dat", "../escape.dat"], omit_files=["../escape.dat"])
    variant(base, "nondivisible-grid", "unsupported", "IMAGENUMBER_X is not a multiple of D",
            Nx=6, D=4, images0=[(0, 0)], levels=[dict(exp=0, W=8, H=8, overlap_x=0, overlap_y=0, format="PNG")])
    return specs


def main():
    here = os.path.dirname(os.path.abspath(__file__))
    root = sys.argv[1] if len(sys.argv) > 1 else os.path.join(here, "..", "..", "src", "tests", "mrxs", "data")
    root = os.path.abspath(root)
    if os.path.isdir(root) and os.listdir(root):
        # Only ever clear a directory this generator wrote: a mistyped argument must
        # not delete someone's files.
        if not os.path.isfile(os.path.join(root, "manifest.json")):
            sys.exit("refusing to clear %s: it is not a directory this generator wrote" % root)
        for entry in os.listdir(root):
            path = os.path.join(root, entry)
            shutil.rmtree(path) if os.path.isdir(path) else os.remove(path)
    os.makedirs(root, exist_ok=True)
    valid = base_specs()
    for spec in valid:
        write_fixture(spec, root)
    for spec in invalid_specs(valid):
        write_fixture(spec, root)
    import PIL
    manifest = {"generator_version": GENERATOR_VERSION, "pillow": PIL.__version__,
                "fixtures": sorted(s["name"] for s in valid + invalid_specs(valid))}
    with open(os.path.join(root, "manifest.json"), "w", newline="\n") as f:
        json.dump(manifest, f, indent=1, sort_keys=True)
        f.write("\n")
    print("wrote %d fixtures to %s" % (len(manifest["fixtures"]), root))


if __name__ == "__main__":
    main()
