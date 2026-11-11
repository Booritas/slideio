# LIF test fixtures

`generate.py` writes the synthetic Leica LIF files the LIF driver tests read
(design: `software-docs/specs/2026-11-11-lif-driver-design.md`). Stdlib only, shares
no code with the C++ driver, and is deterministic.

```bash
py -3 auxfiles/lif-fixtures/generate.py            # writes src/tests/lif/data/
py -3 auxfiles/lif-fixtures/generate.py <out-dir>  # or elsewhere
```

The files are committed, about 93 KB in all. They are exact byte sequences
(UTF-16LE XML, binary block framing, deliberate garbage), so `.gitattributes`
marks `src/tests/lif/data/**` as `-text`: autocrlf must never touch them.

## Pixel function

Every test relies on it; the C++ side mirrors it.

```text
v = x + 3*y + 17*c + 31*z + 59*t + 97*m + 7*salt
uint types: v mod 2^bits          float types: v + 0.5
```

`c` is the stored channel index (description order), `salt` the image's 0-based
index in its file (document order), `bits` the storage width (not the declared
`Resolution`). Each axis uses its own identity: in an XZ image the second raster
axis is `z` and `y` is 0; in an XT image it is `t`. For interleaved images `c` is
the sample index within the pixel. Axes the function has no term for (DimID 5, 9)
contribute 0.

## Layout

Header: `0x70`, `2*N+5`, `0x2A`, `N`, UTF-16LE XML. Block: `0x70`, `0`, `0x2A`,
length (u32 for `Version="1"`, u64 otherwise), `0x2A`, id length, UTF-16LE id,
payload. Then a u32 `0` trailer. The XML is `LMSDataContainerHeader` >
`Element Name="Fixture"` > `Children` > image `Element`s with a sibling
`Memory MemoryBlockID Size`. Default image: 16x12 uint8, one channel, X
`Length=1.5e-5` m, Y `Length=1.1e-5` m.

Block ids: `MemBlock_<salt>` for an image, `MemBlock_empty` (zero length),
`MemBlock_extra` (64 bytes, referenced by nothing), `MemBlock_missing` (named by
`broken_refs`' `Missing`, written nowhere).

Channels carry `ChannelTag="0"`, `DataType`, `Resolution`, `Min`, `Max`, `Unit`,
`LUTName`, `NameOfMeasuredQuantity`, `IsLUTInverted`, `BytesInc`, `BitInc`, and
dimensions `BitInc`, because real files do and liffile refuses a channel
without them. The driver reads none of the extras.

## Fixtures

| File | Exercises |
|---|---|
| `v2_basic` | `Plain`; an empty block before it, an unreferenced 64-byte block after; `HardwareSetting` attachment with `ATLConfocalSettingDefinition Magnification="63"` |
| `v1_basic` | the same with `Version="1"`, four-byte lengths |
| `v3_probe` | the same with `Version="3"`, eight-byte lengths (liffile rejects the version) |
| `order_by_id` | `A` (salt 0), `B` (salt 1); blocks written B then A |
| `nesting` | folder `Nest` with `CZMT`, `ZCMT`, `MCZT`, `CZTM`: X=8 Y=6 C=2 Z=3 M=4 T=2 uint16, strides nest in the order the name gives, innermost first; Z `Length=4e-6` m; T `Unit="s" Length=10`; M `Length=0 Unit=""` |
| `types` | `U8`; `U16_12bit` (`Resolution=12`); `U32`; `F16`; `F32`; `F64`; `NoDataType16` (no `DataType`, X stride 2); `Mismatch` (`DataType=1 Resolution=32`, X stride 2) |
| `rgb` | 8x6 interleaved uint8: `TagRGB` (tags 1,2,3 at BytesInc 0,1,2), `TagBGR` (3,2,1), `Untagged` (no `ChannelTag`) |
| `xzxt` | `XZ` (dims 1, 3; Z N=5), `XT` (dims 1, 4; T N=7) |
| `extra_axes` | `Extra1` (DimID 5, N=1), `Lambda` (DimID 9, N=12), `Plain` |
| `all_unsupported` | `Lambda` only |
| `padded` | `Padded`: Y stride 16 + 4 |
| `bad_strides` | `Alias` (Z stride 100 < plane extent 192), `OOB` (T stride 1000, payload holds two planes), `Plain` |
| `broken_refs` | `DupRef` (its id on two blocks), `Missing` (no such block), `SizeMismatch` (`Memory/@Size` off by one), `Plain` |
| `truncated` | `Good`, `Cut` (final block declares 1000 bytes more than the file holds, `Memory/@Size` is the real 192; no trailer) |
| `unicode` | `Zellkern µ – 細胞` |
| `timestamps` | `Hex` (C=2, T=3, `TimeStampList NumberOfTimeStamps` hex), `Decimal` (`TimeStamp HighInteger/LowInteger`), `Short` (five stamps for six planes). C is innermost, storage index `k = c + 2*t`, tick of plane `k` = `131000000000000000 + k*10000000` (FILETIME) |
| `dup_names` | `Same` (salt 0), `Same` (salt 1) |
| `trailer_garbage` | `Plain`, then a zero trailer followed by `FF FF` |
| `lof` | header + XML root `LMS_Object_File`, no blocks |
| `not_lif` | 64 bytes of `0x41` |

## Cross-check with liffile

liffile 2026.11.8 reads every fixture except `not_lif` and `v3_probe` (by
design), `types/NoDataType16` and `rgb/Untagged` (it requires `DataType` /
`ChannelTag` on every channel), and `types/Mismatch` (its payload is smaller than
the declared float32 array). It reproduces the pixel function on `nesting` and
`types`, and the `Hex` and `Decimal` timestamps agree with each other.
