# Ventana BIF Driver Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add a native `BIF` driver that opens Ventana BIF slides as one stitched,
seamless scene per scanned AOI, with label, thumbnail and mask as aux images.

**Architecture:** This is a new shared library, `slideio-bif`, in
`src/slideio/drivers/bif/`.

- **Pure layer.** File-free parsers and geometry: level descriptions, `iScan`
  XML, `EncodeInfo` XML, per-boundary overlaps, seam cuts and axis maps. It is
  exported so `slideio_bif_tests` can test it in isolation.
- **Driver layer.** `BIFSlide` classifies the IFDs, and `BIFScene` renders a
  level rect. It builds a column map and a row map (output pixel to storage
  pixel), decodes each storage TIFF tile once, and copies runs, with no
  resampling. TIFF handles come from a `ContextPool`, so reads are concurrent.

**Tech Stack:** C++17, libtiff (via `TiffTools`), tinyxml2, OpenCV, GoogleTest.

**Spec:** `software-docs/specs/2026-11-09-bif-driver-design.md`. The §n
references below are to it. The format reference is
`../../../documents/image_formats/bif/Ventana_BIF_Format.md`, outside the repo.

## Global Constraints

- Driver id `"BIF"`; file spec `"*.bif"`. `canOpenFile` matches the name only
  and does no I/O. The structural probe runs in `openFile`.
- Namespace `slideio::bif` for internal types. The export macro is
  `SLIDEIO_BIF_EXPORTS`, keyed on the `SLIDEIO_BIF_API` compile definition.
- Every error goes through `RAISE_RUNTIME_ERROR` with the prefix `"BIF: "`.
  Logging uses `SLIDEIO_LOG(INFO|WARNING)`.
- Corpus root `$SLIDEIO_IMAGES_PATH/bif/openslide/`, files `OS-1.bif`,
  `OS-2.bif`, `Ventana-1.bif`. Corpus suites are named `BIFCorpus*`, and no
  other suite may touch the corpus.
- The Z refusal message is exactly `BIF: multi-layer (Z=<N>) files are not supported yet`.
- Aux image names are `Label`, `Thumbnail` and `Probability`. Scene names are
  `AOI<n>`, or `Image` for a plain Ventana TIFF.
- Resolution is `ScanRes` µm × 1e-6 metres, on both axes.
- Label images on clinical slides can carry patient identifiers (HIPAA/GDPR).
  Never log aux-image pixels or barcode attributes, and never put them in an
  exception message.
- Cite code by greppable names, not line numbers: earlier tasks move lines.
- Build environment: see the memory note "Getting a build configured". Run the
  conan loop for every new `conanfile.*`, then re-run the cmake configure.
  - Configure: `cmake -G "Visual Studio 17 2022" -A x64 -DCMAKE_TOOLCHAIN_FILE=./cmake/conan_toolchain.cmake -S . -B build`.
  - Build: `cmake --build build --config Release --target <target> -- -m`.
  - Executables land in `build/bin/Release/`.

## Review Focus

1. **A `.bif` file that isn't BIF** (for example, a renamed SVS). Expect
   `BIF: not a Ventana BIF file (no iScan metadata in IFD 0)`, not a crash or
   "Cannot find driver". Owned by Task 6.
2. **A read of the last row or column at the coarsest level**, where
   `Th/d` is fractional (OS-1 level 9). The maps must clamp to the level's
   storage size, with no out-of-range tile index. Owned by Task 5 (map) and
   Task 7 (read).
3. **A scaled read whose `blockSize` differs from the rect.** It must equal a
   `cv::resize` (`INTER_AREA`) of the native-size read of the same rect.
   Owned by Task 7.
4. **A channel subset or reordering (`{2,0}`).** The output channels must be
   in the caller's order. Owned by Task 7.
5. **Concurrent reads on both scenes of OS-2 at once.** They must be
   byte-identical to serial reads. Owned by Task 7.

---

### Task 1: TiffTools reads XMLPacket and ImageDepth; corpus in place

**Files:**
- Modify: `src/slideio/imagetools/tifftools.hpp`, `src/slideio/imagetools/tifftools.cpp` (beside `readStringTag`)
- Test: `src/tests/main/test_tifftools.cpp`

**Interfaces:**
- Produces:
  - `static std::string TiffTools::readXmlPacket(libtiff::TIFF* tiff, const TiffDirectory& dir);`
    Returns the bytes of tag 700, truncated at the first NUL, or `""` when the
    tag is absent.
  - `static uint32_t TiffTools::readImageDepth(libtiff::TIFF* tiff, const TiffDirectory& dir);`
    Returns tag 32997, or 1 when the tag is absent.
  - Both call `setCurrentDirectory` first.

- [ ] **Step 1: Check the corpus.** The slides were copied on 2026-11-09. Run
  `sha256sum $SLIDEIO_IMAGES_PATH/bif/openslide/*.bif` and compare against
  `index.yaml` in the same folder. All three must match.
- [ ] **Step 2: Write failing tests.**
  - `TiffTools.readXmlPacketTrimsNul`: write a 16×16 tiled 8-bit RGB TIFF to a
    temp file with libtiff. Set `TIFFTAG_XMLPACKET` to the 9 bytes
    `"<a b=\"1\"/>"` followed by 3 NULs (count 13). Assert
    `readXmlPacket(...) == "<a b=\"1\"/>"`.
  - `TiffTools.readXmlPacketAbsent`: the same file without the tag gives `""`.
  - `TiffTools.readImageDepth`: a file with `TIFFTAG_IMAGEDEPTH` = 3 gives 3;
    a file without it gives 1.
- [ ] **Step 3:** Build `slideio_tests` and run
  `slideio_tests --gtest_filter=TiffTools.read*`. Expect a compile failure:
  the functions don't exist yet.
- [ ] **Step 4:** Implement both functions. Follow the ICC read in
  `scanTiffDirTags`: `TIFFGetField(tiff, TIFFTAG_XMLPACKET, &count, &data)` for
  the packet, and `TIFFGetField(tiff, TIFFTAG_IMAGEDEPTH, &u32)` for the depth.
- [ ] **Step 5:** Run the same filter again. Expect PASS. Then run the whole
  `slideio_tests` suite with `SLIDEIO_SKIP_MISSING_IMAGES=1`. Expect no new
  failures.
- [ ] **Step 6: Commit.** Message: `feat(imagetools): read XMLPacket and ImageDepth tags`.

### Task 2: Library skeleton, test executable, level descriptions

**Files:**
- Create in `src/slideio/drivers/bif/`:
  - `CMakeLists.txt`, `conanfile.py` and `CMakeUserPresets.json`: copy svs's
    and trim to TIFF, JPEG, opencv and tinyxml2.
  - `bif_api_def.hpp`
  - `bifdescription.hpp/.cpp`
- Create `src/tests/bif/`: `CMakeLists.txt` (modelled on `src/tests/mrxs/`),
  `conanfile.txt` (gtest, opencv) and `test_bif_description.cpp`.
- Modify:
  - `src/slideio/drivers/CMakeLists.txt`: add `add_subdirectory(bif)`.
  - `src/tests/CMakeLists.txt`.
  - Root `CMakeLists.txt`: `BIF_DRIVER_NAME`, `NAME_TOOL_LIB_LIST` and
    `SLIDEIO_DRIVER_LIBS`.
  - `src/slideio/slideio/CMakeLists.txt`: link the library.

**Interfaces:**
- Produces, in `bifdescription.hpp` (all exported):
  ```cpp
  namespace slideio::bif {
  struct LevelDescription { int level; double magnification;
      std::map<std::string, std::string> tokens; };
  // nullopt when there is no "level" token; throws when level is present but
  // mag is missing, or either value fails to parse
  std::optional<LevelDescription> parseLevelDescription(const std::string& description);
  enum class AuxKind { None, Label, Thumbnail, Probability };
  AuxKind classifyAuxDescription(const std::string& description);  // exact match
  std::string auxImageName(AuxKind kind);   // "Label", "Thumbnail", "Probability"
  }
  ```

- [ ] **Step 1: Write the tests in `test_bif_description.cpp`.**
  - Parsing `"level=0 mag=40 quality=95"` gives level 0, magnification 40, and
    `tokens["quality"] == "95"`.
  - Parsing `"level=6 mag=0.625 quality=90"` gives magnification 0.625.
  - `"Label Image"` gives `nullopt`.
  - `"level=1"` throws `slideio::RuntimeError`.
  - Classification: `"Label Image"` and `"Label_Image"` are `Label`;
    `"Thumbnail"` is `Thumbnail`; `"Probability_Image"` is `Probability`;
    `"label image"` and `""` are `None`.
- [ ] **Step 2:** Run the conan loop for both new conanfiles, then configure.
  Building `slideio_bif_tests` fails on the missing functions.
- [ ] **Step 3:** Implement. Split on whitespace, then each token at its first
  `=`. Matching is exact and case-sensitive.
- [ ] **Step 4:** Run `slideio_bif_tests --gtest_filter=BIFDescription*`.
  Expect PASS.
- [ ] **Step 5: Commit.** Message: `feat(bif): driver library skeleton and level descriptions`.

### Task 3: XML parsing — acquisition and stitching

**Files:**
- Create:
  - `src/slideio/drivers/bif/bifxml.hpp/.cpp`
  - `src/tests/bif/bif_xml_fixtures.hpp`: trimmed real XML, below.
  - `src/tests/bif/test_bif_xml.cpp`

**Interfaces:**
- Produces, in `bifxml.hpp` (all exported):
  ```cpp
  namespace slideio::bif {
  struct AcquisitionInfo { double scanResMicrons = 0; double magnification = 0;
      int zLayers = 1; std::string scannerModel; };
  // root <iScan>, or <Metadata> with an <iScan> child; otherwise throws
  // "BIF: not a Ventana BIF file (no iScan metadata in IFD 0)"
  AcquisitionInfo parseAcquisitionXml(const std::string& xml);

  enum class JointAxis { Horizontal, Vertical };     // RIGHT/LEFT, UP
  struct Joint { int tile1, tile2; JointAxis axis; bool joined; int confidence;
      double overlapX, overlapY;
      bool usable() const { return joined && confidence > 0; } };
  struct Aoi { int index; bool scanned; int originX, originY;
      int tileWidth, tileHeight; int cols, rows;
      std::vector<Joint> joints;        // scanned AOIs only; usable joints only
      std::string imageInfoXml; };      // <ImageInfo> without its children, serialised
  struct StitchInfo { std::vector<Aoi> aois; };   // document order, unscanned included
  StitchInfo parseStitchXml(const std::string& xml);   // throws on malformed or inconsistent

  // one-based snake number -> (col,row), top-left origin (format description §6);
  // throws if n is outside 1..cols*rows
  cv::Point snakePosition(int tileNumber, int cols, int rows);
  }
  ```

What `parseStitchXml` does:

- It walks `/EncodeInfo/SlideStitchInfo/ImageInfo` and the element children of
  `/EncodeInfo/AoiOrigin`. The two counts must be equal, and the lists are
  paired by order.
- `index` is taken from `AOIIndex` when present, otherwise from the integer
  suffix of the origin element's name (`AOI<n>`), otherwise from the ordinal.
  When `AOIIndex` and the suffix are both present and disagree, it throws.
- For a scanned AOI, it keeps only usable joints (§4.5):
  - `RIGHT` and `LEFT` map to `Horizontal`; `UP` maps to `Vertical`;
  - any other direction on a usable joint throws, naming the direction;
  - a usable joint whose `snakePosition`s are not neighbours throws. For a
    horizontal joint, tile 2 must be `(c+1, r)`; for a vertical one, `(c, r−1)`.
- Unscanned AOIs keep their joints unparsed and are never validated. OS-2's
  `AOI1` has `Tile1='0'`.
- Attribute values may be single- or double-quoted; tinyxml2 handles both.

- [ ] **Step 1: Write `bif_xml_fixtures.hpp`.** It is copied from the samples,
  not invented.
  - The `iScan` packet of `OS-2.bif` IFD 0, verbatim.
  - The `Metadata/iScan` packet of `Ventana-1.bif` IFD 0, verbatim.
  - An `EncodeInfo` for OS-2: its three `ImageInfo` start tags exactly as in
    the file, each with these joints:
    - AOI0: `FlagJoined='0' … RIGHT Tile1='1' Tile2='2'`,
      `'1' Confidence='98' RIGHT Tile1='3' Tile2='4' OverlapX='115' OverlapY='2'`,
      and `'1' '99' UP Tile1='3' Tile2='20' OverlapX='-2' OverlapY='93'`.
    - AOI1 (unscanned): one joint, `Tile1='0' Tile2='0'`.
    - AOI2: `'1' '98' UP Tile1='200' Tile2='371' OverlapX='-2' OverlapY='95'`.
    - The `AoiOrigin` block with `AOI0 0/38080`, `AOI1 6144/68000` and
      `AOI2 30720/0`.
    - 64 NUL bytes appended. The fixture is a `std::string` built with an
      explicit length.
  - An `EncodeInfo` for Ventana-1: `ImageInfo AOIIndex="0" NumRows="21" NumCols="23" Width="1024" Height="1024"`
    with the joints `LEFT 1→2 0`, `LEFT 46→45 0`, `LEFT 4→5 OverlapX="24"` and
    `UP 1→46 0`, and the `AoiOrigin` `AOI0 0/0`.
- [ ] **Step 2: Write the tests in `test_bif_xml.cpp`.**
  - **Acquisition:**
    - OS-2: `scanResMicrons == 0.2325`, magnification 40, `zLayers == 1`.
    - Ventana-1: `scannerModel == "VENTANA DP 200"`, `scanResMicrons == 0.25`.
    - `"<foo/>"` throws, with a message containing `no iScan metadata`.
  - **Snake numbering:** with a 3×3 grid,
    - `snakePosition(1,3,3) == (0,2)`;
    - `(3) == (2,2)`;
    - `(4) == (2,1)`;
    - `(6) == (0,1)`;
    - `(7) == (0,0)`;
    - `(9) == (2,0)`;
    - `0` and `10` throw.
  - **OS-2 stitch:**
    - three AOIs, with indices 0, 1 and 2;
    - `aois[1].scanned == false`;
    - `aois[0]` has 2 joints (the unjoined one is dropped), origin (0, 38080),
      tile 1024×1360, grid 11×17;
    - `aois[2].originX == 30720`.
  - **Ventana-1 stitch:** four joints, all usable; `LEFT` parses as `Horizontal`.
  - **Mutations of the Ventana-1 fixture, each of which throws:**
    - `AOIIndex="5"` against `<AOI0`;
    - a usable joint `Direction="DOWN"` (the message contains `DOWN`);
    - a usable `LEFT 1→3` (not neighbours);
    - one fewer `AoiOrigin` child;
    - truncated XML.
  - The same `DOWN` joint with `FlagJoined="0"` does not throw.
- [ ] **Step 3:** Run `slideio_bif_tests --gtest_filter=BIFXml*`. Expect FAIL.
- [ ] **Step 4:** Implement with tinyxml2. Parse
  `std::string(xml.c_str())`, which trims at the first NUL.
  `imageInfoXml` comes from `XMLPrinter` on a shallow clone of the element.
- [ ] **Step 5:** Run again. Expect PASS.
- [ ] **Step 6: Commit.** Message: `feat(bif): parse iScan and EncodeInfo XML`.

### Task 4: Orientation gate (design §8) — blocks Task 5

**Files:**
- Create `src/tests/bif/test_bif_corpus_orientation.cpp`.

**Interfaces:**
- Consumes: `parseStitchXml`, `snakePosition`, `TiffTools::readXmlPacket`,
  `TiffTools::readTile`, `TiffTools::scanFile`.

- [ ] **Step 1: Write `BIFCorpusOrientation.SnakeMatchesPixels`.** It runs on
  OS-1 AOI0 and OS-2 AOI2.
  1. Read the level-0 packet (the IFD whose description parses to level 0).
     Take the usable joints whose overlap is at least 8 px away from the
     axis median, at most 20 per axis.
  2. For each joint and each of four orientations:
     - as described: `(c, r)`;
     - rows flipped: `(c, R−1−r)`;
     - columns mirrored: `(C−1−c, r)`;
     - both.

     Locate the two tiles in storage at
     `originX/Tw + c, originY/Th + r`, and decode them with `readTile`.
  3. Search for the overlap `k` in `[0, Tw/4]`, and a cross-shift in `[−6, 6]`,
     that maximises the normalised cross-correlation (`cv::matchTemplate`,
     `TM_CCOEFF_NORMED`) between the facing 32-px edge strips.
  4. Count a joint as agreeing when `|k − joint overlap| ≤ 3`.
  5. Assert: the "as described" orientation agrees on ≥ 80% of the sampled
     joints, and every other orientation on fewer than 50%.
- [ ] **Step 2:** Run `slideio_bif_tests --gtest_filter=BIFCorpusOrientation*`.
- [ ] **Step 3: Decide.**
  - **PASS:** append a dated line to design §8 ("Orientation verified
    2026-…: described snake order, x/20 and y/20 joints agree"), and go on.
  - **Another orientation wins clearly:** change `snakePosition` to it, and
    update the snake tests in Task 3 and this test's "as described"
    orientation. Record the corrigendum in design §8 and in the format
    description §6.
  - **No orientation wins:** stop. Report the per-orientation agreement counts
    to the user, and do not start Task 5.
- [ ] **Step 4: Commit.** Message: `test(bif): verify snake orientation against pixels`.

### Task 5: Geometry — overlaps, placement, seams, axis maps

**Files:**
- Create:
  - `src/slideio/drivers/bif/bifgeometry.hpp/.cpp`
  - `src/tests/bif/test_bif_geometry.cpp`

**Interfaces:**
- Consumes: `Aoi`, `Joint`, `JointAxis` and `snakePosition` (Task 3).
- Produces (exported):
  ```cpp
  namespace slideio::bif {
  // median of the usable joints' overlap along `axis` (X for Horizontal, Y for
  // Vertical) across all scanned AOIs; nullopt if there are none
  std::optional<double> slideMedianOverlap(const std::vector<Aoi>& aois, JointAxis axis);
  struct BoundaryOverlaps { std::vector<int> columns; std::vector<int> rows; }; // sizes C-1, R-1
  BoundaryOverlaps estimateOverlaps(const Aoi& aoi, std::optional<double> slideMedianX,
                                    std::optional<double> slideMedianY);
  class AoiGeometry {
  public:
      AoiGeometry(const Aoi& aoi, const BoundaryOverlaps& overlaps);
      cv::Size size() const;                                    // level 0
      const std::vector<int>& columnPositions() const;          // x[c]
      const std::vector<int>& rowPositions() const;             // y[r]
      const std::vector<int>& columnCuts() const;               // cx[b], size C-1
      const std::vector<int>& rowCuts() const;                  // cy[b], size R-1
      // storage pixel at level `downsample`, for output pixels [start, start+count),
      // clamped to [0, storageExtent-1]
      std::vector<int> mapColumns(int start, int count, int downsample, int storageWidth) const;
      std::vector<int> mapRows(int start, int count, int downsample, int storageHeight) const;
  };
  }
  ```

How the geometry is built:

- **Boundaries.** A horizontal joint belongs to column boundary `c1`, the
  column of its tile 1. A vertical joint belongs to row boundary `r2`, the row
  of its tile 2 (the upper tile).
- **Median.** With an even count, the median is the mean of the two middle
  values. It is rounded half away from zero (`std::lround`).
- **Fallbacks (§5.1).** Use the AOI median for the axis, then the slide median,
  then 0. Log at INFO once per AOI per axis for each fallback taken.
- **Clamping.** Clamp to `[0, Tw−1]` or `[0, Th−1]`, and log at WARNING when a
  value is clamped.
- **Positions, size, cuts and maps** follow §5.2, §5.3 and §5.5 exactly.
  - `mapColumns` finds the column with `std::upper_bound` over `columnCuts()`.
  - It returns
    `min(floor((originX + c·Tw + (X·d − x[c])) / d), storageWidth − 1)`.

- [ ] **Step 1: Write the tests.** They build `Aoi` structs directly; no XML.
  - **DP 200 pattern:** a 23×21 AOI with 1024² tiles. Every row has horizontal
    joints with overlap 24 at boundaries 3, 7, 11, 15 and 19, and 0 at the
    other boundaries. Vertical overlaps are all 0. Expect:
    - `columns[3] == 24`, `columns[0] == 0`;
    - `size() == (23·1024 − 5·24, 21·1024)`, i.e. (23432, 21504);
    - `columnCuts()[3] == x[4] + 12`.
  - **Outlier robustness:** one boundary with overlaps {110, 112, 113, 187}
    gives 113. That is `lround(112.5)`; pin the value.
  - **Fallbacks:** a boundary with no joints takes the AOI median. An AOI with
    no vertical joints takes `slideMedianY`. With nothing at all, the overlap
    is 0.
  - **Clamping:** an overlap of 2000 on 1024 tiles gives 1023, and −5 gives 0.
  - **Maps at d = 1:** for a 2×1 AOI with overlap 100:
    - output column `x[1] + 50 − 1` maps to tile 0;
    - `x[1] + 50` maps to tile 1, at storage `1024 + 50`.
  - **Maps at d = 2:** the same AOI gives `floor(… / 2)`.
  - **Coarse level** (Review Focus 2): a 1×75 AOI with `Th = 1360`, `d = 32`
    and `storageHeight = 3188`. The last output row maps to `≤ 3187`, and
    every value is non-decreasing.
- [ ] **Step 2:** Run `--gtest_filter=BIFGeometry*`. Expect FAIL.
- [ ] **Step 3:** Implement.
- [ ] **Step 4:** Run again. Expect PASS.
- [ ] **Step 5: Commit.** Message: `feat(bif): per-boundary stitching geometry`.

### Task 6: Driver, slide, scenes and aux images — open path

**Files:**
- Create in `src/slideio/drivers/bif/`: `bifimagedriver.hpp/.cpp`,
  `bifslide.hpp/.cpp`, `bifscene.hpp/.cpp` and `bifsmallscene.hpp/.cpp`.
- Modify `src/slideio/slideio/imagedrivermanager.cpp`:
  - include the header;
  - add a `make_shared<BIFImageDriver>()` block in `initialize()`;
  - add `"BIF"` to `driverOrder` just before `"GDAL"`.
- Test: `src/tests/bif/test_bif_corpus_open.cpp`, and a `TestEntity("BIF", …)`
  row in `src/tests/main/test_imagedrivermanager.cpp`.

**Interfaces:**
- Consumes: Tasks 1, 2, 3 and 5.
- Produces:
  - `BIFImageDriver : ImageDriver`.
  - `BIFSlide : CVSlide`, with
    `static std::shared_ptr<BIFSlide> openFile(const std::string& path)`.
  - `BIFScene : CVScene`, with the constructor
    `BIFScene(const std::string& path, const std::string& name, int sceneIndex, std::vector<TiffDirectory> levels, AoiGeometry geometry, Aoi aoi, AcquisitionInfo acq)`.
    Its read method is a stub that throws until Task 7.
  - `BIFSmallScene : CVScene`. It is constructed from `(path, name, TiffDirectory)`
    and reads the whole directory with `readTiledDir` or `readStripedDir`, then
    resizes. It is an aux scene. It does not reuse `SVSSmallScene`: that would
    link bif to svs.

What `BIFSlide::openFile` does:

1. Scans the directories and reads the IFD 0 packet. `parseAcquisitionXml`
   throws the "not a Ventana BIF" error.
2. Classifies each IFD (§4.3) and validates the levels:
   - in sequence;
   - tiled;
   - all with the same tile size;
   - `d = mag0/magL` integral within 1e-6.
3. Refuses Z > 1 when `acq.zLayers > 1`, or when `readImageDepth` is > 1 on any
   level, with the exact message from the Global Constraints.
4. Reads the level-0 packet.
   - **Present:** `parseStitchXml`. For each scanned AOI:
     - validate it against level 0: the origin is a multiple of the tile, the
       grid lies inside the storage raster, and the `ImageInfo` tile size
       equals the TIFF tile;
     - then estimate overlaps (with the slide medians), build an
       `AoiGeometry`, and create the scene `AOI<index>`.
   - **Absent:** build one synthetic `Aoi` with origin 0, the grid set to
     `ceil(W/Tw) × ceil(H/Th)` and no joints, and one scene named `Image`.
     The scene rect is `geometry.size()` intersected with the level-0 storage
     size.
5. Sets `m_rawMetadata` to the IFD 0 packet, with `MetadataFormat::XML`, and
   builds the aux scenes, each with its own `iccProfile`.

Scene properties follow the §4.6 table:

- `getRawMetadata()` returns `aoi.imageInfoXml`;
- the ICC profile comes from the level-0 IFD;
- `m_levels` holds one `LevelInfo(l, size/d, 1.0/d, mag, tileSize)` per level;
- `supportsConcurrentReads()` is `true`.

- [ ] **Step 1: Write `BIFCorpusOpen.*` tests.** Use `getTestImagePath("bif/openslide", …)`.
  - **Scene names:** OS-1 has `{AOI0}`; OS-2 has `{AOI0, AOI2}`; Ventana-1 has
    `{AOI0}`.
  - **Sizes:** each scene's size equals §5.2 recomputed in the test from the
    XML, using the Task 3 and Task 5 functions. For Ventana-1 AOI0, assert
    (23432, 21504) literally.
  - **Resolution:** 0.2325e-6 for OS and 0.25e-6 for Ventana-1.
  - **Magnification:** 40.
  - **Level count:** 10 for OS and 8 for Ventana-1.
  - **ICC profile:** non-empty on Ventana-1's scene only.
  - **Aux names:** OS-1 has `{Label, Thumbnail}`; Ventana-1 has
    `{Label, Probability}`.
  - **Raw metadata:** the slide's contains `iScan`, and a scene's contains
    `ImageInfo` but not `TileJointInfo`.
  - **Not-BIF** (Review Focus 1): copy any small SVS from the test data to a
    temp `x.bif`. `Slide::openSlide(path, "AUTO")` must throw with a message
    containing `not a Ventana BIF file`.
- [ ] **Step 2:** Add the `TestEntity("BIF", "<SLIDEIO_IMAGES_PATH>/bif/openslide/Ventana-1.bif")`
  row to the driver-manager test.
- [ ] **Step 3:** Run `slideio_bif_tests --gtest_filter=BIFCorpusOpen*` and
  `slideio_tests --gtest_filter=*ImageDriverManager*`. Expect FAIL.
- [ ] **Step 4:** Implement.
- [ ] **Step 5:** Run both again. Expect PASS. Also run the corpus-free
  `slideio_bif_tests --gtest_filter=-BIFCorpus*`. Expect PASS.
- [ ] **Step 6: Commit.** Message: `feat(bif): open BIF slides as one scene per AOI`.

### Task 7: Read path

**Files:**
- Modify `src/slideio/drivers/bif/bifscene.hpp/.cpp`.
- Test: `src/tests/bif/test_bif_corpus_read.cpp`.

**Interfaces:**
- Consumes:
  - `AoiGeometry::mapColumns` and `mapRows`;
  - `TiffTools::readTile`;
  - `CVScene::readClampedBlock`;
  - `Tools::findZoomLevel` and `Tools::scaleRect`.
- Produces:
  - `readResampledBlockChannelsEx`, which picks a level the way SVS does, then
    delegates to `readResampledLevelBlockChannelsEx`. That override wraps
    `renderLevelRect` in `readClampedBlock` and resizes with `INTER_AREA` when
    `blockSize` differs from the rect.
  - `private: void renderLevelRect(int level, const cv::Rect& rect, const std::vector<int>& channels, cv::Mat& out, ContextPool::Borrow& ctx) const;`
  - `class BIFReadContext : public ReadContext { TIFFKeeper keeper; }`, the
    `SVSReadContext` pattern. `ContextPool m_contextPool` is the last data
    member, for destruction order.

How `renderLevelRect` works:

1. Build the column and row maps for `rect`, with `d = 2^level` (more
   precisely, mag0/magL) and the level's storage size.
2. Split each map into runs of the same storage tile index (`value / Tw`).
3. For each pair of a column run and a row run, decode that tile once with
   `readTile(..., channels, tile)`. Then, for each output row in the row run,
   copy the column run's pixels: `memcpy` over stretches where consecutive map
   values differ by 1, and pixel by pixel otherwise.

A storage tile that no map references is never decoded.

- [ ] **Step 1: Write `BIFCorpusRead.*` tests.**
  - **SeamMatchesSources:** for Ventana-1 boundary 3 and two OS-1 column
    boundaries and one row boundary:
    - read a 64 px wide strip centred on the cut at level 0;
    - build the expected strip by decoding the two storage tiles with
      `TiffTools::readTile` and placing them by the geometry's `x[]` and
      cut. The strips must be equal byte for byte.
  - **SeamContinuity:** at the same OS-1 cuts, the mean absolute difference
    between output columns `cut−1` and `cut` is at most 1.5× the median of
    the same statistic over the 10 column pairs either side.
  - **LevelEqualsScaled:** `readResampledLevelBlockChannels(2, r, r.size())`
    is equal to `readResampledBlockChannels(r·4, r.size())`.
  - **ScaledEqualsResize** (Review Focus 3): a read of `(rect, rect.size()/3)`
    equals `cv::resize(native, INTER_AREA)`, within 1 per channel.
  - **ChannelOrder** (Review Focus 4): the channels `{2,0}` equal planes 2 and
    0 of the full read.
  - **CoarseEdge** (Review Focus 2): OS-1 level 9, the full-level rect and the
    last row/column 1-px reads succeed, and their size is as requested.
  - **PastEdgeBackground:** a rect overhanging the AOI by 100 px has the
    overhang equal to 255.
  - **Concurrency** (Review Focus 5): `TestTools::concurrentReadIdentityTestAllScenes`
    on OS-2, and on Ventana-1.
- [ ] **Step 2:** Run `--gtest_filter=BIFCorpusRead*`. Expect FAIL (the stub
  throws).
- [ ] **Step 3:** Implement.
- [ ] **Step 4:** Run again. Expect PASS. Then run all of `slideio_bif_tests`.
- [ ] **Step 5: Commit.** Message: `feat(bif): stitched level reads with per-tile decode`.

Deviation from spec §6, to note in the PR: there is no banded rendering. Level
selection keeps the source within about 4× the output, the same bound SVS
lives with. Banding stays a follow-on if a read proves too large.

### Task 8: Repository integration

**Files:**
- Modify:
  - `src/tests/main/test_scene_index.cpp`, `test_scene_lifetime.cpp` and
    `test_edge_reads.cpp`: one row each, on Ventana-1.
  - `auxfiles/list-test-images.py`: the `BINARY` map.
  - `auxfiles/package-smoke/main.cpp`: `kExpectedDrivers`.
  - `cmake-scripts/packaging.cmake`: the Debian description.
  - `README.md`: the format table.
  - `CLAUDE.md`: "14 formats … 13 driver libraries", the format list, the
    `drivers/` tree, the test-executable list, and the "Concurrent today" list.
  - `.github/workflows/build-validation.yml` and `release.yml`: a
    `slideio_bif_tests --gtest_filter=-BIFCorpus*` line next to each
    `slideio_mrxs_tests` line, with `.exe` on Windows.
  - `software-docs/TEST_IMAGES.md`: regenerate it.

- [ ] **Step 1:** Make the edits.
- [ ] **Step 2:** Run `python3 auxfiles/list-test-images.py`, then
  `git diff --stat software-docs/TEST_IMAGES.md`. The three BIF files must
  appear.
- [ ] **Step 3:** Build all targets. Run `slideio_tests` with
  `SLIDEIO_SKIP_MISSING_IMAGES=1` and `slideio_bif_tests`. Expect PASS. Then
  build the package-smoke consumer against a local package
  (`python3 install.py -a package-only -c release`, then build
  `auxfiles/package-smoke/`). It must list `BIF`.
- [ ] **Step 4: Commit.** Message: `chore(bif): register BIF across tests, packaging, CI and docs`.

### Task 9: Manual sign-off (user)

- [ ] The user (or a pathologist) views OS-1, OS-2 (both scenes) and Ventana-1
  at 40×, 10× and 2.5× in a slideio-backed viewer, side by side with QuPath.
  Any visible seam artefact blocks the release. Record the outcome, with the
  date, in design §8.
