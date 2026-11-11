# Leica LIF Driver Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** A new `slideio-lif` driver (id `LIF`) that opens Leica `.lif` projects, one scene per image field, with plane addressing taken from the XML byte strides.

**Architecture:** Three pure layers — `lif::Container` (binary header, XML, memory-block index), `lif::parseImages` (XML → `ImageDescription`), `lif::Layout` (validated axis model and offset function) — under a thin `LIFSlide`/`LIFScene` pair that reads rows with `FileReader::readAt`. Tests run on synthetic LIF files written by a standalone Python generator; corpus suites compare 9,264 regions with Bio-Formats.

**Tech Stack:** C++17, tinyxml2, OpenCV, slideio-core (`FileReader`, `Tools::fromUnicode16`, `readClampedBlock`), Google Test, Python 3 (stdlib only) for fixtures.

**Spec:** `software-docs/specs/2026-11-11-lif-driver-design.md` (cited as *D §n*). Format description: `D:/Projects/slideio/documents/image_formats/lif/Leica_LIF_Format.md` (*FD §n*).

## Global Constraints

- Driver id `LIF`; file spec `*.lif`; `canOpenFile` does no I/O; `"LIF"` goes in `driverOrder` immediately before `"GDAL"`.
- Library `slideio-lif` in `src/slideio/drivers/lif/`; namespace `slideio::lif` for internals; export macro `SLIDEIO_LIF_EXPORTS`, defined by `SLIDEIO_LIF_API`.
- Every error raised while opening starts with `LIF: `; a not-LIF file gives `LIF: not a Leica LIF file (<reason>)`.
- Offsets and sizes are `uint64_t` with checked arithmetic; overflow is an error, never a wrap.
- Memory blocks are attached by `MemoryBlockID` only — no positional matching.
- An unsupported image is skipped with a WARNING `LIF: image "<path>" skipped: <reason>`; the open fails only if no image is supported.
- `.lifext`, stitching, extra axes with more than one element, Bio-Formats' padding heuristic and truncated-payload recovery are out of scope (D §1.2).
- `supportsConcurrentReads()` returns `true`; no `ContextPool`, no `thread_local`.
- Corpus suites are named `LIFCorpus*`; CI runs `slideio_lif_tests --gtest_filter=-LIFCorpus*`.
- `src/tests/lif/data/` is marked `-text` in `.gitattributes`.
- Every commit in this plan goes on a feature branch `feature/lif-driver`, never `master`.

## Review Focus

1. **Offsets above 4 GiB.** AMR1 is 3 GB; the next file will be bigger. A payload starting past `2^32` must address correctly — pinned in Task 4 (`LifLayout.OffsetsAbove4GiB`).
2. **Downscaled read of a large image.** A thumbnail of a merged mosaic must not allocate the full-resolution source at once and must equal the unbanded result — pinned in Task 6 (`LifScene.BandedResampleEqualsUnbanded`).
3. **Channel subsets in any order**, e.g. `{2, 0}` on interleaved RGB, must return blue-then-red planes — pinned in Task 6 (`LifScene.ChannelSubsetOrder`).
4. **Releasing the file.** After slide and scenes are destroyed, Windows must let the user delete the `.lif` — pinned in Task 6 (`LifScene.FileReleasedAfterClose`).
5. **Two images with the same name** (LAS X allows it). Both must appear as scenes, in document order, with the name unchanged; `getSceneByName` returns the first — pinned in Task 6 (`LifSlide.DuplicateNamesKept`).

---

## File Structure

```
auxfiles/lif-fixtures/
  generate.py                 # writes src/tests/lif/data/*.lif; shares no code with the driver
  make_region_reference.py    # one-off: Bio-Formats SHA-256 regions -> CRC-32 reference
  LifScenes.java              # one-off: Bio-Formats series sizes and physical sizes
  README.md
src/slideio/drivers/lif/
  lif_api_def.hpp
  lifcontainer.hpp/.cpp       # header, XML decode, block index        (D §4.1–4.2)
  lifxml.hpp/.cpp             # ImageDescription extraction             (D §4.3)
  liflayout.hpp/.cpp          # axis model, type, offsets, calibration  (D §5, §6.2)
  liftimestamps.hpp/.cpp      # tick parsing, plane times               (D §6.3)
  lifimagedriver.hpp/.cpp
  lifslide.hpp/.cpp           # scene list, skipping                    (D §4.4–4.5)
  lifscene.hpp/.cpp           # properties, read path                   (D §6, §7)
  CMakeLists.txt, conanfile.py
src/tests/lif/
  CMakeLists.txt, conanfile.txt
  lif_fixture_values.hpp      # expected pixel function, mirrors generate.py
  test_lif_container.cpp, test_lif_xml.cpp, test_lif_layout.cpp,
  test_lif_timestamps.cpp, test_lif_scene.cpp,
  test_lif_corpus.cpp
  data/                       # generated fixtures + lif-regions.tsv + lif-scenes.tsv
```

---

### Task 1: Synthetic fixture generator

**Files:**
- Create: `auxfiles/lif-fixtures/generate.py`, `auxfiles/lif-fixtures/README.md`
- Create (generated, committed): `src/tests/lif/data/*.lif`
- Modify: `.gitattributes` (add `src/tests/lif/data/** -text` next to the mrxs line)

**Interfaces:**
- Produces: the fixture files below, and the pixel function every test relies on.

Pixel function (copy exactly; Task 6 mirrors it in C++):

```text
v = x + 3*y + 17*c + 31*z + 59*t + 97*m + 7*salt
uint types: v mod 2^bits          float types: v + 0.5
```

`c` is the **stored** channel index (description order), `salt` the image's 0-based index in its file. Each axis uses its own identity: in an XZ image the second raster axis is `z` and `y` is 0; in XT it is `t`. For interleaved images `c` is the sample index within the pixel (0, 1, 2 as stored).

Container framing per FD §3–4: `0x70` (u32), u32 header field (`2*N+5`), `0x2A`, u32 char count `N`, UTF-16LE XML; each block `0x70`, u32 `0`, `0x2A`, length (u32 for Version 1, u64 otherwise), `0x2A`, u32 id length, UTF-16LE id, payload; then a u32 `0` trailer. XML root `LMSDataContainerHeader Version="…"` > `Element Name="Fixture"` (the omitted experiment element) > `Children` > image `Element`s, each `Data/Image/ImageDescription/{Channels,Dimensions}` and a sibling `Memory MemoryBlockID="MemBlock_<n>" Size="<len>"`. Default image: X=16, Y=12, uint8, one channel, X `Length=1.5e-5` m, Y `Length=1.1e-5` m.

| File | Contents (image names; scene expectations live in the tests) |
|---|---|
| `v2_basic.lif` | `Plain`; an empty block before it, an unreferenced 64-byte block after; `HardwareSetting` attachment with `ATLConfocalSettingDefinition Magnification="63"` |
| `v1_basic.lif` | as `v2_basic`, `Version="1"`, four-byte lengths |
| `v3_probe.lif` | as `v2_basic`, `Version="3"`, eight-byte lengths |
| `order_by_id.lif` | `A` (salt 0), `B` (salt 1); blocks written B then A |
| `nesting.lif` | folder `Nest` with `CZMT`, `ZCMT`, `MCZT`, `CZTM`: X=8, Y=6, C=2, Z=3, M=4, T=2, uint16; strides nest in the order the name gives, innermost first; Z `Length=4e-6` m; T `Unit="s" Length=10` |
| `types.lif` | `U8`; `U16_12bit` (`Resolution=12`); `U32` (`DataType=0 Resolution=32`); `F16` (`DataType=1 Resolution=16`); `F32`; `F64`; `NoDataType16` (no `DataType`, X stride 2); `Mismatch` (`DataType=1 Resolution=32`, X stride 2) |
| `rgb.lif` | 8×6 interleaved uint8: `TagRGB` (ChannelTag 1,2,3 at BytesInc 0,1,2), `TagBGR` (3,2,1), `Untagged` |
| `xzxt.lif` | `XZ` (dims 1, 3; Z N=5), `XT` (dims 1, 4; T N=7) |
| `extra_axes.lif` | `Extra1` (DimID 5, N=1), `Lambda` (DimID 9, N=12), `Plain` |
| `all_unsupported.lif` | `Lambda` only |
| `padded.lif` | `Padded`: Y stride `16 + 4` |
| `bad_strides.lif` | `Alias` (Z stride < plane extent), `OOB` (T stride runs past payload), `Plain` |
| `broken_refs.lif` | `DupRef` (its ID appears on two blocks), `Missing` (no such block), `SizeMismatch` (`Memory/@Size` off by one), `Plain` |
| `truncated.lif` | `Good`, `Cut` (final block declares 1000 bytes more than the file holds; no trailer) |
| `unicode.lif` | `Zellkern µ – 細胞` |
| `timestamps.lif` | `Hex` (C=2, T=3, `TimeStampList` hex), `Decimal` (same planes, `TimeStamp HighInteger/LowInteger`), `Short` (one stamp missing). Tick of storage-order plane `k` = `131000000000000000 + k*10000000` |
| `dup_names.lif` | `Same` (salt 0), `Same` (salt 1) |
| `trailer_garbage.lif` | `Plain`, then a zero trailer followed by `FF FF` |
| `lof.lif` | header + XML root `LMS_Object_File`, no blocks |
| `not_lif.lif` | 64 bytes of `0x41` |

- [ ] **Step 1: Create the branch** — `git checkout -b feature/lif-driver`.
- [ ] **Step 2: Write `generate.py`** (stdlib only; `struct`, `pathlib`); default output `src/tests/lif/data/`, overridable by argument.
- [ ] **Step 3: Run it twice and check determinism**
  Run: `py -3 auxfiles/lif-fixtures/generate.py && git status --porcelain src/tests/lif/data && py -3 auxfiles/lif-fixtures/generate.py && git diff --stat src/tests/lif/data`
  Expected: 20 new files after the first run; no diff after the second. Total under 100 KB (`du -sh src/tests/lif/data`).
- [ ] **Step 4: Cross-check one fixture with an independent reader.** With the pinned liffile from the validation README: `python -c "import sys;sys.path.insert(0,'<liffile dir>');import liffile;f=liffile.LifFile('src/tests/lif/data/nesting.lif');print([(i.name,i.sizes) for i in f.images])"`
  Expected: four images, each `T=2, M=4, Z=3, C=2, Y=6, X=8` (order may vary per image).
- [ ] **Step 5: README** — what each fixture exercises (the table above), how to regenerate, and that the files are exact bytes (hence `-text`).
- [ ] **Step 6: Commit** — `git add auxfiles/lif-fixtures src/tests/lif/data .gitattributes && git commit -m "test(lif): synthetic LIF fixture generator"`

---

### Task 2: Library scaffold and `lif::Container`

**Files:**
- Create: `src/slideio/drivers/lif/{lif_api_def.hpp, lifcontainer.hpp, lifcontainer.cpp, CMakeLists.txt, conanfile.py}`
- Create: `src/tests/lif/{CMakeLists.txt, conanfile.txt, test_lif_container.cpp}`
- Modify: `src/slideio/drivers/CMakeLists.txt` (`add_subdirectory(lif)` after `bif`), `src/tests/CMakeLists.txt` (`add_subdirectory(lif)` after `bif`), root `CMakeLists.txt` (`set(LIF_DRIVER_NAME "${LIB_NAME_PREFIX}lif")` after the `BIF_DRIVER_NAME` line)

Model `CMakeLists.txt` on `drivers/bif/CMakeLists.txt` minus TIFF/JPEG/imagetools (links `opencv::opencv`, `tinyxml2::tinyxml2`, `${CORE_LIB_NAME}`). `conanfile.py`: `opencv/4.14.0`, `tinyxml2/9.0.0`. Test `CMakeLists.txt` as `tests/bif`, plus `target_compile_definitions(... LIF_TEST_DATA_DIR="${CMAKE_CURRENT_SOURCE_DIR}/data")`. Then generate the two modules' conan `cmake/` dirs for release and debug (memory: per-module conanfiles).

**Interfaces:**
- Consumes: `slideio::FileReader` (`readAt`, `size`), `Tools::fromUnicode16`.
- Produces (`lifcontainer.hpp`):

```cpp
namespace slideio::lif {
struct MemoryBlock {
    std::string id;
    uint64_t offset = 0;      // payload start
    uint64_t length = 0;      // payload length
    bool truncated = false;   // declared payload runs past EOF
    int occurrences = 1;      // >1: the id is ambiguous
};
class SLIDEIO_LIF_EXPORTS Container {
public:
    static Container read(const FileReader& file);   // throws RuntimeError "LIF: ..."
    const std::string& xml() const;                  // UTF-8
    int version() const;                             // LMSDataContainerHeader@Version, 0 if absent
    int lengthFieldWidth() const;                    // 4 or 8
    const MemoryBlock* findBlock(const std::string& id) const;   // nullptr if absent
    const std::vector<MemoryBlock>& blocks() const;  // file order, empty blocks included
};
}
```

- [ ] **Step 1: Write the failing tests** in `test_lif_container.cpp` (helper `fixture(name)` = `LIF_TEST_DATA_DIR "/" name`):
  - `LifContainer.ReadsVersion2` — `version()==2`, `lengthFieldWidth()==8`, `blocks().size()==3`, `blocks()[0].length==0`, `findBlock("MemBlock_0")->length == 16*12`, root text `xml()` contains `LMSDataContainerHeader`.
  - `LifContainer.ReadsVersion1` — width 4, same block lengths as v2.
  - `LifContainer.ProbesUnknownVersion` — `v3_probe.lif`: width 8, `findBlock("MemBlock_0")` present.
  - `LifContainer.DuplicateIdCounted` — `broken_refs.lif`: the DupRef id has `occurrences==2`.
  - `LifContainer.TruncatedBlockMarked` — `truncated.lif`: last block `truncated==true`; `Good`'s block not.
  - `LifContainer.TrailerGarbageTolerated` — `trailer_garbage.lif` reads without throwing.
  - `LifContainer.DecodesUtf16` — `unicode.lif`: `xml()` contains the UTF-8 bytes of `Zellkern µ – 細胞`.
  - `LifContainer.RejectsNonLif` — `not_lif.lif` throws; message contains `LIF: not a Leica LIF file`.
  - `LifContainer.RejectsLof` — `lof.lif` throws; message contains `LIF: not a Leica LIF file`.
- [ ] **Step 2: Build and run; verify they fail** — `cmake --build build --config Release --target slideio_lif_tests -- -m` then `./build/Release/slideio_lif_tests --gtest_filter=LifContainer.*` (Windows: test binaries under `build/`). Expected: compile error or FAIL.
- [ ] **Step 3: Implement `Container::read`** per D §4.1–4.2. Probe acceptance for unknown versions: identifier count `0 < M < 4096`, payload inside the file, and the following position is EOF, a zero u32, or `0x70` with `0x2A` at +8. Non-zero bytes after the trailer: WARNING, not an error. Parse failure of the XML as a document → retry wrapped in `<LEICA>…</LEICA>`; a second failure throws `LIF: malformed XML description`.
- [ ] **Step 4: Run; verify they pass.**
- [ ] **Step 5: Commit** — `feat(lif): container header, XML and memory-block index`

---

### Task 3: Image descriptions from XML

**Files:**
- Create: `src/slideio/drivers/lif/lifxml.hpp/.cpp`, `src/tests/lif/test_lif_xml.cpp`

**Interfaces:**
- Produces (`lifxml.hpp`):

```cpp
namespace slideio::lif {
struct Dimension { int dimId; int64_t count; int64_t bytesInc; double origin; double length; std::string unit; };
struct Channel {
    int dataType = -1;          // -1: attribute absent
    int resolution = 0;         // significant bits
    int64_t bytesInc = 0;
    int channelTag = -1;        // -1: absent
    std::string name;           // see Step 3
    std::vector<std::pair<std::string, std::string>> attributes;   // every attribute, verbatim
};
struct ImageDescription {
    std::string path;            // "Nest/CZMT"
    std::string memoryId;
    int64_t declaredSize = -1;   // Memory/@Size, -1 if absent
    std::vector<Dimension> dimensions;   // document order
    std::vector<Channel> channels;       // document order
    std::vector<uint64_t> timestampTicks;
    double delayTimeSeconds = 0;         // scanner nDelayTime_s, 0 if absent
    double magnification = 0;
    std::vector<std::pair<std::string, std::string>> tileFieldAttributes;  // per field: "FieldX=..;FieldY=..;PosX=..;PosY=.."
    std::string rawXml;          // owning Element without child Elements
};
SLIDEIO_LIF_EXPORTS std::vector<ImageDescription> parseImages(const std::string& xml);
}
```

`timestampTicks` comes from `lif::parseTimestampTicks` (Task 5) — declare it now in `liftimestamps.hpp` with a stub returning `{}`, so Task 5 only fills it in.

- [ ] **Step 1: Write the failing tests** (inline XML strings, plus fixtures through `Container::read`):
  - `LifXml.PathOmitsExperiment` — `nesting.lif` paths are `Nest/CZMT`, `Nest/ZCMT`, `Nest/MCZT`, `Nest/CZTM`.
  - `LifXml.SkipsContainersWithoutMemory` — inline XML: a folder `Element` with `Memory Size="0"` yields no description.
  - `LifXml.DropsProcessingHistory` — inline: an `Image` whose grandparent is `ProcessingHistory` is not returned.
  - `LifXml.DropsSequentialMaster` — inline: images under `LDM_Block_Sequential_Master` are not returned.
  - `LifXml.FirstDescriptionPerMemoryId` — inline: two images referencing one id → one description, the first.
  - `LifXml.ReadsDimensionsAndChannels` — `types.lif` `U16_12bit`: one channel, `resolution==12`, `dataType==0`; X dimension `count==16`, `bytesInc==2`; `NoDataType16` has `dataType==-1`.
  - `LifXml.ReadsMagnification` — `v2_basic.lif`: `magnification==63`.
  - `LifXml.RawXmlExcludesChildElements` — inline: a parent image `Element` with a child `Element` → parent `rawXml` does not contain the child's `Name`.
  - `LifXml.MalformedThrows` — `parseImages("<a><b></a>")` throws, message starts `LIF: `.
- [ ] **Step 2: Run; verify they fail.**
- [ ] **Step 3: Implement `parseImages`** per D §4.3 with tinyxml2. Channel `name`: first run `grep -o -E "U(se|es)rDefName[^>]{0,200}"` over a corpus file's decoded XML (decode with a two-line Python `open(p,'rb')` + `utf-16-le`) to find its real shape; implement only what that shows. If the corpus has none, leave `name` empty and add a one-line note to D §6.1 saying so.
- [ ] **Step 4: Run; verify they pass.**
- [ ] **Step 5: Commit** — `feat(lif): image descriptions from the XML`

---

### Task 4: `lif::Layout` — axis model, sample type, offsets, calibration

**Files:**
- Create: `src/slideio/drivers/lif/liflayout.hpp/.cpp`, `src/tests/lif/test_lif_layout.cpp`

**Interfaces:**
- Consumes: `ImageDescription`, `Dimension`, `Channel` (Task 3).
- Produces (`liflayout.hpp`):

```cpp
namespace slideio::lif {
class SLIDEIO_LIF_EXPORTS Unsupported : public std::runtime_error { using std::runtime_error::runtime_error; };
struct Calibration { double x = 0, y = 0, z = 0, t = 0; };   // metres, metres, metres, seconds
class SLIDEIO_LIF_EXPORTS Layout {
public:
    static Layout build(const ImageDescription& image, uint64_t payloadLength);  // throws Unsupported
    int width() const; int height() const;
    int numChannels() const;      // scene channels: 3 for interleaved RGB
    int numZ() const; int numT() const; int numFields() const;
    DataType dataType() const;    // DT_Byte ... DT_Float64 per D §5.2
    int sampleBytes() const;
    int significantBits(int channel) const;
    bool interleaved() const;
    int storedSample(int channel) const;           // scene channel -> stored channel/sample index (RGB reorder)
    int64_t pixelStride() const;                   // X BytesInc
    int64_t rowStride() const;                     // Y BytesInc
    uint64_t planeOffset(int channel, int z, int t, int field) const;  // relative to payload start, scene channel
    int64_t storageIndex(int channel, int z, int t, int field) const;  // plane rank in storage order (D §6.3)
    Calibration calibration() const;
};
}
```

Expected offsets for `nesting.lif` (b=2, plane P=8·6·2=96; strides innermost first):
`CZMT`: C 96, Z 192, M 576, T 2304. `ZCMT`: Z 96, C 288, M 576, T 2304. `MCZT`: M 96, C 384, Z 768, T 2304. `CZTM`: C 96, Z 192, T 576, M 1152.

- [ ] **Step 1: Write the failing tests** (descriptions via `parseImages` on fixture XML, `payloadLength` from `Container`):
  - `LifLayout.NestingOffsets` — for each of the four images, `planeOffset(c,z,t,m)` equals `c*Cs + z*Zs + t*Ts + m*Ms` with the strides above, for all 48 combinations.
  - `LifLayout.StorageIndexFollowsStrides` — `CZMT`: `storageIndex(1,0,0,0)==1`, `storageIndex(0,1,0,0)==2`, `storageIndex(0,0,0,1)==6`, `storageIndex(0,0,1,0)==24`.
  - `LifLayout.Types` — `types.lif`: U8→`DT_Byte`, U16_12bit→`DT_UInt16` with `significantBits(0)==12`, U32→`DT_UInt32`, F16→`DT_Float16`, F32→`DT_Float32`, F64→`DT_Float64`, NoDataType16→`DT_UInt16`; `Mismatch` throws `Unsupported`.
  - `LifLayout.InterleavedRgb` — `TagRGB`: `storedSample` = {0,1,2}; `TagBGR` and `Untagged`: {2,1,0}; all `numChannels()==3`, `interleaved()`.
  - `LifLayout.XzAndXt` — `XZ`: `height()==5`, `numZ()==1`; `XT`: `height()==7`, `numT()==1`.
  - `LifLayout.ExtraAxes` — `Extra1` builds; `Lambda` throws `Unsupported` whose `what()` contains `dimension 9`.
  - `LifLayout.Padding` — `Padded`: `rowStride()==20`.
  - `LifLayout.RejectsAliasAndOutOfBounds` — `Alias` and `OOB` throw `Unsupported`.
  - `LifLayout.RejectsUnevenChannelIncrements` — inline: three channels at BytesInc 0, 96, 288 → `Unsupported`.
  - `LifLayout.OffsetsAbove4GiB` — inline: X=65536, Y=65536, uint8, Z=2 with Z stride `2^32`, `payloadLength = 2^33`: `planeOffset(0,1,0,0) == 4294967296ULL`.
  - `LifLayout.RejectsOverflow` — inline: Z stride `2^62`, Z=8 → `Unsupported`, no wrap.
  - `LifLayout.Calibration` — `v2_basic` Plain: `x==1e-6`, `y==1e-6` (to 1e-15); `nesting` CZMT: `z==2e-6`, `t==10`; a dimension with `Unit="um"` gives 0; `N=1` gives 0.
- [ ] **Step 2: Run; verify they fail.**
- [ ] **Step 3: Implement `Layout::build`** per D §5 and §6.2. The aliasing check, as an algorithm the tests do not fully determine:

```text
extent = (H-1)*Ystride + W*Xstride            # bytes of one plane
axes   = [C (stride = min nonzero channel inc, count = scalar C), Z, T, M] with count > 1
sort axes by stride ascending
for a in axes: require a.stride >= extent; extent = a.stride*(a.count-1) + extent
require extent <= payloadLength               # all arithmetic checked uint64
```

  T resolution: T dimension `|Length|/(N-1)` when `Unit` is `s` (`Ks` ×1000), else `delayTimeSeconds`, else 0.
- [ ] **Step 4: Run; verify they pass.**
- [ ] **Step 5: Commit** — `feat(lif): stride-based layout with validation`

---

### Task 5: Timestamps

**Files:**
- Modify: `src/slideio/drivers/lif/liftimestamps.hpp/.cpp` (replace the Task 3 stub)
- Create: `src/tests/lif/test_lif_timestamps.cpp`

**Interfaces:**
- Consumes: `Layout::storageIndex`, `numChannels/numZ/numT/numFields` (Task 4).
- Produces:

```cpp
namespace slideio::lif {
std::vector<uint64_t> parseTimestampTicks(const tinyxml2::XMLElement* imageDescriptionParent);  // both encodings, low word masked
class SLIDEIO_LIF_EXPORTS PlaneTimes {
public:
    static PlaneTimes build(const std::vector<uint64_t>& ticks, const Layout& layout, int field);
    bool complete() const;                     // exactly C*Z*T*M ticks
    int64_t acquisitionTime() const;           // Unix seconds of the field's earliest plane
    double offset(int t, int c, int z) const;  // seconds from acquisitionTime(), >= 0
};
}
```

Fixture facts: `timestamps.lif` `Hex` has C=2, T=3, M=1 (6 planes); tick of storage plane `k` is `131000000000000000 + k*10^7`, so `acquisitionTime() == 13100000000 - 11644473600 == 1455526400` and `offset` of storage plane `k` is `k` seconds.

- [ ] **Step 1: Write the failing tests:**
  - `LifTimestamps.HexEncoding` — `Hex`: `complete()`, `acquisitionTime()==1455526400`, `offset(t,c,0) == layout.storageIndex(c,0,t,0)` for all six.
  - `LifTimestamps.DecimalEncodingMatchesHex` — `Decimal` gives identical values.
  - `LifTimestamps.ShortListIsIncomplete` — `Short`: `complete()==false`, `offset(...)==0`.
  - `LifTimestamps.SignedLowWordMasked` — inline `TimeStamp HighInteger="30500000" LowInteger="-1"`: ticks `== (30500000ULL<<32) | 0xFFFFFFFF`.
  - `LifTimestamps.PerFieldOrigin` — synthetic `Layout` from `nesting.lif` `CZTM` (M outermost) with 48 ticks `base + k*10^7`: field 1's `acquisitionTime()` is 12 s after field 0's, and its earliest plane has `offset==0`.
- [ ] **Step 2: Run; verify they fail.**
- [ ] **Step 3: Implement** per D §6.3 and FD §8.2.
- [ ] **Step 4: Run; verify they pass.**
- [ ] **Step 5: Commit** — `feat(lif): plane timestamps`

---

### Task 6: Driver, slide, scene and read path

**Files:**
- Create: `src/slideio/drivers/lif/{lifimagedriver,lifslide,lifscene}.hpp/.cpp`
- Create: `src/tests/lif/lif_fixture_values.hpp`, `src/tests/lif/test_lif_scene.cpp`

**Interfaces:**
- Consumes: everything from Tasks 2–5.
- Produces:

```cpp
namespace slideio {
namespace lif { struct ReadLimits { int64_t maxResampleSourcePixels = int64_t(16) << 20; }; }
class SLIDEIO_LIF_EXPORTS LIFImageDriver : public ImageDriver {   // getID "LIF", getFileSpecs "*.lif"
public:
    std::string getID() const override;
    std::shared_ptr<CVSlide> openFile(const std::string& filePath) override;
    std::string getFileSpecs() const override;
};
class SLIDEIO_LIF_EXPORTS LIFSlide : public CVSlide {
public:
    static std::shared_ptr<LIFSlide> openFile(const std::string& path, lif::ReadLimits limits = {});
    int getNumScenes() const override;
    std::string getFilePath() const override;
    std::shared_ptr<CVScene> getScene(int index) const override;
    const std::vector<std::string>& skippedImages() const;   // "<path>: <reason>"
};
class SLIDEIO_LIF_EXPORTS LIFScene : public CVScene { /* one image field; holds shared_ptr<const FileReader>,
    shared_ptr<const lif::Layout>, payload offset, field index, PlaneTimes, Calibration, ReadLimits */ };
}
// tests: lif_fixture_values.hpp
double lifExpected(int x, int y, int c, int z, int t, int m, int salt, slideio::DataType type);
```

`LIFScene` overrides: the `CVScene` pure virtuals, `getNumZSlices`, `getNumTFrames`, `getZSliceResolution`, `getTFrameResolution`, `getChannelSignificantBits`, `hasPlaneTimestamps`, `getPlaneTimestamp`, `getAcquisitionTime`, `getChannelName`, `supportsConcurrentReads` (→ `true`), `getChannelBackground` (→ `0` for every channel: fluorescence background is dark — *a call this plan makes; D is silent*), `readResampledBlockChannelsEx`. Compression `Uncompressed`. Raw metadata `MetadataFormat::XML`: slide = `Container::xml()`, scene = `ImageDescription::rawXml`. Channel attributes via `setChannelAttribute(channel, name, value)` for every pair in `Channel::attributes`. Scene names per D §4.5 (`<path>/Field <j+1>` when `numFields() > 1`).

- [ ] **Step 1: Write the failing tests** (open through `LIFImageDriver().openFile`, except where `LIFSlide::openFile` is named for `ReadLimits`):
  - `LifSlide.OpensBasic` — `v2_basic.lif`, `v1_basic.lif`: one scene `Plain`, rect 16×12, 1 channel, `DT_Byte`, magnification 63, resolution (1e-6, 1e-6), `getRawMetadata()` non-empty, metadata format XML.
  - `LifSlide.MatchesById` — `order_by_id.lif`: scene `A` pixel (3,4) == `lifExpected(3,4,0,0,0,0,0,DT_Byte)`; `B` uses salt 1.
  - `LifSlide.TileFieldsAreScenes` — `nesting.lif`: 16 scenes; names `Nest/CZMT/Field 1` … `Nest/CZTM/Field 4`; each 8×6, 2 channels, Z=3, T=2.
  - `LifSlide.SkipsUnsupported` — `extra_axes.lif`: scenes `Extra1`, `Plain`; `skippedImages()` has one entry starting `Lambda: `. `broken_refs.lif`: only `Plain`; three skipped. `bad_strides.lif`: only `Plain`. `truncated.lif`: only `Good`.
  - `LifSlide.AllUnsupportedFails` — `all_unsupported.lif` throws; message contains `Lambda`.
  - `LifSlide.DuplicateNamesKept` — `dup_names.lif`: 2 scenes, both named `Same`, `getSceneByName("Same")` returns the salt-0 one (check pixel (0,0) == 0).
  - `LifSlide.UnicodeName` — scene name equals the UTF-8 `Zellkern µ – 細胞`.
  - `LifScene.EveryPlaneOfNesting` — for every scene, z, t, channel of `nesting.lif`: `read4DBlockChannels` of the full rect equals `lifExpected` at every pixel (salt = image index, m = field).
  - `LifScene.TypesRoundTrip` — each `types.lif` scene: `getChannelDataType(0)` and pixel (5,7) equal `lifExpected` for that type; `U16_12bit` reports significant bits 12.
  - `LifScene.RgbOrder` — `TagRGB`, `TagBGR`, `Untagged`: output channel 0 at (x,y) equals the stored sample whose ChannelTag is 1 (Untagged: stored sample 2).
  - `LifScene.ChannelSubsetOrder` — `TagRGB` with channels `{2, 0}`: output plane 0 == stored sample 2, plane 1 == stored sample 0.
  - `LifScene.XzScan` — `XZ` scene 16×5, pixel (2,3) == `lifExpected(2,0,0,3,0,0,…)` (display Y is Z).
  - `LifScene.PaddedRows` — `Padded` reads equal `lifExpected` on every row.
  - `LifScene.Timestamps` — `Hex` scene: `hasPlaneTimestamps()`, `getAcquisitionTime()==1455526400`; `Short`: `hasPlaneTimestamps()==false`.
  - `LifScene.EdgeIsBackground` — reading `(-4,-4,8,8)` of `Plain`: the overhanging quarter is 0, the inner quarter equals the fixture.
  - `LifScene.ResampleMatchesCvResize` — `Plain` full rect to 8×6 equals `cv::resize(full, INTER_AREA)`.
  - `LifScene.BandedResampleEqualsUnbanded` — `nesting.lif` via `LIFSlide::openFile(path, {maxResampleSourcePixels = 16})`: a full-rect read to 4×3 equals the same read with default limits.
  - `LifScene.ConcurrentReads` — `TestTools::concurrentReadIdentityTestAllScenes` on `nesting.lif`; `supportsConcurrentReads()==true`.
  - `LifScene.FileReleasedAfterClose` — copy `v2_basic.lif` to a `TempFile`, open, read, drop slide and scenes, then `TestTools::isFileHeldOpen(copy)==false`.
  - `LifImageDriver.ClaimsByNameOnly` — `canOpenFile("x.LIF")`, `canOpenFile("x.lif")` true, `canOpenFile("x.lifext")` false, no file needed; `openFile(not_lif.lif)` throws `LIF: not a Leica LIF file`.
- [ ] **Step 2: Run; verify they fail.**
- [ ] **Step 3: Implement** per D §4.4–4.5, §6, §7. Read path inside `readClampedBlock(getRect(), rect, blockSize, channels, reader, output)`: for interleaved images one `readAt` per row of `w * pixelStride()` bytes, gathering requested samples via `storedSample`; for scalar images one `readAt` per row per requested channel (one call for the band when `rowStride() == width()*sampleBytes()` and the rect spans the width); then `cv::resize(..., INTER_AREA)` when sizes differ. When `rect.area()` exceeds `maxResampleSourcePixels`, split into horizontal bands whose target heights sum exactly to `blockSize.height` (the MRXS banding in `mrxsscene.cpp`, searched by `maxResampleSourcePixels`). Byte-swap after reading on a big-endian host only (`core/tools/endian.hpp`).
- [ ] **Step 4: Run; verify they pass**, then run under ThreadSanitizer if a Linux box is at hand (CI does it in Task 7).
- [ ] **Step 5: Commit** — `feat(lif): LIF slide, scenes and read path`

---

### Task 7: Register the driver, CI and docs

**Files (anchors are greppable text, not line numbers):**
- Modify: root `CMakeLists.txt` — `"lib${LIF_DRIVER_NAME}.dylib"` in `NAME_TOOL_LIB_LIST` next to the BIF entry; `${LIF_DRIVER_NAME}` in `SLIDEIO_DRIVER_LIBS` next to `${BIF_DRIVER_NAME}`
- Modify: `src/slideio/slideio/CMakeLists.txt` — `${LIF_DRIVER_NAME}` next to `${BIF_DRIVER_NAME}`
- Modify: `src/slideio/slideio/imagedrivermanager.cpp` — include `lifimagedriver.hpp`; `make_shared<LIFImageDriver>()` beside the BIF block; `"LIF"` before `"GDAL"` in `driverOrder`
- Modify: `src/slideio/drivers/lif/CMakeLists.txt` — `FIX_MACOS_RPATH(${LIBRARY_NAME})` if bif's has it (check `grep FIX_MACOS_RPATH src/slideio/drivers/*/CMakeLists.txt`)
- Modify: `auxfiles/package-smoke/main.cpp` — `"LIF"` in `kExpectedDrivers`, alphabetical
- Modify: `cmake-scripts/packaging.cmake` — "Leica LIF" in the Debian description beside "Ventana BIF"
- Modify: `src/tests/main/test_imagedrivermanager.cpp`, `test_scene_index.cpp`, `test_scene_lifetime.cpp`, `test_edge_reads.cpp` — rows modelled on each BIF row, using corpus file `lif/bioformats/michael/PR2729_frameOrderCombinedScanTypes.lif` (227 KB)
- Modify: `auxfiles/list-test-images.py` — `'lif': 'slideio_lif_tests'` in `BINARY`
- Modify: `.github/workflows/build-validation.yml` (the ThreadSanitizer step and the release test list, beside each `slideio_bif_tests` line) and `.github/workflows/release.yml` (both `slideio_bif_tests`/`"$bif"` places) — `slideio_lif_tests --gtest_filter="-LIFCorpus*"`
- Modify: `README.md` format table; `CLAUDE.md` — "14 formats" → "15 formats", "13 driver libraries" → "14 driver libraries", the format list, `lif/` in the source-layout tree, `slideio_lif_tests` in the test list, the `LIFCorpus*` rule and the `src/tests/lif/data/` fixtures beside the MRXS paragraph

- [ ] **Step 1: Write the failing test** — the four `src/tests/main` rows. Run `slideio_tests --gtest_filter="*Lif*:*lif*:*LIF*"`. Expected: FAIL (driver not registered) or SKIP if the corpus file is absent — copy it first (Task 8 Step 1) or set `SLIDEIO_SKIP_MISSING_IMAGES=1` only for this check.
- [ ] **Step 2: Register** the driver in the files above.
- [ ] **Step 3: Rebuild everything and run** `slideio_tests` (full), `slideio_lif_tests --gtest_filter=-LIFCorpus*`. Expected: all pass. Build tree: reused; reconfigure first because `CMakeLists.txt` changed.
- [ ] **Step 4: Package smoke locally** — `py -3 install.py -a package-only -c release`, then build `auxfiles/package-smoke/` against it as `release.yml` does. Expected: `LIF` reported among the drivers.
- [ ] **Step 5: Docs and CI edits**; `grep -n "slideio_lif_tests" .github/workflows/*.yml` shows four lines.
- [ ] **Step 6: Commit** — `feat(lif): register the LIF driver; CI and docs`

---

### Task 8: Corpus references and `LIFCorpus*` suites

**Files:**
- Create: `auxfiles/lif-fixtures/make_region_reference.py`, `auxfiles/lif-fixtures/LifScenes.java`
- Create (generated, committed): `src/tests/lif/data/lif-regions.tsv`, `src/tests/lif/data/lif-scenes.tsv`
- Create: `src/tests/lif/test_lif_corpus.cpp`
- Modify: `software-docs/TEST_IMAGES.md` (regenerated)

Corpus layout under `$SLIDEIO_IMAGES_PATH/lif/` (relative paths as in the archive):
`bioformats/imagesc-110520/AMR1.lif`, `bioformats/imagesc-30856/20191025 Test FRET 585. 423, 426.lif`, `bioformats/michael/PR2729_frameOrderCombinedScanTypes.lif`, `bioformats/seanwarren/<as in archive>/150519_FRAP_test_ROIs_chromagreen.lif`, `bioformats/zenodo-6606445/Project007.lif`, `zenodo/intro/Intro.lif`, `figshare/IT4_27_02_15.lif`, plus each folder's readme/licence note.

`lif-regions.tsv` columns: `file` (corpus-relative path), `series`, `z`, `c`, `t`, `rx`, `ry`, `rw`, `rh`, `crc32` (hex, zlib CRC-32 of the region bytes, rows top to bottom, little-endian samples). `lif-scenes.tsv` columns: `file`, `series`, `name`, `width`, `height`, `c`, `z`, `t`, `pixelType`, `physX_um`, `physY_um`, `physZ_um`.

- [ ] **Step 1: Copy the seven files into the corpus.** This writes outside the repository: confirm the target `$SLIDEIO_IMAGES_PATH/lif/` with the user before copying. Read-only source.
- [ ] **Step 2: Write `make_region_reference.py`** — inputs: the validation folder, the three region TSVs (`bioformats-`, `zenodo-`, `figshare-regions.tsv`). For each row, reproduce the read of the validation `compare_regions.py` (same inventory JSON, same stride address, same plane decomposition from `order`), **assert its SHA-256 equals the TSV's** (exit non-zero on any mismatch), then write the CRC-32 row with the decomposed `z,c,t`.
  Run: `py -3 auxfiles/lif-fixtures/make_region_reference.py D:/Projects/slideio/documents/image_formats/lif/validation src/tests/lif/data/lif-regions.tsv`
  Expected: `9264 regions, 0 mismatches`; file ~0.5 MB.
- [ ] **Step 3: Write and run `LifScenes.java`** (modelled on validation `LifCheck.java`; Bio-Formats JAR from the validation README) to produce `lif-scenes.tsv` for all seven files. Expected: 137 series rows.
- [ ] **Step 4: Write the failing corpus tests** in `test_lif_corpus.cpp` (each with `SLIDEIO_SKIP_IF_IMAGE_MISSING`):
  - `LIFCorpusOpen.ScenesMatchBioFormats` — per file: scene count == series count; per series: width, height, channels, Z, T and type match `lif-scenes.tsv`; scene resolution (µm) matches `physX_um`/`physY_um`/`physZ_um` to 1e-9 relative, **or**, where it differs, the test prints the pair and the difference is recorded in D §6.2 — a disagreement is a finding, not a silent pass.
  - `LIFCorpusRead.RegionsMatchBioFormats` — every row of `lif-regions.tsv`: `read4DBlockChannels(rect, {c}, z..z+1, t..t+1)` → CRC-32 (`crc32` from zlib; link `ZLIB::ZLIB` in the test target) equals the reference. Expected total: 9,264 matches.
  - `LIFCorpusOpen.SignificantBitsAmr1` — every AMR1 scene: `getChannelSignificantBits(c)==12`.
  - `LIFCorpusOpen.Timestamps` — FRAP and Intro tile scans: where `hasPlaneTimestamps()`, offsets are non-negative and non-decreasing in T. Then compare with liffile (`image.timestamps` from the pinned source) on FRAP via a reference column produced in Step 2's script if liffile is available; record the outcome in D §6.3.
  - `LIFCorpusOpen.Magnification` — print each file's magnification; assert non-zero on AMR1 only if Step 3 of Task 3 found the attribute there.
  - `LIFCorpusRead.ConcurrentIdentity` — `TestTools::concurrentReadIdentityTestAllScenes` on `Project007.lif`.
- [ ] **Step 5: Run; verify they pass** — `slideio_lif_tests --gtest_filter=LIFCorpus*`. If a region mismatches, stop: that is an addressing bug, diagnose with superpowers:systematic-debugging before touching the reference.
- [ ] **Step 6: Regenerate `TEST_IMAGES.md`** — `py -3 auxfiles/list-test-images.py`.
- [ ] **Step 7: Commit** — `test(lif): Bio-Formats region and scene references; corpus suites`

---

### Task 9: Final review and design status

- [ ] **Step 1:** Update the spec's status line to `implemented on feature/lif-driver (<date>)` and fold in what Tasks 3, 6 and 8 recorded (channel-name source, background 0, resolution/timestamp findings).
- [ ] **Step 2:** Full run — `slideio_tests`, `slideio_lif_tests` (all, with corpus), `slideio_bif_tests --gtest_filter=-BIFCorpus*`, `slideio_mrxs_tests --gtest_filter=-MRXSCorpus*`. Expected: all pass.
- [ ] **Step 3:** Request the whole-branch review (superpowers:requesting-code-review), then superpowers:finishing-a-development-branch.
