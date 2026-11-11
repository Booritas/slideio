"""Turn the LIF validation's Bio-Formats region hashes into the corpus test reference.

Usage:
    py -3 auxfiles/lif-fixtures/make_region_reference.py <validation-dir> <out.tsv>

<validation-dir> is image_formats/lif/validation of the documents repository: the
*.lif.json inventories and bioformats-, zenodo- and figshare-regions.tsv. For every row
this repeats the read of that folder's compare_regions.py -- same inventory, same
memory-ID deduplication, same tile expansion, same plane decomposition from the
Bio-Formats dimension order, same byte-increment address -- and requires the SHA-256 of
the bytes to equal Bio-Formats'. Only then does it write the row, with the decomposed
z, c, t and the zlib CRC-32 of the same bytes (rows top to bottom, samples
little-endian, as stored). Any mismatch exits non-zero and writes nothing.

The source files are read from each inventory's `path`; nothing is written there.
Stdlib only. Output is deterministic: rows in input order, LF line endings.
"""
import collections
import csv
import hashlib
import json
import pathlib
import sys
import zlib

REGION_TSVS = ['bioformats-regions.tsv', 'zenodo-regions.tsv', 'figshare-regions.tsv']

# Validation basename -> path relative to $SLIDEIO_IMAGES_PATH/lif/.
CORPUS = {
    'AMR1.lif': 'bioformats/imagesc-110520/AMR1.lif',
    '20191025 Test FRET 585. 423, 426.lif': 'bioformats/imagesc-30856/20191025 Test FRET 585. 423, 426.lif',
    'PR2729_frameOrderCombinedScanTypes.lif': 'bioformats/michael/PR2729_frameOrderCombinedScanTypes.lif',
    '150519_FRAP_test_ROIs_chromagreen.lif':
        'bioformats/seanwarren/150519_FRAP_test_ROIs_chromagreen/150519_FRAP_test_ROIs_chromagreen.lif',
    'Project007.lif': 'bioformats/zenodo-6606445/Project007.lif',
    'Intro.lif': 'zenodo/intro/Intro.lif',
    'IT4_27_02_15.lif': 'figshare/IT4_27_02_15.lif',
}


def expand_series(data):
    """compare_regions.py's series list: (image, field) per Bio-Formats series."""
    blocks = {b['id']: b for b in data['blocks'] if b['length']}
    images, seen = [], set()
    for im in data['images']:
        key = im['memory']['MemoryBlockID'] if im['memory'] else None
        if key not in blocks or key in seen:
            continue
        seen.add(key)
        images.append(im)
    series = []
    for im in images:
        dims = {int(d['DimID']): d for d in im['dims']}
        tiles = int(dims.get(10, {}).get('NumberOfElements', 1))
        series.extend((im, j) for j in range(tiles))
    return blocks, series


def main():
    if len(sys.argv) != 3:
        raise SystemExit(__doc__)
    base = pathlib.Path(sys.argv[1])
    out_path = pathlib.Path(sys.argv[2])
    out_rows = []
    counts = collections.Counter()
    for tsv in REGION_TSVS:
        with (base / tsv).open(encoding='utf8', newline='') as f:
            rows = list(csv.DictReader(f, delimiter='\t'))
        cache = {}
        for row in rows:
            name = row['file']
            if name not in cache:
                data = json.loads((base / (name + '.json')).read_text(encoding='utf8'))
                for entry in cache.values():
                    entry[-1].close()
                cache.clear()
                cache[name] = (data, *expand_series(data), open(data['path'], 'rb'))
            data, blocks, series, src = cache[name]
            s = int(row['series'])
            im, j = series[s]
            ds = {int(d['DimID']): d for d in im['dims']}
            cs = im['channels']
            block = blocks[im['memory']['MemoryBlockID']]
            W = int(ds[1]['NumberOfElements'])
            H = int(ds[2]['NumberOfElements'])
            b = int(ds[1]['BytesInc'])
            assert (W, H, len(cs)) == (int(row['x']), int(row['y']), int(row['c'])), row
            n = int(row['plane'])
            coords = {}
            for axis in row['order'][2:]:
                size = int(row[axis.lower()])
                coords[axis] = n % size
                n //= size
            offset = block['offset'] + int(cs[coords['C']]['BytesInc'])
            for axis, dim in [('Z', 3), ('T', 4)]:
                offset += coords[axis] * int(ds.get(dim, {}).get('BytesInc', 0))
            offset += j * int(ds.get(10, {}).get('BytesInc', 0))
            rx, ry, rw, rh = (int(row[k]) for k in ('rx', 'ry', 'rw', 'rh'))
            pixels = bytearray()
            for y in range(ry, ry + rh):
                pos = offset + y * int(ds[2]['BytesInc']) + rx * b
                length = rw * b
                assert block['offset'] <= pos and pos + length <= block['offset'] + block['length'], row
                src.seek(pos)
                chunk = src.read(length)
                assert len(chunk) == length, row
                pixels += chunk
            if hashlib.sha256(pixels).hexdigest() != row['sha256']:
                counts['mismatch'] += 1
                print(f"MISMATCH {name} series {s} plane {row['plane']} at ({rx},{ry})", file=sys.stderr)
                continue
            counts['match'] += 1
            out_rows.append([CORPUS[name], s, coords['Z'], coords['C'], coords['T'],
                             rx, ry, rw, rh, f'{zlib.crc32(pixels) & 0xffffffff:08x}'])
        for entry in cache.values():
            entry[-1].close()
    total = counts['match'] + counts['mismatch']
    print(f"{total} regions, {counts['mismatch']} mismatches")
    if counts['mismatch']:
        raise SystemExit(1)
    with out_path.open('w', encoding='utf8', newline='\n') as f:
        f.write('file\tseries\tz\tc\tt\trx\try\trw\trh\tcrc32\n')
        for r in out_rows:
            f.write('\t'.join(str(v) for v in r) + '\n')


if __name__ == '__main__':
    main()
