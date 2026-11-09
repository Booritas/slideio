# Ventana BIF reader driver design

Date: 2026-11-09. Status: proposed; no driver implementation in this change.

## 1. Intent

Pathologists need to open Roche/Ventana BIF slides through slideio and see the
tissue correctly. BIF stores a slide as overlapping camera tiles in an ordinary
tiled TIFF, plus XML saying how the tiles join. If you read the TIFF raster as it
is, a strip of tissue is doubled at every seam, about every 1000 px on older
iScan files. A pathologist sees that, and a measurement across a seam is wrong.
The driver's job is to produce seamless, correctly placed tissue.

The aim is faithful geometry, not pixel agreement with a reference reader.
OpenSlide and Bio-Formats each stitch BIF differently, and neither is treated as
the oracle. QuPath is the reference for *presentation*: each scanned area is its
own image.

Primary input: [Ventana BIF format description](../../../documents/image_formats/bif/Ventana_BIF_Format.md).
That document comes from reading the OpenSlide and Bio-Formats source, and was
not checked against samples. §2 below records what the real samples show. Where
they disagree with the description, the samples win. The format description
lives outside this repository for now; like the MRXS one, it will move under
`software-docs/`, and this link has to be updated when it does.

### 1.1 Goals

- One scene per scanned AOI (area of interest), stitched with no doubled tissue
  at seams, at every pyramid level.
- Older iScan files (Coreo/iScan HT, `StitchEncode.exe`) and DP 200 files
  (`ScanOutputManager`) both open through the one driver.
- Label, thumbnail and the DP 200 tissue mask are available as auxiliary images.
- Calibrated resolution, magnification, ICC profile and the raw XML are exposed.
- Concurrent reads from day one (`supportsConcurrentReads() == true`).
- Bounded region reads. Never decode the whole slide to answer a request.

### 1.2 Non-goals

- Pixel-exact agreement with OpenSlide or Bio-Formats.
- Z-stacks. **Decided:** a file with more than one Z layer is refused with a
  clear error until a real multi-layer sample is available (§4.5).
- Per-tile registration, blending or feathering across seams.
- Correcting the small shear the joints also report (§5.4).
- A view of the whole slide with all AOIs placed on the glass.
- `.tif` files from Ventana scanners. Only `*.bif` is claimed.

### 1.3 Anti-goals: these count as failures even if the code "works"

- Tissue duplicated or missing at a seam that the XML could have prevented.
- A DP 200 file failing to open because its joints say `LEFT`. The OpenSlide
  code this description was drawn from does exactly that (§2.3).
- Unscanned AOIs, or unused storage tiles, appearing as tissue.
- A malformed stitching packet silently read as unstitched raster.

## 2. What the samples show

Three slides, in `e:\Archive\slideio\images-extra\bif\openslide\`, with an
`index.yaml` that records their licences. The figures below came from
inspecting the TIFF directories and the tag-700 XML of each file.

| File | Scanner / software | Level 0 storage | Acquisition tile | AOIs | Aux IFDs |
|---|---|---|---|---|---|
| `OS-1.bif` (3.9 GB, H&E) | iScan, `StitchEncode.exe v0.1`, 2011 | 118784×102000 | 1024×1360 | 1 | Label Image, Thumbnail |
| `OS-2.bif` (2.7 GB, Ki-67) | same | 128000×82960 | 1024×1360 | 3 (one unscanned) | Label Image, Thumbnail |
| `Ventana-1.bif` (227 MB, trichrome) | VENTANA DP 200, `ScanOutputManager 1.1.0`, 2022 | 24576×21504 | 1024×1024 | 1 | Label_Image, Probability_Image |

What all three have in common:

- BigTIFF, little-endian.
- The pyramid is YCbCr JPEG in 8-bit, three-channel tiles.
- The TIFF tile size equals the acquisition tile size, so 1024×1360 tiles do
  exist on disk.
- Levels are labelled `level=N mag=M quality=Q`. Each level halves the previous
  one, with magnification going from 40 down to 0.078125 (OS) or 0.3125 (DP 200).
- Tag 700 (XMLPacket) appears in IFD 0 (`iScan` acquisition XML) and in the
  level-0 IFD (`EncodeInfo` stitching XML, 0.14 to 2.4 MB, NUL-padded).
  - The DP 200 also carries a `PrescanData` packet on its mask IFD.
- `Z-layers="1"` everywhere.
- Only the DP 200 has an ICC profile (tag 34675). It sits on the level-0 IFD.

### 2.1 Layout differences

- IFD 0 is the label in both generations. Its description is `Label Image`
  (iScan) or `Label_Image` (DP 200).
  - iScan: the label is JPEG and tiled.
  - DP 200: the label is uncompressed RGB and striped.
- On iScan, IFD 0 also carries the `iScan` XML. On DP 200 it is
  `<Metadata><iScan …>`.
- IFD 1 is `Thumbnail` on iScan, and `Probability_Image` on DP 200 (8-bit
  greyscale, LZW, striped, the same size as the label).
- AOI placement:
  - DP 200: one `ImageInfo` carrying `AOIIndex`, plus a `FrameInfo`.
  - iScan: `ImageInfo` has no `AOIIndex`. The AOI is identified by position,
    with the `AoiOrigin` children named `AOI0`, `AOI1`, …
- `OS-2` has three `ImageInfo`/`AOI<n>` pairs, and `AOI1` has `AOIScanned="0"`.
  - Origins: AOI0 (0, 38080), AOI1 (6144, 68000), AOI2 (30720, 0).
  - Each origin is a multiple of the tile size, and the AOIs fill different
    parts of the storage raster.

### 2.2 Joints

Each joint was mapped through the snake numbering of the format description
(§6 there). Every joint in all three files links tiles that really are
neighbours:

- `RIGHT` and `LEFT` both mean *tile 2 is one column to the right of tile 1*.
- `UP` means *tile 2 is one row above tile 1*.
- No other direction occurs.

| File | Usable joints (`FlagJoined=1`, confidence > 0) | Horizontal overlap | Vertical overlap |
|---|---|---|---|
| OS-1 | 2870 RIGHT, 4165 UP; 10174 unjoined (confidence 0, overlap 0) | mean 112.8, range 92–187 | mean 108.8, range 66–155 |
| OS-2 AOI0/AOI2 | 78+88 and 3334+3246 | mean ≈112.8, range 95–132 | mean ≈110, range 75–177 |
| Ventana-1 | 462 LEFT, 460 UP, all joined, confidence 100 | **0 at every column boundary except 3|4, 7|8, 11|12, 15|16, 19|20, where it is exactly 24 in every row** | 0 everywhere |

The cross-axis components are small. `RIGHT` joints report a vertical shift of
about +1.9 px, and `UP` joints a horizontal shift of about −1.6 to −2 px.

**What this check does not prove.** The joints confirm that alternate rows
reverse direction. They do not show which way up the grid is: whether the
snake's first row is the bottom row of storage, as the format description says,
or the top, or whether the columns are mirrored. A flipped grid still pairs
every joint with a neighbour. Get it wrong and every per-boundary overlap
(§5.1) lands on the mirror-image boundary. On iScan files that matters,
because single-joint overlaps vary from 66 to 177 px. This has not been checked
against pixels. §8 makes it the gate for the geometry work.

### 2.3 Consequences for the design

1. **A single global average is wrong for DP 200.** Averaging Ventana-1's
   horizontal overlaps gives 5.45 px per tile. Inside each group of four tiles
   (one camera frame), that wrongly cuts away up to about 16 px of tissue, then
   resets. The overlap belongs to specific column boundaries, not to every tile.
   The geometry is therefore estimated **per column boundary and per row
   boundary** (§5.1). On iScan files the estimate is just as good: each boundary
   has dozens of joints.
2. **`LEFT` is not a different geometry, at least in this file.** It is treated
   exactly like `RIGHT`. Bio-Formats' column-correction interpretation of
   `LEFT` is not reproduced.
3. `FlagJoined=0` joints carry no information: their overlap is 0 and their
   confidence 0. They are ignored.

## 3. Components

The driver is a new library, `slideio-bif`, in `src/slideio/drivers/bif/`, with
driver id `BIF`. It is a separate library rather than one more driver inside
`slideio-svs` like PHTIFF. Unlike PHTIFF, it reuses almost nothing of
`SVSTiledScene`, because its read path is a stitch rather than a tile copy.

All internal types live in `slideio::bif`. The parser and geometry classes are
exported with the driver's `SLIDEIO_BIF_EXPORTS` macro so that
`slideio_bif_tests` can test them in isolation, the same as MRXS.

| Class | Responsibility |
|---|---|
| `BIFImageDriver` | id `BIF`, file spec `*.bif`; `openFile` builds a `BIFSlide` |
| `BIFSlide : CVSlide` | Classifies IFDs, parses XML, owns scenes and aux images, slide-level raw metadata |
| `BIFScene : CVScene` | One stitched AOI (or the whole raster, §4.4); levels, resolution, ICC, read path; owns a `ContextPool` of TIFF handles |
| `bif::Description` | Parses `level=… mag=…` descriptions, keeping unknown keys |
| `bif::AcquisitionInfo` | Parses the IFD 0 `iScan` packet: `ScanRes`, `Magnification`, `Z-layers`, `ScannerModel`, other attributes |
| `bif::StitchInfo` | Parses `EncodeInfo`: AOIs, origins, grids, joints, and the association between `ImageInfo` and `AoiOrigin` |
| `bif::AoiGeometry` | Per-AOI level-0 column/row positions and seam cuts (§5); pure, file-free, unit-testable |
| `bif::SmallScene` (or reuse) | Aux images. Reuse `SVSSmallScene` if it reads a striped or tiled single IFD without SVS assumptions; otherwise write a minimal equivalent |

## 4. Open pipeline

### 4.1 Detection

`canOpenFile` matches the name only: `*.bif`, case-insensitive. It does no I/O,
and the structural probe runs in `openFile`. This is the MRXS rule and for the
same reason: a probe that fails inside `canOpenFile` makes
`ImageDriverManager::findDriver` hand a corrupt BIF to GDAL or report "Cannot
find driver", and the user gets a useless error. Add `"BIF"` to `driverOrder`
in `imagedrivermanager.cpp`, before `"GDAL"`.

`openFile` checks that IFD 0 has an XMLPacket whose root is `iScan`, or is
`Metadata` with an `iScan` child. If not, it fails with
`BIF: not a Ventana BIF file (no iScan metadata in IFD 0)`.

### 4.2 Reading tag 700

`TiffTools` does not read XMLPacket today. `readStringTag` suits ASCII tags
only. Add a count-based byte reader to `TiffTools` that follows the ICC read at
`tifftools.cpp` (`TIFFGetField(tiff, TIFFTAG_XMLPACKET, &count, &data)`). Two
details matter:

- Trim at the first NUL. The level-0 packets are NUL-padded.
- Parse with tinyxml2. tinyxml2 resolves no external entities, which meets the
  format description's XXE advice.

Only IFD 0 and the level-0 IFD are read. The DP 200 mask's `PrescanData` is
ignored.

### 4.3 IFD classification

Each IFD in the main chain is classified by its description:

- **Contains a `level=` token: a pyramid level.**
  - `level` and `mag` are required.
  - Levels must be 0, 1, 2… in chain order, and every level must be tiled.
  - The TIFF tile size must be the same at every level.
  - `d = mag0 / magL` must be an integer within 1e-6. Otherwise the error is
    `BIF: level L has non-integral downsample …`.
- **`Label Image` or `Label_Image`:** aux image `Label`.
- **`Thumbnail`:** aux image `Thumbnail`.
- **`Probability_Image`:** aux image `Probability`.
- **Anything else:** ignored, with a debug log line naming the IFD and its
  description.

### 4.4 Scenes

When there is stitching XML, there is one `BIFScene` per AOI with
`AOIScanned != 0`, in document order.

- The scene name is `AOI<n>`, where `n` is:
  - the `AOIIndex` when present;
  - otherwise the number in the matching `AoiOrigin` child's name;
  - otherwise the ordinal.
- Keeping the original index means OS-2's scenes are `AOI0` and `AOI2`, and
  match the XML and the label on the glass.

Associating `ImageInfo` with `AoiOrigin`:

- The two lists must have the same length, and they are paired by order.
- If an `ImageInfo` has an `AOIIndex` and it disagrees with the number in the
  paired origin's name, that is an error.

When the level-0 IFD has no XMLPacket, the file is a plain Ventana TIFF. It gets
one scene named `Image` that reads the storage raster with no stitching. Its
AOI is the whole raster and every boundary has overlap 0, so the same code path
serves both cases.

When a packet is present but malformed, the open fails. It is never treated as
"no packet".

### 4.5 Refusals

Each condition below gives an error that names the file and the cause:

- `iScan` `Z-layers` > 1, or TIFF tag 32997 (ImageDepth) > 1 on any pyramid IFD:
  `BIF: multi-layer (Z=N) files are not supported yet`.
- A *usable* joint (`FlagJoined=1`, confidence > 0, in a scanned AOI) with:
  - a tile number outside 1..C·R;
  - tiles that are not neighbours in its direction;
  - a direction other than `RIGHT`, `LEFT` or `UP`, named in the message.

  Joints the driver ignores (unjoined, confidence 0, or in an unscanned AOI)
  are not checked: refusing a slide over data the driver never uses helps
  nobody. *(Builder-check call.)*
- An AOI origin that is not a multiple of the tile size, or an AOI grid that
  runs outside the level-0 storage raster.
- Tile dimensions in `ImageInfo` that differ from the level-0 TIFF tile.
- `NumCols`/`NumRows` < 1, a non-positive `ScanRes` when present, or levels
  that break §4.3.

### 4.6 Per-scene properties

| Property | Source |
|---|---|
| Size | §5.2, at level 0 |
| Channels / type | 3 × uint8 (from the level-0 IFD; refuse anything else for now) |
| Resolution | `ScanRes` µm → metres, both axes; unknown (0) when absent. Level L: × d |
| Magnification | `iScan` `Magnification`; else level 0 `mag` token |
| Compression | level-0 IFD compression |
| ICC profile | level-0 IFD `iccProfile`, when present (DP 200) |
| Levels | one `LevelInfo` per pyramid IFD: size = AOI size / d (rounded down), scale 1/d, magnification `mag`, tile size = TIFF tile size |
| Concurrent reads | `true` |

Slide raw metadata is the IFD 0 XML packet, in `MetadataFormat::XML`.

The level-0 `EncodeInfo` packet (up to 2.4 MB) is not exposed. Each scene's raw
metadata is the `ImageInfo` element of its own AOI *without* its joint
children, serialised as XML. That keeps it small.

Aux images get their IFD's ICC profile, if it has one.

## 5. Geometry

Everything here is in level-0 pixels. Let `Tw×Th` be the tile size, and
`C×R` the AOI grid, with tile `(c, r)` counted from the top-left; snake
numbering gives `(c, r)` per the format description §6. Storage position of
tile `(c, r)`: `sx = OriginX + c·Tw`, `sy = OriginY + r·Th`.

### 5.1 Overlap per boundary

Column boundary `b` (between columns `b` and `b+1`, `0 ≤ b < C−1`): take every
joint in the AOI that is

- `RIGHT` or `LEFT`,
- `FlagJoined=1`,
- confidence > 0,

and whose tile 1 is in column `b`. The boundary's overlap `ox[b]` is the
**median** of their `OverlapX`. Row boundaries `oy[b]` are built the same way:
`UP` joints whose tile 2 is in row `b` (the upper row), with the median of
their `OverlapY`.

The median is used rather than OpenSlide's confidence-weighted mean because the
iScan overlaps have outliers: 187 against a median of about 112. The median
also recovers the DP 200 pattern exactly: 24 at four boundaries, 0 elsewhere.

When a boundary has no usable joint:

1. Use the median over all usable joints of that direction in this AOI.
2. If there are none, use the median over the whole slide.
3. If there are still none, use 0 (tiles abut).

Each fallback that is taken is logged once per AOI at INFO.

Overlaps are clamped to `[0, Tw−1]` (or `[0, Th−1]`), and a value that needed
clamping is logged at WARNING. A negative overlap would leave a gap, and gaps
are not filled.

### 5.2 Placement and size

- Column positions: `x[0] = 0`, `x[c+1] = x[c] + Tw − ox[c]`.
- Row positions: `y[0] = 0`, `y[r+1] = y[r] + Th − oy[r]`.

Overlaps are integers in every sample. A fractional value is rounded to the
nearest integer at the boundary, so positions stay integral at level 0.

AOI size: `W = x[C−1] + Tw`, `H = y[R−1] + Th`.

### 5.3 Seams

At each column boundary, the overlap `[x[b+1], x[b] + Tw)` is cut at its
midpoint, `cx[b] = x[b+1] + ox[b] / 2` (integer division). Rows are cut the same
way to give `cy[b]`.

Tile `(c, r)` owns output columns `[cx[c−1], cx[c])` and output rows
`[cy[r−1], cy[r])`, where the outer edges are `0` and `W`/`H`. So every output
pixel comes from exactly one camera tile. There is no blending, and painting
order does not matter.

This is the reason for midpoint cuts. A tile's centre is better than its
edge (vignetting and lens edge effects), and splitting the overlap shares the
damage evenly.

### 5.4 Known limitation: shear

The cross-axis components (§2.2) are ignored, as OpenSlide does. That leaves a
mismatch of about 2 px (about 0.5 µm at 40×) along each seam. At normal viewing
it is invisible, and it does not accumulate visually, because each seam only
compares two neighbours. Correcting it would need non-separable placement. That
work is deferred and should be revisited only if a pathologist reports seam
misalignment.

### 5.5 Mapping at reduced levels

The storage rasters of reduced levels are the level-0 raster downsampled by `d`.
At coarse levels `Th/d` is fractional: OS files have 1360/32 = 42.5 at level 5.
So mapping works pixel by pixel, separably, without resampling:

```text
for an output pixel column X at level L:
    X0 = X * d                                  # level-0 coordinate
    c  = column whose [cx[c-1], cx[c]) contains X0
    storage_col_L = floor((sx(c) + (X0 - x[c])) / d)
(rows likewise)
```

Copy `storage(storage_col_L, storage_row_L)` from the level-L TIFF raster. At
d = 1 this is an exact integer copy. At reduced levels the error is under one
level-L pixel at seams. A rendered row and column map is computed once per
request, a vector each, which keeps the cost to O(width + height) plus the copy.

## 6. Read path

`readResampledBlockChannelsEx` picks a level with `Tools::findZoomLevel`, scales
the rect, and delegates to `readResampledLevelBlockChannelsEx`, the same as
SVS and MRXS. The level read then:

1. Goes through `readClampedBlock`, so a rect that runs past the AOI edge is
   filled with background (the issue #45 contract).
2. Builds the column and row maps of §5.5 for the clamped rect at level L.
3. Groups the map into runs that land in the same **storage TIFF tile**. Each
   run spans one tile in each direction, so each needed TIFF tile is decoded
   once per request, even at coarse levels where many acquisition tiles share
   one TIFF tile.
4. For each storage tile:
   - decode it with `TiffTools::readTile`, on a TIFF handle borrowed from the
     scene's `ContextPool` (the `SVSReadContext` pattern; the pool is the last
     member, as in `SVSTiledScene`);
   - gather its pixels into the output by remapping its runs.
5. Resizes to `blockSize` when that differs from the rect size, and extracts the
   requested channels.

A storage tile that the AOI grid never references is never read (anti-goal 3).
Large reads follow the MRXS pattern: when the source exceeds the existing
`maxResampleSourcePixels` bound, render in bands.

## 7. Integration checklist

1. `src/slideio/drivers/bif/`: driver sources, `CMakeLists.txt` modelled on svs
   (TIFF, JPEG, opencv, tinyxml2; `SLIDEIO_BIF_API`; `FIX_MACOS_RPATH`), plus
   `bif_api_def.hpp`.
2. `src/slideio/drivers/CMakeLists.txt`: `add_subdirectory(bif)`.
3. Root `CMakeLists.txt`:
   - `BIF_DRIVER_NAME`;
   - an entry in `NAME_TOOL_LIB_LIST`;
   - an entry in `SLIDEIO_DRIVER_LIBS`.
4. `src/slideio/slideio/CMakeLists.txt`: link the library.
5. `src/slideio/slideio/imagedrivermanager.cpp`:
   - include the header;
   - `make_shared` in `initialize()`;
   - add `"BIF"` to `driverOrder` before `"GDAL"`.
6. `TiffTools`: the XMLPacket byte reader (§4.2), with a unit test.
7. `auxfiles/package-smoke/main.cpp`: add to `kExpectedDrivers`.
8. `cmake-scripts/packaging.cmake`: the Debian description's format list.
9. Docs:
   - `README.md` format table;
   - `CLAUDE.md` counts: "14 formats … 13 driver libraries", the format list,
     and the source-layout tree;
   - `software-docs/TEST_IMAGES.md`, regenerated.
10. Tests:
    - `src/tests/bif/` (executable `slideio_bif_tests`);
    - `src/tests/CMakeLists.txt`;
    - a `TestEntity("BIF", …)` row in `test_imagedrivermanager.cpp`;
    - rows in `test_scene_index.cpp`, `test_scene_lifetime.cpp` and
      `test_edge_reads.cpp`;
    - the `BINARY` map in `auxfiles/list-test-images.py`.
11. CI: `slideio_bif_tests --gtest_filter=-BIFCorpus*` next to each MRXS line in
    `build-validation.yml` and `release.yml`.
12. Copy the three samples into the corpus at
    `$SLIDEIO_IMAGES_PATH/bif/openslide/`, with `index.yaml`.

## 8. Validation

**Gate before §5 is coded: check the orientation against pixels.** On OS-1 and
on OS-2 AOI2:

1. Choose about five column boundaries and five row boundaries whose joint
   overlaps differ clearly from the AOI median.
2. Decode the two neighbouring TIFF tiles at level 0, straight from storage.
3. Find the overlap that maximises the normalised cross-correlation of the
   facing edge strips.
4. Compare it with the boundary's median under each of four orientations: the
   format description's (bottom-up), top-down, column-mirrored, and both.

Exactly one orientation should agree to within a few pixels. If it is the
format description's, go ahead. If it is another one, change the snake
numbering in `bif::StitchInfo` to match it. Record the result in this section
and in a corrigendum to the format description; the description is
source-derived, not authoritative. If none agrees, stop and come back to the
design.

The check is a throwaway tool in the scratchpad, not a shipped test. Its
conclusion is pinned by the corpus seam correlation test below.

Orientation verified 2026-11-09: described snake order. The check shipped after
all, as `BIFCorpusOrientation.SnakeMatchesPixels`, sampling 20 joints per axis.
Horizontal joints agree 18/20 (OS-1) and 20/20 (OS-2 AOI2), vertical joints
20/20 and 19/20. The three misses are joints whose recorded overlap is an
outlier that no orientation reproduces. Each axis can rule out only one flip.
Overlaps belong to boundaries (§2.3), so the orthogonal flip lands a joint on
another true neighbour pair across the same boundary and agrees as well as the
right orientation does. For example, rows flipped agrees on 17/20 and 19/20
horizontal joints but 0/20 vertical ones. The test therefore requires the
described order to agree on both axes, and every other order to fall below
half on at least one. Columns mirrored: 1/20 and 5/20 horizontal. Both flipped:
0/20 and 6/20 horizontal, 0/20 vertical on both files.

**Corpus-free (CI).** These run on XML strings committed in the test sources;
no TIFF fixture is needed:

- description parsing;
- `iScan` parsing, both forms;
- `EncodeInfo` parsing, including NUL padding, malformed XML, an absent packet,
  unscanned AOIs, an `AOIIndex`/name mismatch and missing `AOIIndex`;
- snake numbering on odd and even rows;
- non-neighbour joints;
- per-boundary medians and each fallback, clamping, positions, size and cuts;
- the level mapping at d = 1, 2 and 32 with fractional `Th/d`;
- the Z refusal.

The XML for the corpus-free tests is a trimmed extract of each real sample: one
AOI's `ImageInfo` with a handful of joints. It is not invented.

**Corpus (`BIFCorpus*`).**

- All three files open with the right scene count and names:
  - OS-1: `AOI0`;
  - OS-2: `AOI0`, `AOI2`;
  - Ventana-1: `AOI0`.
- Sizes match §5.2 recomputed independently in the test from the XML.
- Resolution and magnification.
- The ICC profile is present on Ventana-1 only.
- Aux names: `Label` and `Thumbnail`; `Label` and `Probability`.
- Seam tests. For a few boundaries in each file, read a strip across the seam.
  Check that it equals the two source tiles stitched at the cut, read directly
  from the TIFF.
  - For OS-1, also check that the 1-px columns either side of the cut have
    high correlation, which shows no visible tissue duplication.
- A level read agrees with the scaled read, using the existing helpers.
- `TestTools::concurrentReadIdentityTestAllScenes`.
- An edge read past the AOI is background.

**Manual sign-off before release.** A pathologist or the user views OS-1,
OS-2 and Ventana-1 in a viewer built on slideio at 40×, 10× and 2.5×, side by
side with QuPath. A visible seam artefact blocks the release.

## 9. Follow-on

- Z-stacks, once a multi-layer sample exists: the storage-block order in the
  format description §9 is the starting hypothesis and has to be verified on
  that sample.
- Shear correction (§5.4), if seams are reported.
- `.tif` Ventana files, if a sample turns up.
- Moving the format description into `software-docs/`.
