# MRXS test fixtures and reference tools

Three hand-run Python tools support the MRXS driver tests
(design: `software-docs/specs/2026-11-06-mrxs-driver-design.md`, section 9).
None of them runs in CI, and none shares code with the C++ driver.

| Tool | Writes | Purpose |
|---|---|---|
| `mrxs-fixtures/generate.py` | `src/tests/mrxs/data/` (committed) | Synthetic bundles plus the geometry a correct reader reports |
| `mrxs-openslide-reference.py check-fixtures` | nothing | Checks every synthetic fixture against OpenSlide |
| `mrxs-openslide-reference.py references <images-root> <out>` | `<images-root>/mirax/reference/` | OpenSlide reference regions of the nine real bundles |
| `mrxs-renderer-prototype.py <reference-dir>` | nothing | Measures the design's renderer against those references |

## Environment

The fixtures in the repository were produced with:

- Python 3.12.8 (the `slideio` conda environment)
- Pillow 11.0.0 (JPEG payloads only), numpy 2.2.0 (prototype only), zlib 1.2.13
- openslide-python 1.4.7 with openslide-bin 4.0.1.2 (OpenSlide 4.0.1)

```bash
pip install "openslide-bin==4.0.1.2" openslide-python pillow numpy
python -I auxfiles/mrxs-fixtures/generate.py
python -I auxfiles/mrxs-openslide-reference.py check-fixtures
```

PNG and BMP payloads are encoded by hand, so regenerating writes byte-identical
files with any Pillow version except for the one JPEG fixture,
`zlibmap-d2-jpeg`. The generator refuses to clear an output directory it did not
write itself (one without its `manifest.json`).

## Fixtures

Each `<name>` has an empty `<name>.mrxs`, the bundle directory `<name>/`, and
`<name>.expected.json`. For a valid fixture that file holds level sizes and
downsamples, every drawable rectangle per level, the level-0 populated bounds,
the background colour, associated-image sizes, and probe pixels: pixels covered
by exactly one drawable, well inside it (exact colour), and pixels no drawable
comes near (background). For an invalid fixture it holds the error category
(`corrupt` or `unsupported`) and whether it must surface on `open` or on `read`.

| Fixture | Exercises |
|---|---|
| `nomap-d1` | D=1, no map, zero overlap: fast path, Q>1 at coarse levels, sparse holes, non-white fill colour |
| `rawmap-d2` | D=2 raw map: jittered photos, negative origin, inactive camera with garbage coordinates, populated camera at (0,0) coordinates (skipped by the zero-position rule) |
| `zlibmap-d2-jpeg` | The same with a zlib map and JPEG payloads (pixel tolerance 6) |
| `saved-c0-4` | Exponents `[2,1,2]`: C=`[4,8,32]`, C0 > D, map normalised by C0 |
| `nomap-overlap-d2` | No map, fractional overlap: synthesized positions, position-aware path, 2.5-pixel subtiles |
| `layout-variants` | 36-byte UUID identifier, nonstandard file names, BOM, CRLF, empty-key line, several pages per level each longer than `PAGEELEMENTCOUNT`, second hierarchical family with null slots, nonzero non-hierarchical next pointer, tagged family to ignore |
| `pages-out-of-order` | Page chain written out of file order (not comparable with OpenSlide, which reads pages sequentially) |
| `bmp24` | BMP24 payloads 13 pixels wide (padded rows) |
| `ini-duplicates` | Repeated INI key (last value wins) and repeated group (merged) |
| `bad-slide-id`, `bad-page-cycle`, `bad-payload-range`, `bad-negative-pointer`, `missing-data-file`, `bad-map-length` | Corrupt on open |
| `codec-mismatch`, `bmp-not-24bit` | Corrupt on read: the payload contradicts `IMAGE_FORMAT` |
| `nondivisible-grid` | Unsupported on open: `IMAGENUMBER_X` not a multiple of D |

`check-fixtures` currently reports all valid fixtures in agreement with
OpenSlide 4.0.1 (level sizes, downsamples, background, bounds, every probe
pixel). OpenSlide opens `bad-page-cycle`, `bad-payload-range` and
`nondivisible-grid`: those rejections are this driver's stricter policy.
