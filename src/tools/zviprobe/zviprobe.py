#!/usr/bin/env python3
"""Structure-only probe for Zeiss ZVI (OLE compound) files.

Reports why the slideio ZVI driver cannot open a file, for cases where the
file itself cannot be shared. It reads the compound-document directory and
replays the driver's own parse of /Image/Contents, /Image/Tags/Contents and
each /Image/Item(n), and prints where that parse stops.

What it prints is structure, not content: compound-document entry names,
entry types and byte sizes, the item coordinates (c, z, t), the image
dimensions and pixel format, and tag *counts*. It never prints a tag value,
a name stored inside the document, or any pixel data. The output is meant to
be safe to paste into a bug report; read it before sending it.

Usage:
    python zviprobe.py <file.zvi> [more.zvi ...]

Requires Python 3 only -- no third-party modules (the CFB reader is built in).
"""

import struct
import sys

FREESECT = 0xFFFFFFFF
ENDOFCHAIN = 0xFFFFFFFE
FATSECT = 0xFFFFFFFD
DIFSECT = 0xFFFFFFFC
MAXREGSECT = 0xFFFFFFFA


class Entry:
    __slots__ = ("eid", "name", "type", "left", "right", "child",
                 "start", "size", "path")

    def __init__(self, eid, name, etype, left, right, child, start, size):
        self.eid = eid
        self.name = name
        self.type = etype
        self.left = left
        self.right = right
        self.child = child
        self.start = start
        self.size = size
        self.path = None


class CompoundFile:
    def __init__(self, path):
        # A ZVI slide can be several gigabytes; sectors are read on demand
        # rather than held in memory.
        self._file = open(path, "rb")
        self._file.seek(0, 2)
        self.file_size = self._file.tell()
        hdr = self._read(0, 512)
        if hdr[:8] != b"\xd0\xcf\x11\xe0\xa1\xb1\x1a\xe1":
            raise ValueError("not an OLE compound file (bad signature)")

        self.major = struct.unpack_from("<H", hdr, 0x1A)[0]
        self.sector_size = 1 << struct.unpack_from("<H", hdr, 0x1E)[0]
        self.mini_sector_size = 1 << struct.unpack_from("<H", hdr, 0x20)[0]
        self.num_fat_sectors = struct.unpack_from("<I", hdr, 0x2C)[0]
        self.first_dir_sector = struct.unpack_from("<I", hdr, 0x30)[0]
        self.mini_cutoff = struct.unpack_from("<I", hdr, 0x38)[0]
        self.first_minifat = struct.unpack_from("<I", hdr, 0x3C)[0]
        self.num_minifat_sectors = struct.unpack_from("<I", hdr, 0x40)[0]
        self.first_difat = struct.unpack_from("<I", hdr, 0x44)[0]
        self.num_difat_sectors = struct.unpack_from("<I", hdr, 0x48)[0]
        self.num_sectors = max(0, self.file_size // self.sector_size - 1)
        self._mini_cache = None
        self.header = hdr

        self._read_difat(hdr)
        self._read_fat()
        self._read_directory()
        self._read_minifat()

    def close(self):
        self._file.close()

    # -- sector plumbing ---------------------------------------------------
    def _read(self, offset, size):
        if offset < 0 or offset >= self.file_size:
            return b""
        self._file.seek(offset)
        return self._file.read(size)

    def sector_offset(self, sect):
        # The header occupies the whole first sector, whatever the sector size.
        return (sect + 1) * self.sector_size

    def read_sector(self, sect):
        return self._read(self.sector_offset(sect), self.sector_size)

    def _read_difat(self, hdr):
        self.difat = list(struct.unpack_from("<109I", hdr, 0x4C))
        sect = self.first_difat
        seen = set()
        per = self.sector_size // 4
        while sect not in (ENDOFCHAIN, FREESECT) and sect <= MAXREGSECT:
            if sect in seen:
                break
            seen.add(sect)
            buf = self.read_sector(sect)
            if len(buf) < self.sector_size:
                break
            vals = struct.unpack_from("<%dI" % per, buf, 0)
            self.difat.extend(vals[:-1])
            sect = vals[-1]
        self.difat = [s for s in self.difat if s <= MAXREGSECT]

    # -- FAT sector list, two ways -----------------------------------------
    # The header names the first 109 FAT sectors. Beyond that they are listed
    # in DIFAT sectors, each of which holds (sector_size/4 - 1) FAT sector
    # numbers followed, in its last 4 bytes, by the number of the next DIFAT
    # sector. The two readings below differ only once a file needs a second
    # DIFAT sector, which is what makes that the interesting threshold.
    def _header_difat(self):
        entries = []
        for i in range(min(109, self.num_fat_sectors)):
            entries.append(struct.unpack_from("<I", self.header, 0x4C + i * 4)[0])
        return entries

    def fat_sectors_correct(self):
        """Following the DIFAT chain, per MS-CFB."""
        entries = self._header_difat()
        sect = self.first_difat
        seen = set()
        per = self.sector_size // 4
        while (len(entries) < self.num_fat_sectors and sect <= MAXREGSECT
               and sect not in seen):
            seen.add(sect)
            buf = self.read_sector(sect)
            if len(buf) < self.sector_size:
                break
            vals = struct.unpack_from("<%dI" % per, buf, 0)
            for value in vals[:-1]:          # the last slot is the chain pointer
                if len(entries) >= self.num_fat_sectors:
                    break
                entries.append(value)
            sect = vals[-1]
        return entries

    def fat_sectors_as_pole_reads(self):
        """As POLE's StorageIO::load() builds the list: every slot of a DIFAT
        sector taken as a FAT sector number, and the DIFAT sectors assumed to
        be consecutive rather than chained."""
        entries = self._header_difat()
        if self.num_fat_sectors > 109 and self.num_difat_sectors > 0:
            per = self.sector_size // 4
            for r in range(self.num_difat_sectors):
                buf = self.read_sector(self.first_difat + r)
                if len(buf) < self.sector_size:
                    break
                vals = struct.unpack_from("<%dI" % per, buf, 0)
                for value in vals:           # no slot withheld for the chain
                    if len(entries) >= self.num_fat_sectors:
                        break
                    entries.append(value)
        return entries

    def first_fat_divergence(self):
        """Index of the first FAT sector the two readings disagree on, and the
        byte offset into the file from which streams become unreliable. Returns
        (None, None) when they agree."""
        correct = self.fat_sectors_correct()
        pole = self.fat_sectors_as_pole_reads()
        for i in range(min(len(correct), len(pole))):
            if correct[i] != pole[i]:
                entries_per_fat_sector = self.sector_size // 4
                covered = i * entries_per_fat_sector * self.sector_size
                return i, covered
        if len(correct) != len(pole):
            return min(len(correct), len(pole)), None
        return None, None

    def _read_fat(self):
        self.fat = []
        per = self.sector_size // 4
        for sect in self.difat:
            buf = self.read_sector(sect)
            if len(buf) < self.sector_size:
                break
            self.fat.extend(struct.unpack_from("<%dI" % per, buf, 0))

    def chain(self, start, limit=None):
        out = []
        sect = start
        seen = set()
        while sect <= MAXREGSECT:
            if sect in seen:
                out.append(("LOOP", sect))
                break
            seen.add(sect)
            out.append(sect)
            if sect >= len(self.fat):
                out.append(("FAT-OOB", sect))
                break
            sect = self.fat[sect]
            if limit and len(out) > limit:
                out.append("TRUNCATED")
                break
        return out

    def _read_minifat(self):
        self.minifat = []
        per = self.sector_size // 4
        for sect in self.chain(self.first_minifat):
            if not isinstance(sect, int):
                break
            buf = self.read_sector(sect)
            if len(buf) < self.sector_size:
                break
            self.minifat.extend(struct.unpack_from("<%dI" % per, buf, 0))

    # -- directory ---------------------------------------------------------
    def _read_directory(self):
        raw = b""
        for sect in self.chain(self.first_dir_sector):
            if not isinstance(sect, int):
                break
            raw += self.read_sector(sect)
        self.entries = []
        for i in range(len(raw) // 128):
            b = raw[i * 128:(i + 1) * 128]
            nlen = struct.unpack_from("<H", b, 64)[0]
            nlen = max(0, min(64, nlen))
            name = b[:nlen].decode("utf-16-le", "replace").rstrip("\x00")
            etype = b[66]
            left, right, child = struct.unpack_from("<III", b, 68)
            start = struct.unpack_from("<I", b, 116)[0]
            size = struct.unpack_from("<Q", b, 120)[0]
            if self.major < 4:
                size &= 0xFFFFFFFF
            self.entries.append(
                Entry(i, name, etype, left, right, child, start, size))
        self._assign_paths()

    def _assign_paths(self):
        self.orphans = set()
        if not self.entries:
            return
        root = self.entries[0]
        root.path = "/"
        self.orphans = set(e.eid for e in self.entries
                           if e.type in (1, 2)) - {0}

        def walk_siblings(eid, prefix):
            stack = [eid]
            seen = set()
            while stack:
                cur = stack.pop()
                if cur > MAXREGSECT or cur >= len(self.entries) or cur in seen:
                    continue
                seen.add(cur)
                e = self.entries[cur]
                e.path = (prefix.rstrip("/") + "/" + e.name) or e.name
                self.orphans.discard(cur)
                stack.append(e.left)
                stack.append(e.right)
                if e.type == 1 and e.child <= MAXREGSECT:
                    walk_siblings(e.child, e.path)

        if root.child <= MAXREGSECT:
            walk_siblings(root.child, "")

    def _mini_container(self):
        if self._mini_cache is None:
            root = self.entries[0]
            parts = []
            for sect in self.chain(root.start):
                if not isinstance(sect, int):
                    break
                parts.append(self.read_sector(sect))
            self._mini_cache = b"".join(parts)
        return self._mini_cache

    def read_stream(self, entry, limit=None):
        """Stream bytes. `limit` caps the read: an item raster is megabytes and
        only its few hundred header bytes are ever parsed."""
        want = entry.size if limit is None else min(entry.size, limit)
        if entry.size >= self.mini_cutoff or entry.eid == 0:
            out = b""
            # Only walk as far down the chain as `want` needs: an item raster
            # is hundreds of sectors long and its header is one.
            sectors = -(-want // self.sector_size) or 1
            for sect in self.chain(entry.start, limit=sectors):
                if not isinstance(sect, int) or len(out) >= want:
                    break
                out += self.read_sector(sect)
            return out[:want]
        # Mini stream: lives inside the root entry's stream. Every small stream
        # in the document shares it, so it is read once.
        mini_container = self._mini_container()
        out = b""
        sect = entry.start
        seen = set()
        while sect <= MAXREGSECT and sect not in seen:
            seen.add(sect)
            off = sect * self.mini_sector_size
            out += mini_container[off:off + self.mini_sector_size]
            if sect >= len(self.minifat):
                break
            sect = self.minifat[sect]
        return out[:want]

    def by_path(self):
        return {e.path: e for e in self.entries if e.path}


# -- ZVI typed-item reader (mirrors ZVIUtils) ------------------------------
FIXED = {0x00: 0, 0x01: 0, 0x10: 1, 0x11: 1, 0x02: 2, 0x12: 2, 0x0B: 2,
         0x03: 4, 0x13: 4, 0x16: 4, 0x17: 4, 0x04: 4, 0x0A: 4,
         0x14: 8, 0x15: 8, 0x05: 8, 0x07: 8, 0x06: 8,
         0x09: 16, 0x0D: 16, 0x0E: 16}
LEN32 = {0x08, 0x41, 0x45, 0x2000, 0x3F}
LEN16 = {0x42}


class Cursor:
    def __init__(self, buf):
        self.buf = buf
        self.pos = 0

    def u16(self):
        v = struct.unpack_from("<H", self.buf, self.pos)[0]
        self.pos += 2
        return v

    def i32(self):
        v = struct.unpack_from("<i", self.buf, self.pos)[0]
        self.pos += 4
        return v

    def u32(self):
        v = struct.unpack_from("<I", self.buf, self.pos)[0]
        self.pos += 4
        return v

    def skip_item(self):
        """Step over one typed item. Returns (type, payload_size)."""
        start = self.pos
        t = self.u16()
        if t in FIXED:
            n = FIXED[t]
        elif t in LEN32:
            n = self.u32()
        elif t in LEN16:
            n = self.u16()
        else:
            raise ValueError("unsupported item type %d (0x%04X) at offset %d"
                             % (t, t, start))
        self.pos += n
        if self.pos > len(self.buf):
            raise ValueError("item type %d at offset %d overruns stream "
                             "(needs %d bytes, %d left)"
                             % (t, start, n, len(self.buf) - start - 2))
        return t, n

    def int_item(self):
        t = self.u16()
        if t not in (0x03, 0x16):
            raise ValueError("expected integer item, got type %d (0x%04X) "
                             "at offset %d" % (t, t, self.pos - 2))
        return self.i32()


PIXEL_FORMATS = {1: "PF_BGR", 2: "PF_BGRA", 3: "PF_UINT8", 4: "PF_INT16",
                 5: "PF_INT32", 6: "PF_FLOAT", 7: "PF_DOUBLE",
                 8: "PF_BGR16", 9: "PF_BGR32"}


def parse_image_contents(buf):
    """Mirror ZVIScene::parseImageInfo()."""
    c = Cursor(buf)
    skipped = []
    for i in range(4):
        skipped.append(c.skip_item())
    width = c.int_item()
    height = c.int_item()
    depth_item = c.skip_item()
    pixel_format = c.int_item()
    raw_count = c.int_item()
    return {"leading_items": skipped, "width": width, "height": height,
            "depth_item": depth_item, "pixel_format": pixel_format,
            "raw_count": raw_count, "bytes_consumed": c.pos,
            "stream_size": len(buf)}


def parse_item_contents(buf, total_size=None):
    """Mirror ZVIImageItem::readContents(), reporting where it would fail.

    `buf` may be a prefix of the stream; `total_size` is the real stream size,
    used only to report how many raster bytes follow the header.
    """
    if total_size is None:
        total_size = len(buf)
    c = Cursor(buf)
    for i in range(11):
        c.skip_item()
    pos_type = c.u16()
    pos_size = c.u32()
    if pos_size < 28:
        raise ValueError("position blob is %d bytes, need >= 28" % pos_size)
    pos = struct.unpack_from("<7I", buf, c.pos)
    c.pos += pos_size
    for i in range(5):
        c.skip_item()
    hdr = struct.unpack_from("<7i", buf, c.pos)
    c.pos += 28
    return {"pos_type": pos_type, "pos_size": pos_size,
            "z": pos[2], "c": pos[3], "t": pos[4],
            "scene": pos[5], "position": pos[6],
            "version": hdr[0], "width": hdr[1], "height": hdr[2],
            "depth": hdr[3], "pixel_format": hdr[5], "valid_bits": hdr[6],
            "data_offset": c.pos, "stream_size": total_size,
            "data_bytes": total_size - c.pos}


def parse_tag_header(buf):
    c = Cursor(buf)
    version = c.int_item()
    count = c.int_item()
    return version, count


def count_readable_tags(buf):
    """How many (value, id, attribute) triples the stream really yields."""
    c = Cursor(buf)
    c.int_item()
    declared = c.int_item()
    n = 0
    err = None
    while c.pos + 2 <= len(buf):
        try:
            c.skip_item()      # value
            c.int_item()       # tag id
            c.skip_item()      # attribute
        except Exception as e:
            err = "%s (at offset %d)" % (e, c.pos)
            break
        n += 1
    return declared, n, err, c.pos


def item_index(name):
    if not name.upper().startswith("ITEM(") or not name.endswith(")"):
        return None
    try:
        return int(name[5:-1])
    except ValueError:
        return None


def main(path):
    cf = CompoundFile(path)
    paths = cf.by_path()

    print("=" * 72)
    print("FILE:", path)
    print("CFB  : v%d, sector=%d, mini-sector=%d, cutoff=%d, "
          "sectors-in-file=%d" % (cf.major, cf.sector_size,
                                  cf.mini_sector_size, cf.mini_cutoff,
                                  cf.num_sectors))
    print("FAT  : %d FAT sectors, %d DIFAT sectors"
          % (cf.num_fat_sectors, cf.num_difat_sectors))

    divergence, covered = cf.first_fat_divergence()
    if divergence is not None:
        print()
        print("!! This file needs %d DIFAT sectors, and slideio's compound-file"
              % cf.num_difat_sectors)
        print("   library (POLE) reads them incorrectly. It takes the last 4 bytes")
        print("   of each DIFAT sector -- the pointer to the next one -- as if they")
        print("   were a FAT entry, and assumes the DIFAT sectors are consecutive.")
        print("   Its sector table first disagrees with the real one at FAT sector"
              " %d," % divergence)
        if covered is not None:
            print("   so data stored past roughly %.1f MB into the file reads as"
                  % (covered / 1048576.0))
            print("   truncated or goes missing.")
        print("   If this file fails to open, this is almost certainly why.")
        print("   A file needs a second DIFAT sector at about 15 MB with 512-byte")
        print("   sectors; with 4096-byte sectors the limit is far beyond any slide.")
    elif cf.num_difat_sectors:
        print("       (DIFAT within the range POLE reads correctly)")
    print("DIR  : %d entries (%d storages, %d streams)"
          % (len(cf.entries),
             sum(1 for e in cf.entries if e.type == 1),
             sum(1 for e in cf.entries if e.type == 2)))
    if cf.orphans:
        print("WARN : %d directory entries not reachable from the root tree: %s"
              % (len(cf.orphans),
                 sorted(cf.orphans)[:20]))
    print()

    # --- /Image/Contents --------------------------------------------------
    img = paths.get("/Image/Contents")
    raw_count = None
    if img is None:
        print("!! /Image/Contents is MISSING -- the driver cannot open this file")
    else:
        buf = cf.read_stream(img)
        print("/Image/Contents: %d bytes" % len(buf))
        try:
            info = parse_image_contents(buf)
            raw_count = info["raw_count"]
            print("  width        : %d" % info["width"])
            print("  height       : %d" % info["height"])
            print("  pixel format : %d (%s)"
                  % (info["pixel_format"],
                     PIXEL_FORMATS.get(info["pixel_format"], "UNKNOWN")))
            print("  RawCount     : %d   <-- items the driver will try to read"
                  % raw_count)
            print("  header consumed %d of %d bytes"
                  % (info["bytes_consumed"], info["stream_size"]))
        except Exception as e:
            print("  !! parse failed: %s" % e)

    # --- /Image/Tags/Contents --------------------------------------------
    t = paths.get("/Image/Tags/Contents")
    if t is None:
        print("\n/Image/Tags/Contents: MISSING")
    else:
        buf = cf.read_stream(t)
        try:
            declared, actual, err, consumed = count_readable_tags(buf)
            print("\n/Image/Tags/Contents: %d bytes, declared %d tags, "
                  "readable %d, consumed %d" % (len(buf), declared, actual,
                                                consumed))
            if err:
                print("  !! stops early: %s" % err)
            elif consumed < len(buf):
                print("  note: %d trailing bytes" % (len(buf) - consumed))
        except Exception as e:
            print("\n/Image/Tags/Contents: !! %s" % e)

    # --- items ------------------------------------------------------------
    present = {}
    for e in cf.entries:
        if e.type != 1 or not e.path:
            continue
        if not e.path.startswith("/Image/Item("):
            continue
        if e.path.count("/") != 2:
            continue
        idx = item_index(e.name)
        if idx is not None:
            present[idx] = e.path

    print("\nItem storages under /Image : %d found" % len(present))
    if present:
        keys = sorted(present)
        print("  index range  : %d .. %d" % (keys[0], keys[-1]))
        expected = set(range(keys[0], keys[-1] + 1))
        gaps = sorted(expected - set(keys))
        if gaps:
            print("  !! GAPS      : %s" % gaps)
        else:
            print("  contiguous   : yes")
        if raw_count is not None:
            missing = [i for i in range(raw_count) if i not in present]
            print("  RawCount=%d -> missing item storages: %s"
                  % (raw_count, missing if missing else "none"))
            extra = [i for i in keys if i >= raw_count]
            if extra:
                print("  items beyond RawCount (driver ignores them): %s" % extra)

    bad = []
    print("\nPer-item streams (%s):" % ("all" if len(present) <= 40
                                        else "first 40 + all broken"))
    for n, idx in enumerate(sorted(present)):
        base = present[idx]
        cont = paths.get(base + "/Contents")
        tags = paths.get(base + "/Tags/Contents")
        problems = []
        line = "  Item(%-4d) Contents=" % idx
        if cont is None:
            line += "MISSING".ljust(12)
            problems.append("no Contents stream")
        else:
            line += ("%d B" % cont.size).ljust(12)
        if tags is None:
            line += " Tags=MISSING"
            problems.append("no Tags/Contents stream")
        else:
            line += " Tags=%d B" % tags.size

        detail = ""
        if cont is not None:
            try:
                # 64 KiB covers the typed-item header by a wide margin; the
                # raster that follows it is never parsed.
                info = parse_item_contents(cf.read_stream(cont, 65536), cont.size)
                detail = ("  c=%d z=%d t=%d  %dx%d fmt=%d validbits=%d "
                          "data=%d B" % (info["c"], info["z"], info["t"],
                                         info["width"], info["height"],
                                         info["pixel_format"],
                                         info["valid_bits"],
                                         info["data_bytes"]))
            except Exception as e:
                detail = "  !! %s" % e
                problems.append(str(e))
        if tags is not None:
            try:
                declared, actual, err, _ = count_readable_tags(
                    cf.read_stream(tags))
                detail += "  tags %d/%d" % (actual, declared)
                if err:
                    detail += " !! %s" % err
                    problems.append("tags: %s" % err)
            except Exception as e:
                detail += "  !! tags %s" % e
                problems.append("tags: %s" % e)

        if problems:
            bad.append((idx, problems))
        if n < 40 or problems:
            print(line + detail)

    print("\nSUMMARY")
    if raw_count is not None:
        missing = [i for i in range(raw_count) if i not in present]
        if missing:
            print("  RawCount declares %d items but %d storages are absent: %s"
                  % (raw_count, len(missing), missing))
    if bad:
        print("  %d item(s) with problems: %s"
              % (len(bad), [i for i, _ in bad]))
    else:
        print("  no per-item problems found")


if __name__ == "__main__":
    if len(sys.argv) < 2:
        print(__doc__)
        print("usage: python zviprobe.py <file.zvi> [more.zvi ...]")
        sys.exit(2)
    for p in sys.argv[1:]:
        try:
            main(p)
        except Exception as exc:
            print("%s: FAILED: %s" % (p, exc))
        print()
