# Leica LIF reader driver design

Date: 2026-11-11. Status: implemented on feature/lif-driver (2026-11-11); manual
review of AI-written code pending before merge. Points marked *(decided during
implementation)* record where the build departed from, or settled, the original
design.

## 1. Intent

Leica confocal and widefield systems (LAS X / LAS AF) save their output as
`.lif` files. A single LIF is a whole *project*: tens of images, each of which
can be a Z-stack, a time series, a multichannel acquisition or a tile scan, kept
in a folder tree. slideio should open these files the same way it opens CZI or
ZVI, so a user can list the images, read any plane or region by channel, Z and
T, and get calibration and timing.

The pixels are stored uncompressed. The difficulty is not decoding: it is
*addressing*. Each image's XML gives a byte increment (`BytesInc`) for every
axis, and the axis nesting order varies between files and even inside one file
(C-before-Z and Z-before-C both occur in `Project007.lif`; tiles inside time in
the combined-scan sample). Two of the three reference readers surveyed get this
wrong on real files (readlif fails thousands of region comparisons). The driver's
job is to address every plane correctly, and to refuse what it does not
understand rather than show wrongly ordered pixels.

Primary input: [Leica LIF format description](../../../documents/image_formats/lif/Leica_LIF_Format.md)
(cited below as *FD §n*) and its [validation folder](../../../documents/image_formats/lif/validation/README.md).
The description is source-derived (Bio-Formats, readlif, liffile) and was
checked against seven real LIF files and two LIFEXT companions: 9,264 sampled
regions matched Bio-Formats exactly with direct stride addressing. That
stride-addressing rule (FD §13.3) is the basis of this design. Like the BIF and
MRXS descriptions, it lives outside this repository for now; it should move
under `software-docs/`, and these links must be updated when it does.

The aim is agreement with Bio-Formats on pixels, plane identity and image
dimensions — Bio-Formats is the reference here, unlike BIF, because LIF has no
geometry to interpret, only addressing to get right, and Bio-Formats' addressing
has been confirmed on every sample.

### 1.1 Goals

- Open every ordinary image in a LIF project as a scene, with its folder path as
  its name.
- Correct plane addressing for any nesting order of C, Z, T and tile axes, by
  explicit byte strides — never by an assumed `XYZCT`/`XYCZT` order.
- Tile scans exposed one scene per field, as Bio-Formats and QuPath do.
- Sample types uint8, uint16, uint32, float16, float32, float64, chosen from
  channel `DataType`/`Resolution` where present (FD §13.2).
- Significant bits (12-bit in 16-bit storage), physical resolution in X/Y/Z, time
  increment, per-plane timestamps where complete, channel attributes, raw XML.
- Concurrent reads from day one (`supportsConcurrentReads() == true`).
- Every read validated against its own memory block's bounds.

### 1.2 Non-goals

- **Stitching tile scans** into a mosaic from stage positions. LAS X writes the
  stitched result as a separate `…_Merged` image when the user asks for one, and
  that image opens as an ordinary scene. FD §7.2: there is no overlap model.
- **The `.lifext` companion** (previews `*_pmd_N`, histograms). **Decided**
  (2026-11-11). Its framing and its `ChildrenOf` link to parent blocks are
  understood (FD §11.2, §13.4), but what its records mean is not. The `pmd`
  children run from 512 px wide downwards, so they are thumbnails, not the
  missing levels of a pyramid. No reference reader is usable to check against:
  Bio-Formats ignores the file and liffile reads only its first group. Every
  primary image reads completely from the `.lif` alone. Exposing `_pmd_0` as a
  thumbnail was considered and declined. It stays a follow-on (§10).
- LOF (`LMS_Object_File`) and XLIF/XLEF/XLCF containers.
- FLIM/TCSPC raw event blocks, and any other memory block no `Data/Image`
  references (Intro.lif has seven).
- ROI, annotation, laser, detector and filter models. Their XML stays reachable
  through raw metadata.
- Applying orientation flags (`FlipX`, `FlipY`, `SwapXY`) to pixels. Bio-Formats
  does not; neither do we (FD §8.3).
- Bio-Formats' row-padding heuristic (FD §7.3). Padding is honoured only where a
  declared Y stride says so.
- Recovering truncated payloads as blank planes.

### 1.3 Anti-goals: these count as failures even if the code "works"

- A plane, channel or tile read from the wrong place because an axis order was
  assumed instead of taken from the strides.
- A memory block attached to an image by position when its ID says otherwise
  (FD §5).
- A read that runs outside its own image's payload into a neighbouring block
  header or another image.
- A 12-bit image normalised, or packed 12-bit unpacking attempted. Storage is
  16-bit; significant bits are metadata (FD §6).
- `DataType="0"` four-byte data shown as float32 because the X stride is 4
  (FD §13.2; the LIFEXT histograms are exactly this).
- One unsupported image (a λ-scan, say) making the whole 40-image project
  unopenable.

## 2. What the samples show

Seven LIF files, all in `E:\Archive\slideio\images-extra\lif\`
(`bioformats/`, `zenodo/`, `figshare/`), summarised from FD §11 and §14:

| File | Size | XML images | Series (tiles expanded) | Notable layout |
|---|---:|---:|---:|---|
| `AMR1.lif` | 3.0 GB | 13 | 27 | uint16, 12 significant bits, 5 channels, Z=7, tile groups of 6/5/6, merged images 2867 and 1961 wide |
| `20191025 Test FRET 585. 423, 426.lif` | 49 MB | 8 | 8 | multichannel |
| `PR2729_frameOrderCombinedScanTypes.lif` | 227 KB | 1 | 4 | X/Y, C, Z, **tile, T** nesting: tile inside time |
| `150519_FRAP_test_ROIs_chromagreen.lif` | 9.6 MB | 8 | 8 | one-channel time series |
| `Project007.lif` | 112 MB | 4 | 7 | uint8, C-before-Z **and** Z-before-C, width 981 tightly packed |
| `Intro.lif` | 605 MB | 15 | 57 | uint16, Z up to 177, 21- and 12-field tile scans, 7 unreferenced blocks |
| `IT4_27_02_15.lif` | 749 MB | 26 | 26 | uint8, 1–3 separate scalar channels |

Common to all: `LMSDataContainerHeader Version="2"`, eight-byte block lengths,
UTF-16LE XML, at least one zero-length block, no trailing bytes, `Memory/@Size`
equal to the binary payload length for every matched image.

Not covered by any sample: Version-1 (four-byte length) LIF, interleaved RGB/BGR,
floating-point primary images, XZ/XT scans, extra axes (DimID 5–9, 11) with more
than one element, row padding, non-ASCII names, truncated files. The design
supports some of these on the strength of the format description alone; §8 says
which, and how each is tested without a sample.

## 3. Components

The driver is a new library, `slideio-lif`, in `src/slideio/drivers/lif/`, with
driver id `LIF`. Internal types live in `slideio::lif`; the parser and layout
classes are exported with `SLIDEIO_LIF_EXPORTS` so `slideio_lif_tests` can test
them in isolation, as MRXS and BIF do.

| Class | Responsibility |
|---|---|
| `LIFImageDriver` | id `LIF`, file spec `*.lif`; `openFile` builds a `LIFSlide` |
| `LIFSlide : CVSlide` | Owns the `FileReader`, the parsed header, block index and scenes; slide raw metadata |
| `LIFScene : CVScene` | One image field: size, channels, Z, T, types, resolution, timestamps, read path |
| `lif::Container` | Reads the file header and the XML, parsing it once into a `lif::XmlDescription`; indexes memory blocks (§4.2): `blocks()` lists every block in file order, empty ones included, and `findBlock(id)` returns the first with an id, its `occurrences` counting duplicates. Reads through `FileReader`, holds no handle |
| `lif::XmlDescription` | The parsed XML description: the document and its top-level elements, a synthetic `<LEICA>` wrapper excluded (§4.2) |
| `lif::ImageDescription` | One `Data/Image`, from `parseImages`: name path, channels, dimensions, memory ID, attachments it needs (§4.3) |
| `lif::Layout` | Pure, file-free: from an `ImageDescription` and its block length, the validated axis model, sample type and the offset function (§5). Throws `lif::Unsupported` with a reason |
| `lif::parseTimestampTicks`, `lif::PlaneTimes` | The first parses both timestamp encodings of an `Image` into FILETIME ticks; the second maps one field's ticks to (t, c, z) planes in storage order (§6.3) |

`lif::Layout` is where correctness lives and is the most heavily unit-tested
piece. Nothing in it touches a file.

## 4. Open pipeline

### 4.1 Detection

`canOpenFile` matches the name only: `*.lif`, case-insensitive, no I/O. The
structural probe runs in `openFile`. This is the MRXS/BIF rule: a probe that
fails inside `canOpenFile` makes `ImageDriverManager::findDriver` pass a corrupt
LIF to GDAL or report "Cannot find driver". Add `"LIF"` to `driverOrder` in
`imagedrivermanager.cpp`, before `"GDAL"`.

`openFile` then requires: little-endian `0x70` at offset 0, `0x2A` at offset 8, a
positive XML character count whose `2*N` bytes fit in the file, and an XML root
that is not `LMS_Object_File` (LOF). Failures raise
`LIF: not a Leica LIF file (<reason>)`. The permissive "`0x70` at byte 3"
acceptance of Bio-Formats' `initFile` (FD §2) is not reproduced.

### 4.2 Header, XML and memory blocks

- **XML**: read `2*N` bytes, decode as UTF-16LE with `Tools::fromUnicode16`
  (not Bio-Formats' NUL-stripping, FD §3), parse with tinyxml2, which resolves no
  external entities. If the text does not parse, wrap it in a synthetic `<LEICA>`
  root — after removing a leading `<?xml …?>` declaration, which tinyxml2 refuses
  inside an element — and parse once more; failing that, the open fails with
  `LIF: malformed XML description: <tinyxml2's error>`, quoting the unwrapped
  parse. A text that parses is never wrapped, however many top-level elements it
  has: every top-level element is searched, both for the container identity
  (an `LMS_Object_File` anywhere rejects the file; the version comes from the
  first `LMSDataContainerHeader`) and for images (§4.3). The XML is parsed once
  per open: `Container` keeps the parsed `XmlDescription` and `parseImages` walks
  it. *(Decided during implementation.)*
- **Block length width** is chosen by the container version (FD §13.1):
  root `LMSDataContainerHeader`, `Version="1"` → four bytes, `Version="2"` →
  eight. Any other version falls back to the marker probe (FD §4), and a probe
  result is accepted only if the identifier count is sane (`0 < M < 4096`), the
  payload lies inside the file, and the position after it is EOF, an all-zero
  trailer, or another header starting `0x70 … 0x2A`. Otherwise the open fails —
  an ambiguous probe never silently picks a width. *(My call: the probe exists
  only for versions no sample has shown.)*
- **Traversal**: from `13 + 2*N`, read records until EOF. A header whose first
  integer is zero ends the traversal (the zero trailer); the remaining bytes are
  checked to be zero and a WARNING is logged if they are not. Every offset is
  `uint64_t` with checked addition.
- **Index**: every block is recorded, empty ones included. A duplicated ID is
  recorded as ambiguous: an image referencing it is unsupported (§4.4); an
  unreferenced duplicate is harmless and only logged at INFO. *(Decided during
  implementation: core logging has no DEBUG level, so every message this design
  once put at DEBUG is INFO.)*
- A block whose declared payload runs past EOF ends traversal; it is recorded
  as truncated, and an image referencing it is unsupported.
- A malformed block header in mid-file (bad marker, impossible identifier count)
  ends traversal with a WARNING; images whose blocks were not indexed are skipped
  as unsupported. *(Decided during implementation.)*

### 4.3 Images

Following FD §5, on the parsed XML:

1. Remove `LDM_Block_Sequential_Master` subtrees.
2. Collect every `Image` element under an `Element/Data`, in document order,
   excluding those whose grandparent is `ProcessingHistory`.
3. For each, the memory reference is the `Memory/@MemoryBlockID` among the
   immediate children of the owning `Element`. No ID, `Memory/@Size="0"`, or a
   block of length 0 with no positive `Memory/@Size`: the element is a container,
   not an image, and is skipped silently. A block of length 0 whose `Memory/@Size`
   is positive is an image with its data missing, and is reported by rule 6.
4. Memory blocks are attached **by ID only**. Positional matching (Bio-Formats'
   fallback and its late recovery pass) is not implemented: every sample
   resolves by ID, and a positional guess is anti-goal 2.
5. When several image descriptions reference the same memory ID, the first in
   document order is used (what the validation did, FD §11.1).
6. `Memory/@Size`, when present, must equal the block's payload length.
7. The name is the `Element/@Name` path from the image up to, but excluding, the
   outermost experiment element, joined with `/` (FD §5).

### 4.4 Supported and unsupported images

Each image is passed to `lif::Layout` (§5). An image the layout rejects is
**left out of the scene list**, and a WARNING names the image and the reason
(`LIF: image "Folder/Lambda 1" skipped: dimension 9 (excitation wavelength) has
12 elements; extra axes are not supported`). The slide still opens. If no image
in the file is supported, the open fails, and the message lists the first few
reasons.

The skipped images are recorded on the slide so a test, and a curious user, can
find them: the slide's raw metadata (§6.4) is the whole XML, and the driver
logs the list once at open. Errors raised while opening are normalised at
`LIFSlide::openFile` to begin with `LIF: ` and end with `(file: <path>)`, whichever
layer threw them. *(Decided during implementation.)* *(My call: one exotic acquisition in a project
must not lock the user out of the rest — anti-goal 6. The trade-off is that
scene indices are not XML image indices; §4.5.)*

### 4.5 Scenes

One `LIFScene` per supported image per tile field, in document order, fields in
index order. This is Bio-Formats' series expansion (FD §7.2), so a scene index
matches a Bio-Formats series index whenever no image was skipped.

- An image without a tile axis, or with one of a single element: scene name is
  the image's path, e.g. `TileScan 1/Merged`.
- An image with `K > 1` tile fields: scene `j` is named `<path>/Field <j+1>`.
  *(My call on the spelling.)* `FieldX`/`FieldY` are not in the name, and no
  API exposes them: every field's raw metadata is the image's owning `Element`
  (§6.4), whose `TileScanInfo` attachment holds all the `Tile` records, and field
  `j`'s record is the `j`-th `Tile`. *(Decided during implementation.)*

All fields of one image share one `Layout` and one block; a scene holds a
`shared_ptr` to the layout and its own tile index.

## 5. Layout: from XML to byte offsets

### 5.1 Axis model

Parse every `DimensionDescription` (`DimID`, `NumberOfElements`, `BytesInc`,
`Origin`, `Length`, `Unit`; every floating-point value in the XML is read with `.` as
the decimal point whatever the process locale, *PR #96 review*) and every `ChannelDescription` (`DataType`,
`Resolution`, `BytesInc`, `LUTName`, `ChannelTag`, `Min`, `Max`, `Unit`, any
other attribute kept verbatim). Axes are kept by identity, not flattened:

| DimID | Role |
|---:|---|
| 1 | X — must be present and must be the first dimension |
| 2 | Y |
| 3 | Z |
| 4 | T |
| 10 | M, tile field |
| 5, 6, 7, 8, 9, 11, other | extra |

Display axes: X, and Y = the DimID 2 dimension. With no DimID 2, the next
dimension in document order is Y if it is Z (an XZ scan) or T (an XT scan); that
axis then has extent 1 as a stack axis (FD §6). In that case an XZ scan has Z resolution 0 and numZ 1, an XT scan has T resolution 0, and an image with no Y, Z or T second axis is one row high *(decided during implementation)*. Rules, each a rejection
with a reason when broken:

- X, Y and every stacked axis have `NumberOfElements ≥ 1`; a repeated DimID is
  rejected (Bio-Formats' "second Y fills Z, then T" is not reproduced).
- A negative `BytesInc` is rejected on X, on the display Y and on any axis with
  more than one element. On an axis of one element it is never used (index 0) and
  is taken as 0 *(PR #96 review)*.
- An extra axis with more than one element is rejected. With exactly one
  element it is ignored. *(Bio-Formats folds extra axes into Z or T, which
  misnames them; refusing is what FD §9 recommends. λ-scans are the likely
  casualty and the first follow-on, §9.)*
- At most one tile axis (FD §6: more than one is not described).

### 5.2 Sample type

Per channel, from `DataType` and `Resolution` (FD §13.2):

| DataType | Resolution | Type, storage bytes |
|---:|---|---|
| 0 | 1–8 / 9–16 / 17–32 / 33–64 | uint8 / uint16 / uint32 / uint64 → `DT_Byte` / `DT_UInt16` / `DT_UInt32` / `DT_UInt64` |
| 1 | 9–16 / 17–32 / 33–64 | `DT_Float16` / `DT_Float32` / `DT_Float64` |
| other | | rejected |

When `DataType` is absent on every channel, fall back to Bio-Formats' X-stride
rule (FD §6: 1 → uint8, 2 → uint16, 4 → float32, 8 → float64). It is a fallback,
not a cross-check override: when `DataType` is present the stride must agree
with it, or the image is rejected.

All channels of an image must share one storage width (FD §13.2: liffile does
not support mixed either). `Resolution` is reported as
`getChannelSignificantBits`, clamped to the storage width (it can exceed it when the
type came from the X-stride fallback); pixels are returned as stored.

*(Decided during implementation.)* UInt32 and UInt64 images are skipped as
unsupported in this version: core maps `DT_UInt32` to a wrong OpenCV type, and
OpenCV has no 64-bit unsigned type (§10 has the follow-on). `DataType` present on
only some channels, or mixed integer and float at one width, is rejected.

### 5.3 Interleaved colour

If X `BytesInc` equals `S × b` with `S > 1` (b = sample bytes), samples are
interleaved. Supported only for `S = 3` with exactly three channels whose
`BytesInc` values are `0, b, 2b`. Other interleavings are rejected.

The scene then has three channels in **R, G, B order**. Which stored sample is
red comes from `ChannelTag` (1 = R, 2 = G, 3 = B) when all three channels carry
one; otherwise storage is taken to be **BGR**, as Bio-Formats does (FD §6,
§13.2). No sample exercises either path; §8 covers it with synthetic fixtures,
and a corpus sample is a release follow-on.

### 5.4 Offset function

For scalar channels, the byte address of pixel `(x, y)` of channel `c`, slice
`z`, frame `t`, field `m` is (FD §13.3):

```text
addr = B + ch[c].BytesInc + z·Z.BytesInc + t·T.BytesInc + m·M.BytesInc
         + y·Y.BytesInc + x·X.BytesInc
```

where `B` is the payload start and absent axes contribute 0. This one rule
covers every nesting the samples show, including tile-inside-time, without
deriving a plane order. For interleaved colour the channel term selects the
sample within the pixel.

`Layout` validates once, at open:

- **Bounds.** The largest address (every index at its maximum, plus `b`) is at
  most `B + payloadLength`. All arithmetic is checked `uint64_t`; overflow is a
  rejection.
- **No aliasing.** Sort the stacked strides (channels as one axis with stride =
  smallest non-zero channel increment, then Z, T, M) ascending; each stride must
  be at least the byte extent of everything nested inside it, starting from the
  plane extent `(H−1)·Y.BytesInc + W·X.BytesInc`. A layout where two planes
  would overlap is rejected rather than read.
- **Rows.** `Y.BytesInc ≥ W · X.BytesInc`. A larger Y stride is row padding and
  is honoured exactly; no heuristic padding (FD §7.3).
- **Channels** for scalar data: channel increments are distinct, and sorted
  ascending they are evenly spaced (the channel axis has one stride, which the
  aliasing check above then validates). Unevenly spaced channel increments are
  rejected, not guessed at (FD §13.2: liffile's gap-inference has an open TODO).

Because every read is bounded by its own block, a read can never touch another
image (anti-goal 3).

## 6. Per-scene properties

### 6.1 Geometry and type

| Property | Source |
|---|---|
| Rect | `(0, 0, W, H)` from the display axes |
| Channels | channel count (3 for interleaved RGB) |
| Z slices / T frames | Z / T `NumberOfElements`, 1 when absent or used as display Y |
| Channel type | §5.2 |
| Compression | `Compression::Uncompressed` |
| Zoom levels | one (no pyramid in LIF; §1.2) |
| Concurrent reads | `true` (§7) |
| Channel background | `getChannelBackground` returns 0 for every channel (dark fluorescence background; *decided during implementation*) |
| Channel name | Always empty for now: none of the seven corpus files carries `UserDefName`/`UesrDefName`, so its shape is unknown and is not parsed (`lif::Channel::name` stays empty) |
| Channel attributes | every `ChannelDescription` attribute, via `setChannelAttribute` (`LUTName`, `ChannelTag`, `Resolution`, `Min`, `Max`, …) |

### 6.2 Calibration

`slideio::Resolution` is in metres.

- X, Y: `|Length| / (N − 1)` for `N > 1` (FD §8.1; not `Length/N`). Unit `m`
  only; any other unit gives 0 (unknown), silently. `N = 1` gives 0.
- Z (`getZSliceResolution`): the same rule on the Z dimension.
- T (`getTFrameResolution`): the T dimension's `|Length|/(N−1)` when its unit is
  `s` (`Ks` × 1000); else the scanner setting `nDelayTime_s` when present; else 0.
  *(My call: Bio-Formats uses only `nDelayTime_s`; the T dimension is the more
  direct statement. The corpus test compares both and records any
  disagreement.)*
- For an XZ scan the Y resolution is the Z spacing; for XT it is 0 (time is not
  a length).
- Magnification: `ATLConfocalSettingDefinition` or `ATLCameraSettingDefinition`
  `@Magnification`, a direct child of the `HardwareSetting` (or `HardwareSettingList`)
  attachment; failing that, the first one under an `LDM_Block_Sequential` in it; else 0.

*Corpus outcome (`LIFCorpusOpen.ScenesMatchBioFormats`, `.Magnification`):* X, Y and
Z agree with Bio-Formats to 1e-9 relative on 137 of 137 series, except Z on the 21
Z stacks of `IT4_27_02_15.lif`: Bio-Formats reports one step for the whole file,
1.00708103855232 µm (the scanner's `dblVoxelZ`/`dblStepSize`), while `|Length|/(N−1)`
gives 1.0070808–1.0070813 µm per image — under 3e-7 relative, kept as is and printed by
the test. Magnification matches the attribute in six files (FRET-Efficiency, a
computed image, has none). `IT4_27_02_15.lif` names its attachment `HardwareSettingList`
and holds the definition only inside `LDM_Block_Sequential`; when no direct definition
carries `@Magnification`, the first one under `LDM_Block_Sequential` is used (sequences
share the objective), giving 100 on the Z stacks and 5 on the `*_5x` images.

### 6.3 Timestamps

Parse `TimeStampList` (hex, LAS AF ≥ 3.1) or `TimeStamp` `HighInteger`/
`LowInteger` (older) into FILETIME ticks, masking the low word (FD §8.2).

The k-th timestamp belongs to the k-th stored plane in **storage order**: the
stacked axes (C, Z, T, M) sorted by stride, fastest first — the same order §5.4
validates. `hasPlaneTimestamps()` is true only when the list has exactly one
entry per stored plane (C·Z·T·M); anything else reports no timestamps and logs
the count at INFO. Partial lists are never padded (FD §8.2).

`getAcquisitionTime()` is the earliest timestamp of the scene's own field, in
Unix seconds, floored to a whole second; `getPlaneTimestamp` is the offset from
that floored value, so none is negative, as the `CVScene` contract requires, and
`acquisition + offset` is exact. The earliest plane may therefore read a
sub-second fraction rather than 0. A tick list containing a zero tick, or one that
gives an acquisition time before 1970, is treated as absent. *(Decided during
implementation.)*

*(Builder's note: the field-dependence of timestamps is not established by any
reader — Bio-Formats reuses one array for every field. Storage order including M
is the natural reading of the data; the corpus test checks plane timestamps
against liffile on Intro's tile scans and the FRAP time series.)*

*Corpus outcome (`LIFCorpusOpen.Timestamps`, liffile 2026.11.8 from PyPI):* all 461
planes of the 21 FRAP and Intro images with a complete list agree — liffile floors to
a millisecond, so `acquisition + offset` is required to lie in [liffile, liffile + 1 ms),
which it does on every plane, the 21- and 12-field tile scans of Intro included (M in
storage order). `Region 1` and `Region 1_Merged` (6 stamps for 354 planes) report none.

### 6.4 Metadata

- Slide raw metadata: the complete decoded XML, `MetadataFormat::XML`.
- Scene raw metadata: the image's owning `Element`, serialised without its
  child `Element`s (so a folder's raw metadata does not contain its children),
  `MetadataFormat::XML`. Includes `TileScanInfo` with every field's `Tile`
  record — field `j`'s is the `j`-th `Tile` — from which a caller can get stage
  positions. All fields of an image share this text.
- No auxiliary images. No ICC profile.

## 7. Read path

`LIFScene` overrides `readResampledBlockChannelsEx(rect, size, channels, z, t)`;
the 4D and level variants use the base class defaults (single level).

1. Clamp the rect to the scene through the existing `readClampedBlock`
   contract, so area outside the image is background (issue #45).
2. For each requested channel, compute the plane base from §5.4 with
   `(c, z, t, m)`. Rows are read with `FileReader::readAt`:
   - Y stride equal to the row bytes and the rect spanning the full width: one
     read for the whole band.
   - Otherwise one read per row of `w · X.BytesInc` bytes.
   - Interleaved colour: one read per row covers all three channels; scalar
     channels are read plane by plane and merged into the output's channel
     interleave.
3. Reorder BGR to RGB when §5.3 says so.
4. Resize to `blockSize` when it differs, with the resize helper the other
   drivers use, then return.

A downscale (no larger on either axis) whose source exceeds
`ReadLimits::maxResampleSourcePixels` (16 Mi pixels) is not read whole: it is read in
tiles and area-filtered tile by tile, so memory stays bounded on merged mosaics.
*(Decided during implementation; revised after the PR #96 review.)*

- **Same filter as the unbanded read.** The weights are those of
  `cv::resize(INTER_AREA)` for the whole source rectangle — OpenCV's
  `computeResizeAreaTab`, rebuilt per axis with the same scale, cell bounds and
  thresholds — indexed by each source pixel's position in that rectangle. Output
  pixel `(ox, oy)` is the weighted mean of the box `[ox·sx, (ox+1)·sx) ×
  [oy·sy, (oy+1)·sy)` with fractional edge weights in global coordinates. Each tile
  adds its pixels' weighted values to per-output sums (horizontal pass, then
  vertical), so tiles need not align with output cells and an output cell that
  spans many tiles collects its sum from all of them. The result does not depend on
  the budget and equals the unbanded `cv::resize` within 1 for integer types (sums
  kept in double and rounded half up, where OpenCV accumulates in float or fixed
  point) and within 1e-4 relative for float types, 1e-3 for half floats — at integer
  and non-integer ratios alike.
- **Budget invariant, on both axes.** No buffer such a read assembles from the file
  holds more than `max(maxResampleSourcePixels, 1)` source pixels: a tile is as wide
  as the rectangle when a row fits in the budget, else a run of one row. A
  width-only downscale and a single output row whose box is the whole image obey it
  like any other. The per-row byte span `readRaw` gathers gapped or interleaved
  pixels from is part of one tile row, so within it too. Tests observe the peak
  through `lif::ReadStats` (an optional member of `ReadLimits`).
- **Not tiled:** a read within the budget, and any read that enlarges either axis.
  For those `cv::resize` interpolates rather than averages, and a read that enlarges
  needs no more memory than its output on that axis — except a read that enlarges
  one axis while shrinking the other, which is read whole (a known gap: such a
  read of a large mosaic is not bounded by the budget).

**Concurrency.** The scene's only file state is the slide's `FileReader`, which
is cursor-free and thread-safe; the `Layout` is immutable after open. So the
scene reports `supportsConcurrentReads() == true` with no `ContextPool` —
the CZI/ZVI shape. No `thread_local` holds a handle.

Endianness: samples are little-endian; on a big-endian host they are swapped
after reading (the `core/tools/endian` helpers). slideio does not build for a
big-endian target today, so this is a guard, not a tested path.

## 8. Validation

### 8.1 Corpus-free (CI)

Synthetic LIF fixtures are generated by `auxfiles/lif-fixtures/generate.py`,
which shares no code with the driver (the MRXS arrangement), and committed in
`src/tests/lif/data/`. Pixels are a function of `(x, y, c, z, t, m)` so a test
can assert exactly which plane it got. The directory is marked `-text` in
`.gitattributes`. Fixtures:

- Version 1 (four-byte lengths) and Version 2 (eight-byte), empty blocks,
  unreferenced blocks, zero trailer, non-zero bytes after the trailer.
- Block order differing from XML order (ID matching), a duplicate referenced ID,
  a missing ID, `Memory/@Size` mismatch, a truncated final block.
- The combined-scan nesting (C, Z, tile, T) and its permutations: Z-before-C,
  tile outermost, tile innermost.
- uint8, uint16 with `Resolution="12"`, uint32 with `DataType="0"`, float16,
  float32, float64, and `DataType` absent (stride fallback).
- Interleaved RGB with `ChannelTag` RGB, with `ChannelTag` BGR, and untagged.
- XZ and XT scans; an extra axis of 1 (accepted) and of 12 (skipped, slide
  still opens); a project where every image is unsupported (open fails).
- Padded rows (Y stride larger than the row).
- Aliasing strides and out-of-bounds strides (rejected).
- Non-ASCII element names (UTF-16LE decoding).
- Both timestamp encodings; a short timestamp list.

Unit tests on `lif::Layout`, `lif::parseTimestampTicks` and `lif::PlaneTimes` additionally run on XML
fragments copied from the real samples' inventories in the validation folder.

### 8.2 Corpus (`LIFCorpus*`)

The seven files are copied to `$SLIDEIO_IMAGES_PATH/lif/` with their
attributions (FD §14). Every corpus suite is named `LIFCorpus*`; CI runs
`slideio_lif_tests --gtest_filter=-LIFCorpus*`.

- **Pixels against Bio-Formats.** The validation TSVs record three 7×5 regions on
  every plane of every series (9,264 regions), with dimensions, type and plane
  order. Hashes there are SHA-256; the test instead uses zlib CRC-32 (zlib is
  already a dependency, and the test needs no cryptographic hash). A one-off
  script extends `compare_regions.py` to emit a CRC-32 per region *after*
  confirming the SHA-256 against Bio-Formats, and that reference file is
  committed in `src/tests/lif/`. The test reads every region through the public
  `Scene` API and compares. Scene count, names, sizes, channel counts and Z/T
  counts are checked against the same file.
- Significant bits 12 on AMR1; types per file.
- Resolution against Bio-Formats' physical sizes (recorded by extending
  `LifCheck.java`), to 1e-9 relative.
- Timestamps against liffile on FRAP and Intro.
- `TestTools::concurrentReadIdentityTestAllScenes`.
- An edge read past the image is background.
- A level read agrees with the scaled read (trivially single-level, but it
  pins the contract).

Gaps that remain after this: interleaved RGB, float and Version-1 files on real
data. The release notes say so until a sample appears.

## 9. Integration checklist

1. `src/slideio/drivers/lif/`: sources, `CMakeLists.txt` modelled on bif
   (opencv, tinyxml2, slideio-core; no TIFF/JPEG), `lif_api_def.hpp`
   (`SLIDEIO_LIF_API`). No `FIX_MACOS_RPATH` there, as in no other driver: it
   rewrites a target's references to the libraries of `NAME_TOOL_LIB_LIST`
   (item 3), so it belongs on the targets that link the driver — the main
   library and `slideio_lif_tests` — which call it.
2. `src/slideio/drivers/CMakeLists.txt`: `add_subdirectory(lif)`.
3. Root `CMakeLists.txt`: `LIF_DRIVER_NAME`, `NAME_TOOL_LIB_LIST`,
   `SLIDEIO_DRIVER_LIBS`.
4. `src/slideio/slideio/CMakeLists.txt`: link the library.
5. `imagedrivermanager.cpp`: include, `make_shared` in `initialize()`, `"LIF"`
   in `driverOrder` before `"GDAL"`.
6. `auxfiles/package-smoke/main.cpp`: `kExpectedDrivers`.
7. `cmake-scripts/packaging.cmake`: the Debian description's format list.
8. Docs: `README.md` format table; `CLAUDE.md` ("15 formats … 14 driver
   libraries", the format list, source-layout tree, test executables, the
   `LIFCorpus*` filter rule); `software-docs/TEST_IMAGES.md` regenerated.
9. Tests: `src/tests/lif/` (`slideio_lif_tests`), `src/tests/CMakeLists.txt`,
   a `TestEntity("LIF", …)` row in `test_imagedrivermanager.cpp`, rows in
   `test_scene_index.cpp`, `test_scene_lifetime.cpp`, `test_edge_reads.cpp`, and
   the `BINARY` map in `auxfiles/list-test-images.py`.
10. CI: the `slideio_lif_tests --gtest_filter=-LIFCorpus*` line next to the BIF
    one in `build-validation.yml` and `release.yml`.
11. Python bindings (`slideio-python`): nothing driver-specific; confirm the
    driver appears in `get_driver_ids()`.

## 10. Follow-on

- Extra axes, starting with λ-scans (DimID 5/9): needs a decision on how a
  fifth dimension is exposed, since `CVScene` has only C, Z, T.
- `.lifext` companions: verify whether `*_pmd_N` children are a usable pyramid
  for their `ChildrenOf` parent and expose them as zoom levels or aux images.
- Truncated acquisitions: expose the complete planes of a block that ends early.
- UInt32 and UInt64 images (§5.2): fix `CVTools::toOpencvType` for `DT_UInt32`
  first, and decide how a 64-bit unsigned type is exposed.
- Real samples for interleaved RGB, float and Version-1 LIF.
- XLIF/XLEF (LAS X's folder-based export), which share the XML model.
- Moving the format description into `software-docs/`.
