# Nikon ND2 reader driver design

Date: 2026-11-12. Status: proposed. Points marked *(my call)* are design
decisions taken without a requirement behind them; review them first.

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
  offset 16 and `ND2 CHUNK MAP SIGNATURE 0000001!` at `size − 40` (FD §2).
- `00 00 00 0C 6A 50 20 20 0D 0A 87 0A` → legacy; also require
  `LABORATORY IMAGING ND BOX MAP 00` at `size − 40`.
- Otherwise `ND2: not a Nikon ND2 file (<reason>)`.

All errors raised while opening are normalised at `ND2Slide::openFile` to
start with `ND2: ` and end with `(file: <path>)`, as LIF does.

### 4.2 Chunked container

- Version from the signature chunk's data (`VerM.m`). Major 3 reads the `…LV`
  chunks; major 2 prefers the XML chunks and falls back to `…LV` when a 2.x file
  has only those (FD §3.2). Any other major is refused.
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
  EOF is treated as absent.

### 4.3 Metadata decoding

`nd2::Variant` is a tree of named nodes holding bool, int64, uint64, double,
string (UTF-8), byte array, or children. Both decoders produce it (FD §4):

- **CLX Lite**: records as FD §4.1; level `indexOffset` used only to skip the
  index table; empty-named siblings become a list; type 76 inflates with zlib
  and recurses. A byte array is decoded as a nested tree only on demand by the
  caller that expects one (JOBS), never speculatively. Every length is checked
  against the remaining buffer; a decode error names the chunk.
- **CLX XML**: tinyxml2 (no external entities). Before parsing, replace control
  characters other than tab/CR/LF with spaces (Bio-Formats does this; some files
  contain them). `runtype` selects the scalar type; `no_name` and the FD's list
  names (`BinaryItem`, `TextInfoItem`, `Wavelength`, `MinSrc`, `MaxSrc`) become
  list elements. Numbers are parsed with `.` as the decimal point whatever the
  locale (the LIF PR #96 lesson).
- The `SLx…` wrapper level (binary) or single `no_name` child (XML) is unwrapped.
- Field lookup tolerates the 2.x quirk of `uLoopPars` wrapped in
  `i0000000000` (FD §7.1).

### 4.4 Attributes, channels, experiment

- **Attributes** (FD §5): require `uiWidth > 0`, `uiHeight > 0`, `uiComp > 0`,
  `uiBpcInMemory ∈ {8, 16, 32}`, `uiWidthBytes ≥ uiWidth·uiComp·bytes`,
  `eCompression ∈ {absent, 0, 2}`, tile size absent or equal to the frame.
  Anything else: the file fails to open with the reason. 8 → `DT_Byte`,
  16 → `DT_UInt16`, 32 → `DT_Float32`.
- **Channels** (FD §6.2): `sPicturePlanes.uiCount` channels,
  `componentsPerChannel = uiComp / uiCount`, which must divide exactly. Records
  from `sPlaneNew`, else `sPlane`, in component order. If picture metadata is
  missing, treat as `uiComp` single-component channels with no names
  *(my call)*.
- **Experiment** (FD §7.2): flatten the loop tree exactly as FD §7.2 rules 1–4,
  **without** `nd2`'s `pSubLoops` special case. XY count = points whose
  `pItemValid` flag is non-zero (FD §7.3); NE time count = sum of valid periods.
  Then validate:
  - at most one time loop (types 1 and 8 both map to T), one XY loop, one Z loop;
  - a custom loop (`eType 7`) with count > 1 is refused;
  - spectral loops never appear (rule 2 removes them).

  A refusal fails the open with the loop list in the message — unlike LIF there
  is no per-image fallback, because all positions share one loop tree.

### 4.5 Frames present, and the T count

The set of verified `ImageDataSeq|N!` chunks is the authority for which frames
exist (FD §7.4). Let the loop product be `L`:

- Frames with `N ≥ L` are ignored (logged once at INFO with a count).
- Missing frames with `N < L` read as background (§7).
- **T trimming.** When the time loop is the outermost loop and the last time
  points have no frames at all, T is reduced to the last time point with at
  least one present frame. This is `ceil((maxPresentIndex + 1) / innerProduct)`
  from FD §7.4, and gives Bio-Formats' T = 81 for `B08.nd2`. When T is not
  outermost, no trimming. *(My call; FD §7.4 leaves the choice to the API.)*
- `uiSequenceCount` is not used for counts; it is reported as metadata only.

### 4.6 Scenes

One `ND2Scene` per acquired XY position, in loop order; one scene when there is
no XY loop. This matches Bio-Formats' series and the LIF tile-field rule, so a
scene index equals a Bio-Formats series index.

Scene name: the point's `dPosName` when non-empty, else `Position <k+1>` with
`k` the index among valid points; a file without an XY loop has one scene named
after the file stem. *(My call on spelling.)* Duplicate point names get
` (<k+1>)` appended so names stay unique.

### 4.7 Legacy JPEG 2000 variant

Following FD §11:

- Walk the boxes from offset 0 (big-endian headers, 64-bit extended lengths)
  and read the box map from the trailer; require that the map's offsets each
  point at a box start. *(The walk is the authority if they disagree; FD §11.1
  shows they never do.)*
- `jp2h/ihdr` gives width, height, components and bit depth (8 or 16 unsigned).
- XML boxes by LIM type: `VIMD` (frame 0) for channels, `ACAL`/`VCAL` for
  calibration, `AIM1`/`AIMD` for loops, `TINF` for text. These are parsed with
  the §4.3 XML decoder.
- **Dimensions.** Loops from `AIM1`/`AIMD` as in §4.4, but the XY count comes
  from the `TINF` `Dimensions:` line when it has an `XY(n)` term, which
  overrides the loop's count (FD §11.3; gives 5, not 11, for
  `4_2_1_cont_NoMR001.nd2`). Channels from `VIMD.PicturePlanes.Count`, never
  from the spectral loop.
- Frame `i`, channel `c` is the `(i·C + c)`-th `jp2c` box; RGB (`ihdr` 3
  components) has one codestream per frame. A missing codestream reads as
  background, so `b16_14_12.nd2`'s last frame has an empty second channel.
  T trimming (§4.5) then drops nothing for that file, giving T = 51 where
  Bio-Formats reports 50. *(My call: the first channel of frame 51 exists, so
  hiding it loses data; the corpus test records the difference rather than
  hiding it.)*
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
| Magnification | `dObjectiveMag` when > 0, else 0 |

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
  optics/probe fields present on the plane record (`pFluorescentProbe` name,
  emission wavelength), kept verbatim as strings.

### 6.3 Calibration

`slideio::Resolution` is in metres.

- X, Y: `dCalibration` µm × 1e-6 when `bCalibrated` is true and the value > 0,
  else 0. `dAspect ≠ 1` scales Y *(to be confirmed against Bio-Formats on the
  corpus; no sample is known to have it)*.
- Z: `|dZStep|`; if 0 and count > 1, `|dZHigh − dZLow| / (count − 1)` (FD §8).
- T: the time loop's `dPeriod` ms ÷ 1000 when > 0; else the mean difference of
  consecutive per-T-frame times from `AcqTimesCache` when complete; else 0.
  *(My call: the corpus test compares both with Bio-Formats and records which
  matches.)*
- Legacy: the same fields from `ACAL`/`VCAL`/`AIM1` (FD §11.2).

### 6.4 Timestamps

Per-frame time is `CustomData|AcqTimesCache!` (float64 ms, indexed by frame
index), or, if that chunk is absent, each frame's 8-byte prefix (FD §6; equal
in all 53 samples that have both).

- `hasPlaneTimestamps()` is true when every frame of the scene that exists has
  a time and none is NaN. Missing frames return the scene's acquisition offset
  0. All channels of a frame share its time.
- `getAcquisitionTime()`: `dTimeAbsolute` (Julian day; unknown when < 1)
  converted to Unix seconds, plus the scene's earliest frame time, floored to a
  whole second. `getPlaneTimestamp(t, c, z)` is the offset from that floored
  value, so none is negative and `acquisition + offset` is exact — the LIF
  convention.
- No absolute time → `getAcquisitionTime()` 0 and plane timestamps relative to
  the experiment start.

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
   - **zlib**: inflate the frame as a stream into a one-row buffer, discarding
     rows before `y` and stopping after `y+h−1`. No cache. *(My call: only two
     tiny Ver2.0 files use it; correctness over speed.)*
   - **Legacy**: decode the channel's codestream with
     `ImageTools::decodeJp2KStream` and crop. No cache.
4. Resize to the requested size with the helper other drivers use.

**Large downscaled reads.** The 23641² frame is 2.2 GB of uint16 × 2. A
downscaled read whose source exceeds the budget must be read in strips and area
filtered, exactly the problem LIF solved (LIF design §7). *(My call: move LIF's
tiled `INTER_AREA` downscale out of `lifscene.cpp` into a shared helper in
slideio-core — e.g. `slideio::TiledAreaResampler`, taking a row-strip reader
callback and `ReadLimits` — and use it from both drivers, rather than copying
it. That refactor is a separate first step with LIF's existing tests as its
safety net. It applies to uncompressed chunked frames; zlib and legacy frames
are small.)*

**Concurrency.** The only file state is the slide's cursor-free `FileReader`;
everything parsed at open is immutable; zlib streams and decode buffers are
local to a call. So `supportsConcurrentReads() == true` with no `ContextPool`,
the CZI/ZVI/LIF shape. No `thread_local` holds a handle.

Samples are little-endian (chunked); on a big-endian host they would be
swapped. slideio does not build for one; this is a guard, not a tested path.

## 8. Validation

### 8.1 Corpus-free (CI)

Synthetic files from `auxfiles/nd2-fixtures/generate.py`, sharing no code with
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
- Refusals: `uiWidth = 0`, lossy compression, tiled frames, a custom loop of 3,
  two time loops, a truncated frame chunk.
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
  the reference comes from `nd2` (`nd2lib-summary.json` path), not Bio-Formats.
  Legacy regions are compared after undoing Bio-Formats' big-endian, planar
  convention.
- Scene count, names, size, channels, Z/T against the same TSV, with the
  recorded exceptions (`b16_14_12.nd2` T, §4.7).
- `290110.tranMgc005cr.nd2` fails to open with the attributes message.
- Resolution, magnification and T increment against Bio-Formats (extend
  `Nd2Check.java` to record them).
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
- Binary mask layers as auxiliary images.
- A frame cache for zlib/legacy frames if real workloads read them repeatedly.
- Moving the format description into `software-docs/`.
