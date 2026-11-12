# ND2 test fixtures

`generate.py` writes the synthetic Nikon ND2 files the ND2 driver tests read (design:
`software-docs/specs/2026-11-12-nd2-driver-design.md`). It was written from
`Nikon_ND2_Format.md`, shares no code with the C++ driver or with the validation parser,
and is deterministic (no timestamps, no randomness).

```bash
py -3 auxfiles/nd2-fixtures/generate.py            # writes src/tests/nd2/data/
py -3 auxfiles/nd2-fixtures/generate.py <out-dir>  # or elsewhere
```

The chunked files need only the standard library. `legacy.nd2` and `legacy_xy.nd2` encode
JPEG 2000 codestreams with `imagecodecs` (lossless: `jpeg2k_encode(level=0, reversible=True,
codecformat='J2K')`) and so also need `numpy`; without them the script prints a note and
leaves those two files alone. The files are committed, so CI never runs the generator.
Run it from an environment that has both to regenerate the full set of 23.

The files are committed, about 970 KB in all (every aligned frame chunk is 8 KB: the
4072-byte name pushes the pixels onto the second 4096-byte page). They are exact byte
sequences (little-endian binary framing, zero padding, `0xEE` row padding, a raw `\x01`
inside XML), so `.gitattributes` marks `src/tests/nd2/data/**` as `-text`: autocrlf must
never touch them.

## Pixel function

Every test relies on it; the C++ side mirrors it.

```text
v = x + 3*y + 17*k + 31*frame + 7*salt         k = component index within the frame
uint types: v mod 2^bits                       float32: v + 0.5
```

`frame` is the ND2 frame index (`ImageDataSeq|frame!`), `salt` is 0 in every file. Row
padding (`uiWidthBytes` beyond the packed width) is `0xEE`, so a read that touches it is
visible. For the legacy files `frame` is the frame number and `k` the channel.

## Defaults

16x12, uint8, 1 channel, `dCalibration 0.5` (`bCalibrated` true), `dObjectiveMag 20`,
`dTimeAbsolute 2460000.5`, channel `sDescription "DAPI"`, `uiColor 0x0000FF`,
`eCompression 2` (none). Frame `N` carries the float64 time `N*100.0` ms in its 8-byte
prefix and in `CustomData|AcqTimesCache!` (one entry per frame position up to the highest
frame present; the chunk is absent from `v20_unpadded` and `v20_zlib`, whose times live only in
the prefixes). Frames are numbered over the **valid** XY points only: `v3_tpz` has 3
configured points with `pItemValid` 1,0,1, so 3 x 2 x 4 = 24 frames and
`frame = (t*2 + p)*4 + z` with `p` the ordinal among valid points.

Layout per FD section 3: signature chunk (`nameLength 32`, `dataLength 64`, `VerM.m`),
`ImageAttributes[LV]!`, `ImageMetadata[LV]!`, `ImageMetadataSeq[LV]|0!`, optional
`ImageTextInfo[LV]!`, the frames, `CustomData|AcqTimesCache!`, and last the map chunk, whose
final 40 bytes (the terminating entry) are the trailer. Metadata is CLX Lite for Ver3 and
CLX XML for Ver2, each wrapped (`SLxImageAttributes` and so on / `no_name`). "Aligned" means
every chunk starts on a 4096 boundary and frames use `nameLength 4072`; non-frame chunks keep
their bare name as `nameLength` (no padding bytes after the `!`). "Unpadded" means
`nameLength` is the bare name length and chunks are packed back to back.

## Files

| File | Contents |
|---|---|
| `v3_basic.nd2` | Ver3.0 aligned, no loops (the experiment holds one time loop of count 0), 1 frame |
| `v3_tpz.nd2` | Ver3.0 aligned; loops T3, XY (3 points, `pItemValid` 1,0,1, `dPosName` "A","skip","A"), Z4 `dZStep 1.5`; T `dPeriod 2000`; uint16 `uiBpcSignificant 12`; 2 channels "DAPI" `0x0000FF`, "GFP" `0x00FF00`; 24 frames; `ImageMetadataSeqLV\|0!` is a type-76 compressed tree; has `ImageTextInfoLV!` |
| `order_pz.nd2` | Ver3.0; XY (2 points, empty names), Z3; 6 frames |
| `order_zt.nd2` | Ver3.0; Z2 outer, T3 inner; 6 frames |
| `v21_xml.nd2` | Ver2.1 XML, chunks **not** 4096-aligned, frames with 4072 names; T2, Z3 with `uLoopPars` wrapped in `i0000000000`; RGB 15x5 uint8 (`uiComp 3`, 1 channel, `uiWidthBytes 48`); a raw `\x01` inside `TextInfoItem_5` (invalid XML 1.0 on purpose) |
| `v20_unpadded.nd2` | Ver2.0 XML, unpadded names, uncompressed; Z3; uint16 7x4 (`uiWidthBytes 16`); **no** `AcqTimesCache` |
| `v20_zlib.nd2` | as `v20_unpadded` with `eCompression 0`, frames zlib (`78 01`) |
| `stale_map.nd2` | Ver3.0 aligned; Z5; map entries of frames 1 and 3 point 4096 bytes too early |
| `frames_missing.nd2` | Ver3.0; T5 outer, Z2 (`uiSequenceCount` 10); frames 0-5 present except 3 |
| `frames_surplus.nd2` | Ver3.0; Z3 (`uiSequenceCount` 3); frames 0-4 |
| `ne_time.nd2` | Ver3.0; NE time loop (2 periods, `pPeriodValid` 1,0, counts 2 and 5) > spectral loop (`eType 6`) > Z2; 4 frames |
| `float32.nd2` | Ver3.0; 1 frame, `uiBpcInMemory 32` |
| `multi_rgb.nd2` | Ver3.0; 6x4 uint8, 4 channels x RGB (`uiComp 12`, `uiWidthBytes 72`), names "BF1".."BF4" |
| `truncated_frame.nd2` | Ver3.0; Z3; `AcqTimesCache` precedes the frames so frame 2 is the last chunk before the map; frame 2's `dataLength` is the real bytes up to EOF plus 1000. The map chunk and the trailer still end the file; the map lists frames 0, 1, 2 |
| `bad_width.nd2`, `lossy.nd2`, `tiled.nd2`, `custom_loop.nd2`, `two_time_loops.nd2`, `bad_comp.nd2` | refusals: `uiWidth 0` (`uiWidthBytes 0`); `eCompression 1`; `uiTileWidth 8`; `eType 7` count 3; T2 > T3; 3 channels with `uiComp 4`. One frame each (three for `custom_loop`) |
| `not_nd2.nd2` | 64 bytes of `0x41` |
| `legacy.nd2` | JP2 legacy per FD section 11; 8x6 uint16, 2 channels, T3; codestreams for frames 0-1 both channels, frame 2 channel 0 only (5 `jp2c`); `TINF` `Dimensions: T(3) x λ(2)`; `VIMD` `PicturePlanes Count 2`; `VCAL`/`VIMD` calibration 0.25; `AIM1` has a type-8 time loop (count 3) > type-6 spectral loop |
| `legacy_xy.nd2` | JP2 legacy; 8x6 uint8, 1 channel; `AIM1` loops T2 (type 8) > XY `Count 4` all valid; `TINF` `Dimensions: T(2) x XY(2)` (the text disagrees with `Count` on purpose); 4 `jp2c` |
| `big.nd2` | not written by the generator; see the sparse-file test |

Legacy layout: `jP  `, `ftyp`, `jp2h` (`ihdr` + `colr`), then per frame its `jp2c` boxes (raw
`FF4F FF51` codestreams, one per channel) followed by `VIMD` and `VCAL` `xml ` boxes, then `AIM1`,
`TINF`, and a final `uuid` box (`PMAP`) holding the box map and the 40-byte trailer. Every box
is in the map; walking the boxes reaches EOF exactly. Each `xml ` payload is a LIM comment
followed by a CLX XML `<variant>`.

## Details a reader must handle

- CLX Lite level records: `indexOffset` is measured from the record's type byte; the index
  table holds the children's offsets measured from the same byte.
- Type-76 compressed tree (`v3_tpz` picture metadata): bytes `4C 00`, an 8-byte little-endian
  inflated length and 2 zero bytes (12 bytes in all), then a zlib stream.
- `v21_xml`'s `\x01` makes the metadata chunk invalid XML: strip control characters before parsing.

## Fields beyond the brief

Added because the independent readers need them (`nd2` 0.12 refused to build channel metadata
without them): every picture plane carries a non-empty `pFluorescentProbe` (`m_sName`), and
`dRefractIndex1 1.0`, `dObjectiveNA 0.75`, `wsObjectiveName "Plan Apo 20x"`, `dZoom 1.0`,
`dProjectiveMag -1.0`, `dAspect 1.0` and zero stage positions are present everywhere. Loops carry
`uiRepeatCount` and the usual `dStart`/`dPeriod`/`dDuration` time fields; Z loops carry
`dZLow`/`dZHigh`/`dZHome`/`dZStep`/`iType 0`; XY loops carry `dPosX`/`dPosY`/`dPosZ` of
(1000*i, -500*i, 10*i) per point.

## Known reader quirks these files expose

- `nd2` trusts the map, so it reads `stale_map` frames 1 and 3 from the wrong place, and reads
  `v20_unpadded` wrongly (its fast path assumes `nameLength 4072`; see FD section 3.4).
- `nd2` reports `legacy_xy` with 4 positions (it ignores the `TINF` count), and cannot parse
  `v21_xml`'s text info (the `\x01`).

## Corpus references

The `ND2Corpus*` suites (`src/tests/nd2/test_nd2_corpus.cpp`) compare the driver with four
committed references in `src/tests/nd2/data/`, made from the 64 corpus files under
`$SLIDEIO_IMAGES_PATH/nd2/` and the ND2 validation folder of the documents repository
(`image_formats/nd2/validation`). Both generators write rows in a fixed order with LF line
endings.

```bash
# nd2-regions.tsv: 3,117 rows, CRC-32 per slideio channel of the 7x5 regions Bio-Formats read;
# also nd2-loops.tsv (55 chunked files' inventory loops) and nd2-times.tsv (2,246 frame
# times of 190821_L1_IRS-Akt_CS2003.nd2 and B08.nd2). Needs nd2, imagecodecs and numpy
# (the environment of the validation README).
nd2env/Scripts/python -I auxfiles/nd2-fixtures/make_region_reference.py $SLIDEIO_IMAGES_PATH/nd2 <validation-dir> src/tests/nd2/data/nd2-regions.tsv
# nd2-scenes.tsv: 248 rows, one per Bio-Formats series of the 63 files it reads validly.
java -cp bioformats_package.jar auxfiles/nd2-fixtures/Nd2Scenes.java $SLIDEIO_IMAGES_PATH/nd2 src/tests/nd2/data/nd2-scenes.tsv
```

`make_region_reference.py` maps each Bio-Formats (series, plane) to a frame exactly as the
validation's `compare_regions.py` (chunked, through `nd2spec`) and `compare_nd2lib.py`
(legacy JPEG 2000, through `nd2`, undoing Bio-Formats' big-endian and planar-RGB output) do,
and writes nothing unless every SHA-256 equals Bio-Formats'. It prints
`3057 regions, 0 unexpected mismatches, 12 VPA002 regions referenced from nd2`. For
`2D 500uM VPA002.nd2`, where Bio-Formats reads 4 x RGB with a 4-sample stride, it checks
that Bio-Formats' bytes are exactly that misaddressing of the right frame, then takes each of
the three components of the channel from where `nd2spec` and `nd2` agree. The 18 rows of
`290110.tranMgc005cr.nd2` (`uiWidth` 0, refused) are skipped.

`Nd2Scenes.java` writes Bio-Formats' size, type and calibration per series. Bio-Formats sets
no time increment and no nominal magnification for ND2, so its `tIncrement_s` is the mean
step of the plane times and its `magnification` the calibrated one (parsed from the objective
name); the test records where the driver differs.
