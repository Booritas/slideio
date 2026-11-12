"""Turn the ND2 validation's Bio-Formats region hashes into the corpus test references.

Usage (an environment with `nd2`, `imagecodecs` and `numpy`, see the validation README):
    python -I auxfiles/nd2-fixtures/make_region_reference.py <nd2-dir> <validation-dir> <out-regions.tsv>

<nd2-dir> is $SLIDEIO_IMAGES_PATH/nd2 (or E:/Archive/slideio/images-extra/nd2, the same
bytes). <validation-dir> is image_formats/nd2/validation of the documents repository:
bioformats-regions.tsv, bioformats-relocated.tsv, nd2spec.py and inventory/*.json.

For every Bio-Formats row this repeats the read of that folder's comparison scripts and
requires the SHA-256 of the bytes to equal Bio-Formats' before writing anything:

- the plane number is decomposed with the Bio-Formats dimension order of the row (RGB
  samples are not planes), giving (Z, C, T); series = the XY position;
- chunked files (compare_regions.py): the frame index is row-major over nd2spec's loops,
  stale chunk-map entries are relocated by nd2spec.verify_frames, and the region of
  component C is addressed with uiWidthBytes;
- legacy JPEG 2000 files (compare_nd2lib.py): the frame comes from nd2's raw frame API
  over nd2's loops with the TINF XY count, and the Bio-Formats bytes are rebuilt from it
  big-endian and, for RGB, channel-planar -- Bio-Formats' two conventions.

zenodo/bioformats-issue/2D 500uM VPA002.nd2 (4 channels x RGB, uiComp 12) is the one
file Bio-Formats reads wrongly (FD 6.2: plane c, pixel (x, y) at byte y*4W + 4x + c).
For it the script requires Bio-Formats' bytes to equal exactly that misaddressing of
nd2spec's frame -- which pins the region to the right frame -- and takes the reference
for the true layout from where nd2spec and nd2 agree: Bio-Formats channel c is ND2
channel c, components 3c..3c+2, each read by both and required to be equal.

jonas/divisionByZero/290110.tranMgc005cr.nd2 (uiWidth 0) is skipped: the driver refuses it.

Then one row is written per region and slideio channel (= component, storage order):
file, series, z, c, t, rx, ry, rw, rh and the zlib CRC-32 of the region's samples, rows
top to bottom, little-endian. Any other mismatch exits non-zero and writes nothing.

Beside <out-regions.tsv> it also writes
- nd2-loops.tsv: each chunked file's flattened loops from inventory/*.json, outermost
  first, as `T<n> P<n> Z<n>` (eType 1/8 -> T, 2 (`M`) -> P, 4 -> Z; a loop of another
  type with count <= 1 is dropped, as the driver drops it);
- nd2-times.tsv: nd2's frame times (relativeTimeMs, absoluteJulianDayNumber of the
  first channel) for every frame present inside the loops of the two time-lapse files in
  TIME_FILES, with the frame decomposed into (series, t, z) over nd2spec's loops; with T
  the outermost loop, frames past the declared product count as further time points.
  `source` is `nd2`, or `prefix` for a frame past uiSequenceCount, which nd2 does not serve:
  its time is the frame's 8-byte prefix and its absolute time is taken from nd2's frame 0.

Output is deterministic: rows in input order, LF line endings, repr() for floats.
"""
import collections
import csv
import hashlib
import itertools
import json
import pathlib
import struct
import sys
import zlib

import nd2
import numpy as np

VPA002 = 'zenodo/bioformats-issue/2D 500uM VPA002.nd2'
REFUSED = 'jonas/divisionByZero/290110.tranMgc005cr.nd2'
TIME_FILES = ['zenodo/bioformats/190821_L1_IRS-Akt_CS2003.nd2', 'zenodo-16921650/B08.nd2']
REGION_TSVS = ['bioformats-regions.tsv', 'bioformats-relocated.tsv']
AXES = {'TimeLoop': 'T', 'NETimeLoop': 'T', 'XYPosLoop': 'M', 'ZStackLoop': 'Z'}
INVENTORY_AXES = {1: 'T', 8: 'T', 2: 'P', 4: 'Z'}


def plane_coords(row):
    """(Z, C, T) of the row's plane from the Bio-Formats dimension order."""
    n = int(row['plane'])
    coords = {}
    for axis in row['order'][2:]:
        size = int(row[axis.lower()])
        if axis == 'C':
            size //= int(row['rgb'])  # RGB samples are not planes
        coords[axis] = n % size
        n //= size
    return coords


def frame_index(loops, want):
    index = 0
    for axis, count in loops:
        index = index * count + want.get(axis, 0)
    return index


class ChunkedFrames:
    """Rows of uncompressed frames are read in place; zlib frames are inflated whole."""

    def __init__(self, nd2spec, f):
        self.spec, self.f = nd2spec, f
        self.info = nd2spec.describe_chunked(f)
        self.layout = nd2spec.frame_layout(self.info)
        checked, _ = nd2spec.verify_frames(f, self.info['frames'])
        self.frames = dict(checked)
        self.loops = [(l['axis'], l['count']) for l in self.info['loops']]
        self.cache = {}

    def rows(self, index, ry, rh):
        """Bytes of frame rows [ry, ry + rh) concatenated, uiWidthBytes each."""
        lay = self.layout
        offset = self.frames[index]
        if lay['compression'] == 0:
            if index not in self.cache:
                self.cache = {index: self.spec.read_frame_bytes(self.f, offset, lay)[1]}
            return self.cache[index][ry * lay['row_bytes']:(ry + rh) * lay['row_bytes']]
        name_length, data_length, _ = self.spec.read_chunk_header(self.f, offset)
        pixels = offset + self.spec.CHUNK_HEADER.size + name_length + 8
        assert 8 + lay['height'] * lay['row_bytes'] <= data_length, (index, data_length)
        self.f.seek(pixels + ry * lay['row_bytes'])
        data = self.f.read(rh * lay['row_bytes'])
        assert len(data) == rh * lay['row_bytes']
        return data

    def region(self, index, k, rx, ry, rw, rh):
        """Component k of the region, little-endian, row-major (nd2spec.extract_region)."""
        lay = dict(self.layout, height=rh)
        return self.spec.extract_region(self.rows(index, ry, rh), lay, k, rx, 0, rw, rh)


def nd2_frame(f, index, cache):
    if index not in cache:
        cache.clear()
        raw = np.asarray(f._rdr.read_frame(index))
        cache[index] = raw.reshape(raw.shape[0], raw.shape[1], -1)
    return cache[index]


def nd2_index_loops(f):
    """nd2's loops; for legacy files with the TINF XY count (FD 11.3, compare_nd2lib.py)."""
    loops = [(AXES.get(l.type, l.type), l.count) for l in f.experiment]
    ddim = getattr(f._rdr, 'ddim', {}) or {}
    if f.is_legacy and ddim.get('P'):
        loops = [(a, ddim['P'] if a == 'M' else c) for a, c in loops]
    return loops


def crc(data):
    return f'{zlib.crc32(data) & 0xffffffff:08x}'


def main():
    if len(sys.argv) != 4:
        raise SystemExit(__doc__)
    root = pathlib.Path(sys.argv[1])
    base = pathlib.Path(sys.argv[2])
    out_path = pathlib.Path(sys.argv[3])
    sys.path.insert(0, str(base))
    import nd2spec

    rows = []
    for tsv in REGION_TSVS:
        with (base / tsv).open(encoding='utf8', newline='') as f:
            rows += list(csv.DictReader(f, delimiter='\t'))

    out_rows = []
    counts = collections.Counter()
    for rel, group in itertools.groupby(rows, key=lambda r: r['file']):
        group = list(group)
        if group[0]['series'] == 'ERROR':
            raise SystemExit(f'Bio-Formats could not open {rel}')
        if rel == REFUSED:
            counts['refused_rows'] += len(group)
            continue
        path = root / rel
        with open(path, 'rb') as src:
            kind = nd2spec.detect(src)
            chunked = ChunkedFrames(nd2spec, src) if kind == 'chunked' else None
            lib = nd2.ND2File(path) if (kind != 'chunked' or rel == VPA002) else None
            try:
                lib_loops = nd2_index_loops(lib) if lib is not None else None
                cache = {}
                for row in group:
                    s = int(row['series'])
                    coords = plane_coords(row)
                    want = dict(T=coords['T'], Z=coords['Z'], M=s)
                    rx, ry, rw, rh = (int(row[k]) for k in ('rx', 'ry', 'rw', 'rh'))
                    rgb = int(row['rgb'])
                    emit = []  # (slideio channel, little-endian bytes)
                    if chunked is None:
                        # Legacy: rebuild Bio-Formats' big-endian, planar bytes from nd2's frame.
                        frame = nd2_frame(lib, frame_index(lib_loops, want), cache)
                        comps = list(range(rgb)) if rgb > 1 else [coords['C']]
                        planes = [frame[ry:ry + rh, rx:rx + rw, k] for k in comps]
                        bf = b''.join(p.astype(p.dtype.newbyteorder('>')).tobytes() for p in planes)
                        emit = [(k, p.astype(p.dtype.newbyteorder('<')).tobytes()) for k, p in zip(comps, planes)]
                    else:
                        assert rgb == 1, (rel, 'chunked RGB planes are not expected')
                        index = frame_index(chunked.loops, want)
                        if index not in chunked.frames:
                            raise SystemExit(f'{rel}: frame {index} of series {s} plane {row["plane"]} is absent')
                        if rel == VPA002:
                            lay = chunked.layout
                            c, w = coords['C'], lay['width']
                            assert int(row['c']) * 3 == lay['components'] == 12
                            data = chunked.rows(index, 0, lay['height'])
                            bf = bytes(data[y * 4 * w + 4 * x + c] for y in range(ry, ry + rh)
                                       for x in range(rx, rx + rw))
                            frame = nd2_frame(lib, index, cache)
                            for k in range(3 * c, 3 * c + 3):
                                ours = chunked.region(index, k, rx, ry, rw, rh)
                                theirs = frame[ry:ry + rh, rx:rx + rw, k].astype('<u1').tobytes()
                                if ours != theirs:
                                    counts['mismatch'] += 1
                                    print(f'MISMATCH nd2spec/nd2 {rel} component {k} at ({rx},{ry})', file=sys.stderr)
                                    continue
                                emit.append((k, ours))
                            counts['vpa002'] += 1
                        else:
                            bf = chunked.region(index, coords['C'], rx, ry, rw, rh)
                            emit = [(coords['C'], bf)]
                    if hashlib.sha256(bf).hexdigest() != row['sha256']:
                        counts['mismatch'] += 1
                        print(f"MISMATCH {rel} series {s} plane {row['plane']} {coords} at ({rx},{ry})",
                              file=sys.stderr)
                        continue
                    counts['regions'] += 1
                    for channel, data in emit:
                        out_rows.append([rel, s, coords['Z'], channel, coords['T'], rx, ry, rw, rh, crc(data)])
            finally:
                if lib is not None:
                    lib.close()
        print(rel, len(group), file=sys.stderr)

    print(f"{counts['regions']} regions, {counts['mismatch']} unexpected mismatches, "
          f"{counts['vpa002']} VPA002 regions referenced from nd2 ({len(out_rows)} reference rows; "
          f"{counts['refused_rows']} rows of the refused file skipped)")
    if counts['mismatch']:
        raise SystemExit(1)

    with out_path.open('w', encoding='utf8', newline='\n') as f:
        f.write('file\tseries\tz\tc\tt\trx\try\trw\trh\tcrc32\n')
        for r in out_rows:
            f.write('\t'.join(str(v) for v in r) + '\n')

    # Loops of every chunked file, from the inventory.
    loop_rows = []
    for inv in sorted((base / 'inventory').glob('*.json')):
        data = json.loads(inv.read_text(encoding='utf8'))
        if not isinstance(data, dict) or data.get('variant') != 'chunked':
            continue
        loops = [f"{INVENTORY_AXES.get(l['eType'], 'E%d' % l['eType'])}{l['count']}"
                 for l in data.get('loops') or [] if l['eType'] in INVENTORY_AXES or l['count'] > 1]
        loop_rows.append((data['file'], ' '.join(loops)))
    loop_rows.sort()
    with (out_path.parent / 'nd2-loops.tsv').open('w', encoding='utf8', newline='\n') as f:
        f.write('file\tloops\n')
        for r in loop_rows:
            f.write('\t'.join(r) + '\n')
    print(f'{len(loop_rows)} chunked files in nd2-loops.tsv')

    # Frame times of the two time-lapse files. nd2 serves frame metadata only up to
    # uiSequenceCount; a frame past it gets its time from its own 8-byte prefix (FD 6), and
    # its absolute time from nd2's frame 0 (dTimeAbsolute = absolute - relative of frame 0).
    time_rows = []
    for rel in TIME_FILES:
        with open(root / rel, 'rb') as src:
            info = nd2spec.describe_chunked(src)
            offsets = dict(info['frames'])
            loops = [(l['axis'], l['count']) for l in info['loops']]
            product = 1
            for _, count in loops:
                product *= count
            # With T the outermost loop, frames past the declared product continue T (FD 7.4,
            # design 4.5); otherwise they lie outside the loops and are left out.
            t_outer = bool(loops) and loops[0][0] == 'T'
            present = sorted(i for i in offsets if t_outer or i < product)
            with nd2.ND2File(root / rel) as lib:
                served = lib.attributes.sequenceCount
                first = lib.frame_metadata(0).channels[0].time
                start_jdn = first.absoluteJulianDayNumber - first.relativeTimeMs / 86400000.
                for index in present:
                    coords, n = {}, index
                    for k, (axis, count) in enumerate(reversed(loops)):
                        outermost = k == len(loops) - 1
                        coords[axis] = n if outermost else n % count
                        n //= count
                    if index < served:
                        time = lib.frame_metadata(index).channels[0].time
                        ms, jdn, source = time.relativeTimeMs, time.absoluteJulianDayNumber, 'nd2'
                    else:
                        name_length, _, _ = nd2spec.read_chunk_header(src, offsets[index])
                        ms = struct.unpack('<d', src.read(8))[0]
                        jdn, source = start_jdn + ms / 86400000., 'prefix'
                    time_rows.append([rel, coords.get('M', 0), coords.get('T', 0), coords.get('Z', 0), index,
                                      repr(float(ms)), repr(float(jdn)), source])
    with (out_path.parent / 'nd2-times.tsv').open('w', encoding='utf8', newline='\n') as f:
        f.write('file\tseries\tt\tz\tframe\trelativeMs\tabsoluteJdn\tsource\n')
        for r in time_rows:
            f.write('\t'.join(str(v) for v in r) + '\n')
    print(f'{len(time_rows)} frame times in nd2-times.tsv')


if __name__ == '__main__':
    main()
