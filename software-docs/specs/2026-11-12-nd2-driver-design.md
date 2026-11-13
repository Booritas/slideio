# Nikon ND2 reader driver design

Date: 2026-11-12. Status: implemented on feature/nd2-driver (2026-11-13); manual
review of AI-written code pending before merge. Points marked *(my call)* are
design decisions taken without a requirement behind them; review them first.
Points marked *(Decided during implementation.)* record where the build settled
or departed from the original design.

## 1. Intent

Nikon NIS-Elements saves wide-field, confocal and slide-scanner acquisitions as
`.nd2` files. One file holds a whole experiment: several stage positions, each a
Z-stack and/or time series of one or more channels, and occasionally a single
whole-slide scan of 23641 × 23641 pixels. slideio should open these files the
way it opens CZI or LIF: list the positions, read any region of any channel, Z
slice and time frame, and report calibration and timing.

As with LIF, the pixels are easy (uncompressed, interleaved, little-endian) and
the work is in *addressing*. A frame's index is a row-major enumeration of an
experiment loop tree whose order differs between files (`T,Z`, `P`, `T,P,Z`,
`P,Z`, …), invalid XY points must be filtered out, files hold fewer or more
frames than the loops declare, one sample's chunk map is stale, and rows can be
padded. Both reference readers get at least one sample wrong (§2).

Primary input: [Nikon ND2 format description](../../../documents/image_formats/nd2/Nikon_ND2_Format.md)
(cited below as *FD §n*) and its [validation folder](../../../documents/image_formats/nd2/validation/README.md).
The description was written from Bio-Formats and Talley Lambert's `nd2`
package, and checked against 64 real files (55 chunked, 9 legacy JPEG 2000).
A standard-library parser written from it, `validation/nd2spec.py`, matched
Bio-Formats on 2,505 of 2,517 sampled regions; the 12 differences are a
Bio-Formats bug. Like the LIF, BIF and MRXS descriptions it lives outside this
repository for now; these links must be updated when it moves under
`software-docs/`.

The aim is agreement with Bio-Formats on pixels, plane identity and dimensions,
**except** where the FD shows Bio-Formats to be wrong (4-channel × RGB frames,
FD §6.2); there the reference is `nd2`, whose frame bytes match the FD layout.

### 1.1 Goals

- Both container variants: chunked ND2 (Ver2.x, Ver3.x) and legacy JPEG 2000
  ND2 (Ver1.0).
- One scene per acquired XY position, Z and T from the experiment loops, in any
  loop order.
- Sample types uint8, uint16, float32; significant bits; uncompressed and
  zlib-compressed frames; row padding taken from `uiWidthBytes`.
- Multi-component channels, including RGB and several RGB channels in one frame.
- Stale chunk maps recovered by a magic scan.
- Missing frames read as background; frames beyond the loop product ignored.
- Pixel size, Z step, time increment, magnification, per-plane timestamps,
  channel names and colours, raw metadata.
- Concurrent reads from day one.
- Bounded memory when a whole-slide frame is read downscaled.

### 1.2 Non-goals

- **Lossy-compressed frames** (`eCompression = 1`). No sample, no reference
  reader. The file fails to open with a message saying so.
- **Tiled frames** (`uiTileWidth/Height` smaller than the frame). No sample, no
  reader handles them. Rejected at open.
- **Custom loops** (`eType 7`) with more than one item, and NE time loops whose
  `pSubLoops` add a dimension. `CVScene` has only C, Z and T. Rejected at open
  (§5.3).
- Binary mask layers (`CustomDataSeq|<tag>|N!`), ROIs, JOBS definitions, LUTs,
  events. Their chunks are not decoded beyond raw metadata.
- Per-frame stage positions as an API. They stay reachable through raw metadata.
- A zoom pyramid. ND2 has none (FD §9); the scene has one level, as LIF does.
- Bio-Formats' size-based guessing of channel count, pixel type and row padding
  (FD §10). Attributes are trusted after validation or the file is refused.

### 1.3 Anti-goals: these count as failures even if the code "works"

- A frame taken from the wrong place because the loop order was assumed
  (`XYCZT`) instead of taken from the loop tree.
- Pixels read with a stride guessed from the width instead of `uiWidthBytes`
  (FD §6), or at `chunk + 4096` instead of through the chunk header (FD §3.4;
  `nd2`'s bug on `Exception9_e3.nd2`).
- The 4 × RGB file read with a 4-sample stride (Bio-Formats' bug, FD §6.2).
- A stale map entry trusted, so a frame is read from a neighbouring chunk.
- The `uiWidth = 0` file opened with guessed dimensions (FD §5).
- A missing frame returned as a neighbouring frame's pixels rather than as
  background.
- `uiSequenceCount` used as the frame count (FD §7.4).

## 2. What the samples show

64 files in `E:\Archive\slideio\images-extra\nd2\`, inventoried one JSON per
file in `validation/inventory/` (FD §14):

| Variant | Files | Coverage |
|---|---:|---|
| Chunked Ver3.0 | 31 | uint8/uint16/float32, RGB, up to 5 channels, a 23641² × 2-channel slide (one 2.2 GB chunk), a 7 GB time-lapse |
| Chunked Ver2.2 | 4 | |
| Chunked Ver2.1 | 18 | stale map (`b16_pdtB+y50__crop.nd2`), `uiWidth = 0` (`290110.tranMgc005cr.nd2`), non-4096-aligned chunks |
| Chunked Ver2.0 | 2 | zlib frames, unpadded chunk names |
| Legacy JPEG 2000 | 9 | uint8/uint16, 1–2 channels, one RGB, one file missing the last frame's second channel |

Loop orders seen: `T,Z` 16, `P` 9, `T,P,Z` 8, `Z` 7, `T` 5, none 5, `P,Z` 2,
`T,P` 1. Channel/component combinations: (1,1) ×29, (2,2) ×12, (1,3) ×7,
(3,3) ×2, (5,5) ×2, (4,12) ×1. Frame count ≠ loop product in 7 files
(FD §7.4).

Where the two references disagree, and who is right:

| File | Bio-Formats | `nd2` | FD layout matches |
|---|---|---|---|
| `2D 500uM VPA002.nd2` (4 × RGB) | wrong stride | correct | `nd2` |
| `B08.nd2` (NE time loop) | correct, T = 81 | duplicated Z loop | Bio-Formats |
| `Exception9_e3.nd2` (unpadded) | correct | fixed +4096 offset | Bio-Formats |
| `4_2_1_cont_NoMR001.nd2` (legacy) | XY = 5 | XY = 11 (text-lookup bug) | Bio-Formats |

## 3. Components

A new library, `slideio-nd2`, in `src/slideio/drivers/nd2/`, driver id `ND2`.
Internal types in `slideio::nd2`, parser and layout classes exported with
`SLIDEIO_ND2_EXPORTS` so `slideio_nd2_tests` can test them in isolation, as LIF
does. Dependencies: OpenCV, tinyxml2, zlib (`zlib/1.3.1`, as in MRXS), and
`slideio-imagetools` for the legacy variant's JPEG 2000 decoding
(`ImageTools::decodeJp2KStream`).

| Class | Responsibility |
|---|---|
| `ND2ImageDriver` | id `ND2`, file spec `*.nd2`; `openFile` picks the variant by the first four bytes and builds the slide |
| `ND2Slide : CVSlide` | Owns the `FileReader` and the scenes; slide raw metadata; error normalisation |
| `ND2Scene : CVScene` | One XY position: size, channels, Z, T, type, calibration, timestamps, read path |
| `nd2::ChunkFile` | Chunked container: signature, trailer, chunk map, entry verification, magic-scan recovery; `find(name)` returns the verified header (offset, `nameLength`, `dataLength`) (§4.2) |
| `nd2::Variant` | A decoded metadata tree, one type for both encodings: `decodeClxLite(bytes)` (incl. type 76 zlib) and `decodeClxXml(text)` (FD §4) |
| `nd2::Attributes` | `ImageAttributes` with validation (FD §5) |
| `nd2::Experiment` | The flattened loop list (FD §7.2), XY validity, the frame-index function (§5) |
| `nd2::Channels` | Picture planes → channels and components (FD §6.2) |
| `nd2::FrameSource` | Abstract: "give me rows `[y0, y1)` of frame N" — `ChunkFrameSource` (raw / zlib) and `Jp2FrameSource` (legacy) |
| `nd2::LegacyFile` | JP2 box walk, box map, XML boxes, `jp2c` list (FD §11) |

`nd2::Variant`, `nd2::Experiment` and `nd2::Attributes` are pure (no file) and
carry most of the unit tests.

## 4. Open pipeline

### 4.1 Detection

`canOpenFile` matches the name only, `*.nd2` case-insensitive, no I/O — the
MRXS/BIF/LIF rule, so a corrupt ND2 is reported by this driver rather than
handed to GDAL. `"ND2"` goes into `driverOrder` in `imagedrivermanager.cpp`
before `"GDAL"`.

`openFile` reads the first 16 bytes:

- `DA CE BE 0A` → chunked; also require `ND2 FILE SIGNATURE CHUNK NAME01!` at
  offset 16. The `ND2 CHUNK MAP SIGNATURE 0000001!` trailer at `size − 40`
  (FD §2) is *not* required (R18): a file whose trailer is missing or destroyed
  — a crashed acquisition, a truncated copy — opens, and `ChunkFile` rebuilds
  the chunk map by the scan in §4.2 with a WARNING naming the file. *(Decided
  after review: recovering such a file is worth more than refusing it.)*
- `00 00 00 0C 6A 50 20 20 0D 0A 87 0A` → legacy; also require
  `LABORATORY IMAGING ND BOX MAP 00` at `size − 40`.
- Otherwise `ND2: not a Nikon ND2 file (<reason>)`.

All errors raised while opening are normalised at `ND2Slide::openFile` to
start with `ND2: ` and end with `(file: <path>)`, as LIF does. The legacy
branch is built by `nd2::LegacyFile` and the §4.7 reader.

### 4.2 Chunked container

- Version from the signature chunk's data (`VerM.m`). Major 3 reads the `…LV`
  chunks; major 2 prefers the XML chunks and falls back to `…LV` when a 2.x file
  has only those (FD §3.2). Any other major is refused by `ChunkFile` itself,
  so no caller can open such a file by another route. *(Decided during implementation.)*
- Chunk map: from the trailer offset, parse entries (`!`-terminated name, u64
  offset, u64 size) until the terminating `ND2 CHUNK MAP SIGNATURE 0000001!`.
  The `size` field is ignored; lengths come from each chunk's own header
  (FD §3.3).
- **Verification.** Every entry the driver uses is verified when first used:
  magic, and a name field that begins with the expected name. Frame entries are
  verified *all at once at open*, by reading each 16-byte header plus name.
  *(My call: cheaper than it sounds — one small `readAt` per frame, a few
  thousand at most in the samples — and it means a read can never meet a stale
  entry. Bio-Formats spot-checks about ten.)*
- **Recovery.** If any entry fails, the map is discarded and rebuilt by scanning
  the file for `DA CE BE 0A` followed by `nameLength < 8192` and a
  `!`-terminated ASCII name; a WARNING names the file and the number of bad
  entries. The scan reads in 4 MiB blocks through `FileReader`, and after a
  valid header skips `16 + nameLength + dataLength` before searching again
  (padding is zero, so the next magic is found quickly). A duplicate name found
  by the scan keeps the later chunk *(my call: an edited file appends)*, and is
  logged.
- All offsets `uint64_t`, all additions checked; a chunk whose data runs past
  EOF is treated as absent (R6) and reads as background, so `truncated_frame`
  is not a refusal. A raw frame chunk that lies inside the file but is shorter
  than the 8-byte prefix plus `height · uiWidthBytes` is *present* and throws on
  read: the driver never pads from neighbouring bytes (R20). *(Decided during implementation.)*

### 4.3 Metadata decoding

`nd2::Variant` is a tree of named nodes holding bool, int64, uint64, double,
string (UTF-8), byte array, or children. Both decoders produce it (FD §4):

- **CLX Lite**: records as FD §4.1; level `indexOffset` used only to skip the
  index table; empty-named siblings become a list; type 76 inflates with zlib
  and recurses. A type-76 record is a 2-byte header (type, name length) plus 10
  skipped bytes, so the zlib stream starts at offset 12 and nothing is sniffed
  (R13). All inflation in one decode shares a budget of 256 MiB, so nested
  type-76 records cannot multiply it. A byte array is decoded as a nested tree
  only on demand by the caller that expects one (JOBS), never speculatively.
  Every length is checked against the remaining buffer; a decode error names
  the chunk. *(Decided during implementation.)*
- **CLX XML**: tinyxml2 (no external entities). Before parsing, replace control
  characters other than tab/CR/LF with spaces (Bio-Formats does this; some files
  contain them). `runtype` selects the scalar type; `no_name` and the FD's list
  names (`BinaryItem`, `TextInfoItem`, `Wavelength`, `MinSrc`, `MaxSrc`) become
  list elements. Numbers are parsed with `.` as the decimal point whatever the
  locale (the LIF PR #96 lesson). A `CLxByteArray` in XML is stored as raw UTF-8
  text, not base64. Numbers must parse completely; the MSVC spellings of NaN and
  infinity (`1.#QNAN`, `-1.#INF`) are recognised and map to NaN/inf. *(Decided during implementation.)*
- The `SLx…` wrapper level (binary) or single `no_name` child (XML) is unwrapped.
- Field lookup tolerates the 2.x quirk of `uLoopPars` wrapped in
  `i0000000000` (FD §7.1).

### 4.4 Attributes, channels, experiment

- **Attributes** (FD §5): require `uiWidth > 0`, `uiHeight > 0`, `0 < uiComp ≤ CV_CN_MAX`
  (512, checked in `Attributes::parse` before the component count sizes anything)
  *(Decided during the final review.)*,
  `uiBpcInMemory ∈ {8, 16, 32}`, `uiWidthBytes ≥ uiWidth·uiComp·bytes`,
  `eCompression ∈ {absent, 0, 2}`, tile size absent, 0 or equal to the frame
  (0 means no tiling, R16). Anything else: the file fails to open with the reason. 8 → `DT_Byte`,
  16 → `DT_UInt16`, 32 → `DT_Float32`.
- **Channels** (FD §6.2): `sPicturePlanes.uiCount` channels,
  `componentsPerChannel = uiComp / uiCount`, which must divide exactly. Records
  from `sPlaneNew`, else `sPlane`, in component order. If picture metadata is
  missing, treat as `uiComp` single-component channels with no names
  *(my call)*.
- **Experiment** (FD §7.2): flatten the loop tree exactly as FD §7.2 rules 1–4,
  **without** `nd2`'s `pSubLoops` special case, and with one departure from
  rule 3: a loop that rule 3 would drop (another type at the same level, or a
  shallower level after a deeper one) means the tree branches, and the file is
  refused as unsupported rather than read with a frame order that no longer
  matches its loops. No corpus file has such a tree. *(Decided after the PR
  review.)* XY count = points whose
  `pItemValid` flag is non-zero (FD §7.3), where an empty `pItemValid` counts
  every point; NE time count = sum of valid periods, where a missing
  `pPeriodValid` means all periods are valid (R16). The 2.x quirk of `uLoopPars`
  wrapped in a one-item list is unwrapped (Task 8b: without it 22 Ver2.x files
  had no loops). Chunked and legacy files share one path: both build a
  `std::vector<Loop>` and call `Experiment::fromLoops` (R21), which owns the
  flatten rules, axis classification and the validation below. A `Loop` stores
  what it needs by value (R4). Then validate: *(Decided during implementation.)*
  - at most one time loop (types 1 and 8 both map to T), one XY loop, one Z loop;
  - a custom loop (`eType 7`) with count > 1 is refused;
  - spectral loops never appear (rule 2 removes them).

  `Experiment::frameIndex` takes the scene's position as an ordinal among the
  valid points, and `validItems` maps ordinal to loop item (R5). A refusal fails
  the open with the loop list in the message — unlike LIF there
  is no per-image fallback, because all positions share one loop tree.

### 4.5 Frames present, and the T count

The set of verified `ImageDataSeq|N!` chunks is the authority for which frames
exist (FD §7.4). Let the loop product be `L`:

- **Declared-frame bound.** With `present` the number of frames the file holds
  (frame chunks; complete frames in a legacy file), the loops may declare at
  most `min(2^31, (present + 1) · 1024)` frames. A file declaring more fails the
  open with `Unsupported`, naming the declared count and the bound. The check
  comes before anything is sized by a declared count: an XY loop whose point
  count (from `Points`, else `uiCount`) exceeds the bound is refused before its
  points are listed, a legacy TINF `XY(n)` above it likewise, and the product is
  checked in `Experiment::fromLoops` before frame times or positions are built.
  NE time period counts that are not positive are ignored, and a sum past
  INT64_MAX is refused. Across the 63 corpus files that get this far (the 64th is
  refused for `uiWidth` 0 first) the largest `L / present` is
  1.14 (`B08.nd2`, 920 declared, 810 present), so the factor of 1024 leaves
  three orders of magnitude for acquisitions stopped early while keeping a
  damaged count from allocating gigabytes. *(Decided during the final review.)*
- **T fitting (R25 as refined by R29, R31, R32).** Only when the time loop is the outermost loop:
  - *Trim.* T ends at the last declared time point holding a frame,
    `ceil((maxPresentIndex + 1) / innerProduct)` over the frames inside the
    loops (`B08.nd2`: 92 → 81, as Bio-Formats).
  - *Extend.* When the last declared time point holds a frame, surplus time
    points (FD §7.4) are added one at a time while each is **complete** -- all
    `innerProduct` frames present -- and its known frame times come after the
    previous time point's. Unknown (NaN) times leave completeness to decide
    alone. The first time point that fails either test stops the extension;
    frames that do not form such a time point are ignored. This gives
    `old_cells_02.nd2` 41 → 42, `rfp_h2a_cells_02.nd2` 115 → 116 and
    `jonas/3.nd2` 26 → 27 (R32: `rfp_h2a_cells_02`'s frame 973 is missing from
    inside the declared series, so its surplus time point is complete), and
    leaves `190821_L1_IRS-Akt_CS2003.nd2` (10 of 46
    positions), `multicolorTest.nd2` (14 of 15 Z) and `130606_JCC719_003.nd2`
    (82 surplus frames whose times run back into the declared series) at their
    declared T -- all as Bio-Formats.
  - A frame index at or past `L + presentFrameCount` (before fitting) is never
    an acquired frame (R31), which guards damaged chunk names and bounds the
    fitted product by the file.
  - The loop product `L` below is the one after fitting, and frame times cover
    it. Frames the fit does not take in are ignored (logged once at INFO).
- When T is not outermost, nothing is fitted, and frames with `N ≥ L` are
  ignored (logged once at INFO with a count).
- Missing frames with `N < L` read as background (§7). A present raw frame
  chunk too short for its image throws instead (§4.2, R20).
- `uiSequenceCount` is not used for counts; it is reported as metadata only.

### 4.6 Scenes

One `ND2Scene` per acquired XY position, in loop order; one scene when there is
no XY loop. This matches Bio-Formats' series and the LIF tile-field rule, so a
scene index equals a Bio-Formats series index.

Scene name: the point's `dPosName` when non-empty, else `Position <k+1>` with
`k` the index among valid points; a file without an XY loop has one scene named
after the file stem. *(My call on spelling.)* Duplicate point names get
` (<k+1>)` appended so names stay unique.

**Known gap.** Ver2.x chunked files that keep stage positions as `dPosX`/`dPosY`
lists rather than `Points` records get `Position <k+1>` names and empty scene
raw metadata; the legacy path already reads those lists (§11). *(Decided during implementation.)*

### 4.7 Legacy JPEG 2000 variant

Following FD §11:

- Walk the boxes from offset 0 (big-endian headers, 64-bit extended lengths)
  and read the box map from the trailer; require that the map's offsets each
  point at a box start. *(The walk is the authority if they disagree; FD §11.1
  shows they never do.)*
- `jp2h/ihdr` gives width, height, components and bit depth: 1 to 16 bits unsigned, read as 8-bit samples up to 8 bits and as 16-bit samples above, with the `ihdr` depth (12 for a 12-bit camera) as the significant bits unless `ARTT` says otherwise. *(Widened after the PR review; the corpus has only 8 and 16.)*
- XML boxes by LIM type: `VIMD` (frame 0) for channels, `ACAL`/`VCAL` for
  calibration, `AIM1`/`AIMD` for loops, `TINF` for text. These are parsed with
  the §4.3 XML decoder.
- **Dimensions.** Loops from `AIM1`/`AIMD` (types 2, 4, 8; type 6 skipped) as in
  §4.4, through `Experiment::fromLoops` (R21). The XY count comes from the `TINF`
  `Dimensions:` line when it has an `XY(n)` term (R22). When the number of points
  flagged valid equals `n`, those points are used and keep their stage records;
  otherwise `n` overrides the loop count, with points `0 … n-1`. Both give 5, not
  11, for `4_2_1_cont_NoMR001.nd2`. FD §11.3's statement that all 11 points are
  flagged in that file is wrong: only 6 to 10 are. *(Decided during implementation.)* Channels from `VIMD.PicturePlanes.Count`, never
  from the spectral loop.
- Frame `i`, channel `c` is the `(i·C + c)`-th `jp2c` box; RGB (`ihdr` 3
  components) has one codestream per frame. A missing codestream reads as
  background. For T fitting (§4.5) a frame counts only when the codestreams of
  all its channels exist, so `b16_14_12.nd2` (101 codestreams, 2
  channels) has T = 50, as Bio-Formats (R23; the first draft predicted 51). The
  51st frame is at or past the loop product and ignored. Only where such a
  partial frame is kept as a time point does it read its existing channels, and
  background for the missing ones. *(Decided during implementation.)*
- Pixels are returned in native byte order and RGB interleaved, as for chunked
  files. (Bio-Formats' big-endian, planar output is a convention the corpus test
  undoes, FD §11.3.)

## 5. Frame addressing

With loops `l0 … ln−1` outermost first, counts `n_j`, and the scene's position
index `p` placed at the XY loop's slot:

```text
frameIndex = ((i0·n1 + i1)·n2 + i2)…        (row-major, innermost fastest)
```

where `i_j` is `t`, `p` or `z` according to loop `j`'s axis. `nd2::Experiment`
computes it with checked arithmetic, and the XY index is the *loop* index of the
k-th valid point — when `pItemValid` excludes points, frames are numbered over
valid points only (FD §7.3: 44 × 1 × 3 = 132 frames for a 20-entry list with one
valid point).

Within a frame (FD §6), for component `k` of pixel `(x, y)`:

```text
pixelStart = chunkOffset + 16 + nameLength + 8
address    = pixelStart + y·uiWidthBytes + (x·uiComp + k)·bytesPerSample
```

The 8-byte time prefix is skipped; `nameLength` comes from the verified header,
never assumed to be 4072. For zlib frames the same addressing applies to the
inflated block.

## 6. Per-scene properties

### 6.1 Geometry and type

| Property | Source |
|---|---|
| Rect | `(0, 0, uiWidth, uiHeight)` |
| Channels | `uiComp`: every component is a slideio channel (§6.2) |
| Z slices / T frames | Z loop count / T after §4.5, 1 when absent |
| Channel type | §4.4 |
| Significant bits | `uiBpcSignificant` clamped to the storage width; 0 for float32 |
| Compression | `Uncompressed`, `Zlib`, or `Jpeg2000` (legacy) |
| Zoom levels | one |
| Concurrent reads | `true` (§7) |
| Channel background | 0 for every channel *(my call, as LIF)* |
| Magnification | `dObjectiveMag` when > 0, else the number immediately before the first `x`/`X` of `wsObjectiveName` that begins a word (`"Plan Apo 60x Oil"` → 60; R27), else 0. Legacy: the same fields of `VIMD` *(Decided during implementation.)* |

### 6.2 Channels and components

slideio channels are scalar, so a frame's `uiComp` components become `uiComp`
channels in storage order — the layout the FD and `nd2` agree on. For
single-component channels, slideio channel `k` is ND2 channel `k`. For RGB
(3 components per channel), components are R, G, B (FD §6.2), so a one-channel
brightfield or H&E image is an ordinary 3-channel RGB scene, and the 4 × RGB
file is a 12-channel scene. *(My call: the alternative — one slideio channel per
ND2 channel with 3 samples each — has no representation in `CVScene`.)*

Per slideio channel:

- `getChannelName`: the ND2 channel's `sDescription`; for multi-component
  channels `<name> R`/`G`/`B` (or `<name> <j>` when the count is not 3).
- Attributes: `Color` as `#RRGGBB` decoded from `uiColor` = `0x00BBGGRR`
  (FD §8), `ND2Channel` (index), `Component` (index within it), and the
  probe field present on the plane record (`pFluorescentProbe` name), kept
  verbatim as a string. Emission wavelength is not reported: the FD names no
  field for it (R16, §11). *(Decided during implementation.)*

### 6.3 Calibration

`slideio::Resolution` is in metres.

- X, Y: `dCalibration` µm × 1e-6 when `bCalibrated` is true and the value > 0,
  else 0. `dAspect ≠ 1` scales Y *(to be confirmed against Bio-Formats on the
  corpus; no sample is known to have it)*.
- Z: `|dZStep|`; if 0 and count > 1, `|dZHigh − dZLow| / (count − 1)` (FD §8).
- T (R28, R30), per scene: the mean difference of consecutive per-T frame
  times at the scene's own P, Z = 0 when they are all known; else the time
  loop's `dPeriod` ms ÷ 1000 (or the first valid NE period) when > 0; else 0.
  The measured step is what Bio-Formats' plane times give; `dPeriod` is only
  the requested interval (3600 s against a measured 8122 s in
  `ns_blobtest2.0_20x_A2_far.nd2`). `ImageInfo::tStep` holds the nominal
  fallback. *(Decided during implementation.)*
- **Recorded differences from Bio-Formats** (corpus test, asserted per file and
  per series with the driver's own value):
  - VPA002 channels, 12 against 4: Bio-Formats' stride bug (FD §6.2).
  - `b16_pdtB+y50__crop` T 57 against 56 (R26): all 684 frames are present and
    `nd2` and the file's text agree; Bio-Formats' rescan after the stale map
    keeps fewer offsets. Its T step differs too (638.5 against 633.8 s).
  - Magnification, 10 files where Bio-Formats has none: 8 legacy files (it does
    not parse legacy objective names), and BF007 (100x) and
    `130606_JCC719_003` (60x), where the driver equals `nd2`.
  - T step, 13 files: 9 legacy files (Bio-Formats has no plane times);
    `wtf_DIC` and `wtf_alignment` (T = 1, the driver falls back to `dPeriod`,
    600 s); `b16_pdtB`; `cag_p5_simgc_2511_70ms22s_crop` series 1 (Bio-Formats
    repeats series 0's 599.998 s, the position's own prefixes step 599.952 s).
  - Pixel size of the legacy `but3_cont200-1`, `but3_cont2002` and `por003`:
    Bio-Formats has none, the driver reports 50/50/100 µm, equal to `nd2`.
- Legacy: the same fields from `ACAL`/`VCAL`/`AIM1` (FD §11.2).

### 6.4 Timestamps

Per-frame time is `CustomData|AcqTimesCache!` (float64 ms, indexed by frame
index), or, if that chunk is absent, each frame's 8-byte prefix (FD §6; equal
in all 53 samples that have both). Frames past the end of a short cache take
their time from the prefix too, not NaN (R17). *(Decided during implementation.)*
The times are an `nd2::FrameTimes`: the cache table as it is, and a frame's
prefix (or a legacy frame's `VIMD`) read the first time that frame's time is
asked for, then kept. Opening a file reads only the times the T fit (R29) needs;
a scene works out its step and timestamps on first use. Nothing is sized by the
declared frame count. *(Decided after the PR review: open used to make one
positional read per frame, and the time vector could reach 2^31 doubles.)*

- `hasPlaneTimestamps()` is true when every frame of the scene that exists has
  a time and none is NaN. Missing frames return the scene's acquisition offset
  0. All channels of a frame share its time.
- `getAcquisitionTime()`: `dTimeAbsolute` (Julian day; unknown when < 1)
  converted to Unix seconds, plus the scene's earliest frame time, floored to a
  whole second. `getPlaneTimestamp(t, c, z)` is the offset from that floored
  value, so none is negative and `acquisition + offset` is exact — the LIF
  convention.
- No absolute time → `getAcquisitionTime()` 0 and plane timestamps relative to
  the experiment start (frame time ms ÷ 1000, the same for every scene; R19).
  When frame times are incomplete, `getAcquisitionTime()` is still reported
  while `hasPlaneTimestamps()` is false. *(Decided during implementation.)*

### 6.5 Metadata

- Slide raw metadata: `MetadataFormat::JSON`, an object with one member per
  decoded metadata chunk (`ImageAttributes`, `ImageMetadata`,
  `ImageMetadataSeq|0`, `ImageTextInfo`, `ImageCalibration|0`) holding its
  `nd2::Variant` serialised with nlohmann_json, plus `version`. Binary and XML
  sources give the same JSON shape. Byte arrays are emitted as their length
  only. *(My call: the binary trees have no textual form, and one format for
  both is easier to consume than XML for some files and something else for
  others.)* Legacy: the same, built from its XML boxes (frame-0 `VIMD`/`VCAL`).
- Scene raw metadata: the scene's XY point record (`Points.iN`) as JSON, or
  empty when there is no XY loop.
- No auxiliary images, no ICC profile.

## 7. Read path

`ND2Scene` overrides `readResampledBlockChannelsEx(rect, size, channels, z, t)`;
4D and level variants use the base defaults.

1. Clamp through `readClampedBlock` so area outside the frame is background
   (issue #45).
2. Frame index from §5; if the frame is absent, fill with background.
3. Ask the frame source for rows `[y, y+h)`:
   - **Uncompressed**: one `FileReader::readAt` of `h·uiWidthBytes` bytes when
     the rect spans the full width, else one read per row of `w·uiComp·bytes`
     bytes starting at column `x`. All components come with one read; the
     requested channels are picked out with `cv::mixChannels` (or `extractChannel`).
   - **zlib**: inflate the frame as a stream, discarding rows before `y` and
     stopping after `y+h−1`; the compressed data is read in 1 MiB blocks.
   - **Legacy**: decode the channel's codestream with
     `ImageTools::decodeJp2KStream` and crop.

   A tiled downscale reads the tiles of one frame through one
   `nd2::FrameCursor`, so a compressed frame is decoded once per read, not once
   per tile: a zlib cursor keeps its inflate position and the full-width rows of
   the last band, and a legacy cursor keeps each channel's decoded codestream.
   *(Decided after the PR review.)*
4. Resize to the requested size with the helper other drivers use.

**Large downscaled reads.** The 23641² frame is 2.2 GB of uint16 × 2. A
downscaled read whose source exceeds the budget is read in strips and area
filtered, exactly the problem LIF solved (LIF design §7). The tiled `INTER_AREA`
downscale now lives in slideio-core, taking a row-strip reader callback and
`ReadLimits`, and both drivers use it. It applies to uncompressed chunked
frames; zlib and legacy frames are small. Extracting it corrected the 2×2
rounding to match OpenCV for every channel count, which also fixed LIF's 2- and
5-channel downscale. *(Decided during implementation.)*

**Concurrency.** The only file state is the slide's cursor-free `FileReader`;
everything parsed at open is immutable; zlib streams and decode buffers are
local to a call. So `supportsConcurrentReads() == true` with no `ContextPool`,
the CZI/ZVI/LIF shape. No `thread_local` holds a handle.

Samples are little-endian (chunked); on a big-endian host they would be
swapped. slideio does not build for one; this is a guard, not a tested path.

## 8. Validation

### 8.1 Corpus-free (CI)

25 synthetic files from `auxfiles/nd2-fixtures/generate.py`, sharing no code with
the driver (and not with `nd2spec.py`, so fixtures and reference parser cannot
share a mistake), committed in `src/tests/nd2/data/`, marked `-text` in
`.gitattributes`. Pixels are a function of `(x, y, component, frameIndex)`.
Fixtures:

- Ver3.0 with 4072-byte names and 4096 alignment; Ver2.1 XML metadata; Ver2.0
  unpadded names (the `Exception9_e3` trap) and zlib frames.
- A stale map (entries offset from the real chunks) recovered by scan.
- Loop orders `T,P,Z`, `P,Z`, `Z,T`, none; an XY loop with invalid points; an
  NE time loop with an invalid period; a spectral loop (skipped as a dimension).
- Frames missing (stopped acquisition → T trimmed, a hole → background) and
  surplus frames.
- uint8/uint16 with 12 significant bits/float32; padded rows; 1 × RGB, 2 × mono,
  4 × RGB.
- CLX Lite with a type-76 compressed tree; XML with control characters and the
  `i0000000000`-wrapped `uLoopPars`.
- Refusals: `bad_width`, `lossy`, `tiled`, `custom_loop`, `two_time_loops`,
  `bad_comp`, `not_nd2`. The unsupported major version has no fixture of its
  own: it is tested by patching a temporary copy
  (`Nd2ChunkFile.RejectsUnsupportedMajor`).
- Missing trailer: recovered, not refused. `Nd2Slide.MissingTrailerRecovered`
  zeroes the trailer of a temporary copy of `v3_basic` and checks that every
  plane reads the same as the original.
- `truncated_frame`: a frame chunk whose declared length runs past EOF is
  absent and reads as background; it is not a refusal (R6). *(Decided during implementation.)*
- A tiny legacy JP2 file (encoded with OpenJPEG at generation time) with two
  channels and one codestream missing.

Unit tests on `nd2::Variant`, `nd2::Experiment` and `nd2::Attributes`
additionally run on the loop trees and attributes recorded in
`validation/inventory/*.json`.

### 8.2 Corpus (`ND2Corpus*`)

The 64 files are copied to `$SLIDEIO_IMAGES_PATH/nd2/`. Every corpus suite is
named `ND2Corpus*`; CI runs `slideio_nd2_tests --gtest_filter=-ND2Corpus*`.

- **Pixels.** As for LIF: a one-off script turns `bioformats-regions.tsv`
  (and `bioformats-relocated.tsv`) into a CRC-32 reference file after
  confirming each SHA-256, committed in `src/tests/nd2/`. For the 4 × RGB file
  the reference comes from the regions where `nd2spec` and `nd2` agree, not from
  Bio-Formats.
  Legacy regions are compared after undoing Bio-Formats' big-endian, planar
  convention.
- Scene count, size, channels, Z/T against the same TSV, with the recorded
  exceptions asserted explicitly: VPA002's channels and
  `b16_pdtB+y50__crop.nd2` T 57 against 56 (R26: all 684 frames are present;
  nd2 and the file's text agree on 57).
- `290110.tranMgc005cr.nd2` fails to open with the attributes message.
- Resolution, magnification and T increment against Bio-Formats, recorded by
  `Nd2Scenes.java`. Final results: 3117 regions match; T 62 of 63;
  magnification 46 of 46; T step 27 of 29; the remainder are the recorded
  differences of §6.3. *(Decided during implementation.)*
- Plane timestamps against `nd2`'s frame metadata on two time-lapse files.
- `TestTools::concurrentReadIdentityTestAllScenes`; an edge read is background;
  a downscaled read of the 23641² slide stays within the read budget
  (`ReadStats`).

## 9. Integration checklist

1. `src/slideio/drivers/nd2/`: sources, `CMakeLists.txt` and `conanfile.py`
   modelled on lif plus `zlib/1.3.1` and `slideio-imagetools`,
   `nd2_api_def.hpp` (`SLIDEIO_ND2_API`).
2. `src/slideio/drivers/CMakeLists.txt`: `add_subdirectory(nd2)`.
3. Root `CMakeLists.txt`: `ND2_DRIVER_NAME`, `NAME_TOOL_LIB_LIST`,
   `SLIDEIO_DRIVER_LIBS`.
4. `src/slideio/slideio/CMakeLists.txt`: link the library.
5. `imagedrivermanager.cpp`: include, `make_shared`, `"ND2"` before `"GDAL"`.
6. `auxfiles/package-smoke/main.cpp`: `kExpectedDrivers`.
7. `cmake-scripts/packaging.cmake`: Debian description format list.
8. Docs: `README.md` format table; `CLAUDE.md` (16 formats, 15 driver
   libraries, format list, source tree, test executables, the `ND2Corpus*`
   filter and the fixtures directory); `software-docs/TEST_IMAGES.md`
   regenerated.
9. Tests: `src/tests/nd2/` (`slideio_nd2_tests`), `src/tests/CMakeLists.txt`,
   rows in `test_imagedrivermanager.cpp`, `test_scene_index.cpp`,
   `test_scene_lifetime.cpp`, `test_edge_reads.cpp`, and the `BINARY` map in
   `auxfiles/list-test-images.py`.
10. CI: `slideio_nd2_tests --gtest_filter=-ND2Corpus*` in
    `build-validation.yml` and `release.yml`.
11. Python bindings: nothing driver-specific; confirm `ND2` in
    `get_driver_ids()`.

## 10. Implementation order

1. Shared tiled area downscale extracted from LIF (§7), LIF tests green.
2. `nd2::Variant` (both decoders) and `nd2::ChunkFile` with recovery.
3. Attributes, channels, experiment, frame index — the pure core.
4. Chunked scene and read path (raw, then zlib), metadata, timestamps.
5. Legacy JPEG 2000 variant.
6. Corpus tests and integration checklist.

## 11. Follow-on

- Lossy (`eCompression = 1`) and tiled frames, when a sample appears.
- Custom loops and NE sub-loop dimensions: needs the same fifth-dimension
  decision as LIF's λ-scans.
- Per-frame stage positions (`CustomData|X/Y/Z!`) as an API.
- `dPosX`/`dPosY` list-style positions in Ver2.x chunked files: scene names and
  scene raw metadata (§4.6); share the legacy reader's code.
- Emission wavelength as a channel attribute, once a source field is identified.
- Legacy reads decode every channel's whole codestream per read (once per read
  since the cursors), which makes the 75 s corpus regions test; OpenJPEG's
  decode-area or reduced-resolution decoding would cut both time and the
  whole-frame memory a legacy cursor holds.
- Real lossy or tiled samples, to test those paths instead of refusing them.
- Binary mask layers as auxiliary images.
- A frame cache for zlib/legacy frames if real workloads read them repeatedly.
- Moving the format description into `software-docs/`. Its two errors are
  corrected in place (2026-11-13): FD §11.3 (4_2_1 has 6 to 10 flagged points,
  not all 11) and FD §4.1 (type 76 is a 2-byte header plus 10 skipped bytes, zlib at offset 12).
