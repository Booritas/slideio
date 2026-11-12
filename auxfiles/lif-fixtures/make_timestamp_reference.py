"""Write liffile's plane timestamps of the LIF corpus as a test reference.

Usage:
    <python with liffile> auxfiles/lif-fixtures/make_timestamp_reference.py <lif-dir> <out.tsv>

<lif-dir> is $SLIDEIO_IMAGES_PATH/lif. Covers the FRAP time series and Intro.lif (its
tile scans among them), the two files the LIFCorpusOpen.Timestamps test checks.

Needs liffile (PyPI) and NumPy. Generated with liffile 2026.11.8 from PyPI, NumPy 2.5.4.
That is the version, not the pinned git revision, the LIF validation README names.

For every image whose `timestamps` holds exactly one entry per stored plane, one row
per plane: file (relative to <lif-dir>), liffile's image path, the plane's m, t, c, z,
and the time in Unix seconds as liffile gives it -- datetime64[ms], i.e. FILETIME
floored to a millisecond -- written with exactly three decimals. The k-th entry is
decomposed over liffile's own `dims` (the storage order, slowest first) without Y, X
and S; an axis the image lacks is 0. Images with a list of any other length are not
written: the driver reports no timestamps for them, and the test checks that too.
Deterministic: images in liffile order, planes in list order, LF line endings.
"""
import pathlib
import sys

import liffile

FILES = [
    'bioformats/seanwarren/150519_FRAP_test_ROIs_chromagreen/150519_FRAP_test_ROIs_chromagreen.lif',
    'zenodo/intro/Intro.lif',
]


def main():
    if len(sys.argv) != 3:
        raise SystemExit(__doc__)
    root = pathlib.Path(sys.argv[1])
    rows = []
    for rel in FILES:
        with liffile.LifFile(root / rel) as lif:
            for im in lif.images:
                ts = im.timestamps
                if ts is None:
                    continue
                axes = [a for a in im.dims if a not in ('Y', 'X', 'S')]
                sizes = [im.sizes[a] for a in axes]
                planes = 1
                for s in sizes:
                    planes *= s
                if len(ts) != planes:
                    print(f'{rel}: {im.path}: {len(ts)} timestamps for {planes} planes, not written')
                    continue
                for k, value in enumerate(ts.astype('int64')):
                    coords = {'M': 0, 'T': 0, 'C': 0, 'Z': 0}
                    n = k
                    for axis, size in reversed(list(zip(axes, sizes))):
                        coords[axis] = n % size
                        n //= size
                    ms = int(value)
                    rows.append([rel, im.path, coords['M'], coords['T'], coords['C'], coords['Z'],
                                 f'{ms // 1000}.{ms % 1000:03d}'])
    with pathlib.Path(sys.argv[2]).open('w', encoding='utf8', newline='\n') as f:
        f.write('file\timage\tm\tt\tc\tz\tseconds\n')
        for r in rows:
            f.write('\t'.join(str(v) for v in r) + '\n')
    print(f'liffile {liffile.__version__}: {len(rows)} planes')


if __name__ == '__main__':
    main()
