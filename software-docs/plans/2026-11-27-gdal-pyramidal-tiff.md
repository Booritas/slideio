# GDAL pyramidal TIFF and concurrent reads: implementation plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** a generic TIFF opened by the GDAL driver exposes its pyramid as zoom levels of one scene, and every GDAL scene reads concurrently.

**Architecture:** a pure grouping function turns `TiffDirectory`s into scene groups. A new `GDALTiffScene` reads a group through a `ContextPool` of per-borrow libtiff handles. `GDALScene` shrinks to the FreeImage path. `GDALSlide` picks one of the two scene types.

**Tech Stack:** C++17, libtiff (via `slideio/imagetools/libtiff.hpp`), OpenCV, GoogleTest, CMake/VS 2022.

**Spec:** `software-docs/specs/2026-11-27-gdal-pyramidal-tiff-design.md`. Read it first. The grouping rules 1-7 are normative and are not repeated here.

## Global Constraints

- The driver id stays `"GDAL"`. No new library, no change to `findDriver` order.
- Per-read state lives in `ContextPool`, never in `thread_local`. The pool member is declared **last** in `GDALTiffScene`.
- The pool factory captures the file path **by value**, never `this`.
- Level 0 of every file that opens today reads bit-identically to today.
- `readTile` keeps the GDAL behaviour: edge tiles are cropped to the valid area, and a tile that decodes to the wrong size or type throws. There is no zero-fill.
- Line numbers go stale. Every anchor in this plan is greppable text.
- Build: `cmake --build build --config Release --target slideio_tests`. Run: `build/bin/Release/slideio_tests.exe --gtest_filter=...`. Set `SLIDEIO_SKIP_MISSING_IMAGES=1` locally.
- All AI-written code is human-reviewed before it merges. This plan commits only on `feature/gdal-pyramidal-tiff`.

## Review Focus

1. **Planar (`PLANARCONFIG_SEPARATE`) pyramids.** Reading a reduced level of a planar file, with a channel subset, returns that level's pixels. The test is in Task 3.
2. **Odd sizes at reduced levels.** In a 1001x701 → 501x351 → 251x176 pyramid, a scaled `readBlock` over the bottom-right corner matches `cv::resize(INTER_AREA)` of the expected level within ±1, and padding never shows. The test is in Task 3.
3. **A pyramid scene outliving its slide.** Drop the slide, then read every level. The test is in Task 3.
4. **Auto-detection.** `slideio::openSlide(path)` with no driver id on a generic pyramidal `.tif` still lands in `GDAL` with the pyramid. The test is in Task 3.
5. **16-bit grayscale COG layout.** Reduced IFDs of a `uint16` single-channel base group correctly and read back exactly. The test is in Task 3.

---

### Task 1: Grouping function

**Files:**
- Create: `src/slideio/drivers/gdal/gdaltiffgrouping.hpp`, `src/slideio/drivers/gdal/gdaltiffgrouping.cpp`
- Modify: `src/slideio/drivers/gdal/CMakeLists.txt` (add both to `SOURCE_FILES`)
- Test: create `src/tests/main/test_gdal_tiff_grouping.cpp` and add it to `src/tests/main/CMakeLists.txt` next to `test_gdal_driver.cpp`
- Modify: `.github/workflows/build-validation.yml` and `.github/workflows/release.yml`. Every corpus-free `slideio_tests` filter (grep `FileReader.*:ContextPool.*`) gains `:GDALTiffPyramid.*`.

**Interfaces:**
- Produces:
  ```cpp
  namespace slideio {
      struct GDALTiffSceneGroup {
          int firstDirectory;                 // top-level IFD index the scene is ordered by
          std::vector<TiffDirectory> levels;  // levels[0] is the base; descending width
      };
      SLIDEIO_GDAL_EXPORTS std::vector<GDALTiffSceneGroup>
          groupTiffDirectories(const std::vector<TiffDirectory>& topLevel);
  }
  ```
  The result is sorted by `firstDirectory`. SubIFDs are read from `topLevel[i].subdirectories`.

- [ ] **Step 1: Write failing tests** in suite `GDALTiffPyramid`, using a helper `dir(w, h, channels, DataType, bits, subFileType)` that builds a `TiffDirectory`:
  - `plainPagesStayScenes`: three 256x256 pages → 3 groups, 1 level each, `firstDirectory` 0,1,2.
  - `subIfdsBecomeLevels`: base 1000x800 with SubIFDs 500x400 and 250x200 → 1 group, level widths {1000,500,250}.
  - `reducedIfdsBecomeLevels`: base 1000x800, then top-level 500x400 and 250x200 with `subFileType=1` → 1 group, 3 levels.
  - `masksAreSkipped`: base, reduced 500x400, then `subFileType=4` and `subFileType=5` pages → 1 group, 2 levels. A SubIFD with `subFileType=4` of the same format as the base is not a level.
  - `onlyMasksFallBackToScenes`: two `subFileType=4` pages → 2 groups.
  - `mismatchedReducedPageIsOwnSceneAndDoesNotEndRun`: order A(1000x800,3ch), r1(500x400,3ch,sft=1), x(500x400,1ch,sft=1), r2(250x200,3ch,sft=1) → 2 groups. Group 0 has `firstDirectory` 0 and widths {1000,500,250}. Group 1 has `firstDirectory` 2 and 1 level.
  - `aspectRatioMismatchIsNotALevel`: base 1000x800 with reduced 300x300 → 2 groups. Base 1001x701 with reduced 501x351 and 251x176 → 1 group, 3 levels (within the 1-pixel tolerance).
  - `notSmallerIsNotALevel`: a reduced page the same size as the base → its own scene.
  - `sizeTieKeepsSubIfd`: base with SubIFD 500x400 (`bitsPerSample` 8, `description` "sub") and top-level reduced 500x400 (`description` "top") → 2 levels, and `levels[1].description == "sub"`.
  - `reducedBeforeAnyBaseIsOwnScene`: the first page has `subFileType=1` → its own group.
  - `twoPyramidsInOneFile`: A, rA, B, rB → 2 groups with 2 levels each.

- [ ] **Step 2: Run and confirm the build fails** because `groupTiffDirectories` is undefined.
  Run: `cmake --build build --config Release --target slideio_tests`

- [ ] **Step 3: Implement `groupTiffDirectories`.** Apply spec rules 1-7. The aspect check is `abs(h - round(baseH * w / (double)baseW)) <= 1 && abs(w - round(baseW * h / (double)baseH)) <= 1`. Candidates are SubIFDs first, then top-level reduced IFDs. Sort accepted levels by width, descending, with a stable sort so a tie keeps the earlier candidate.

- [ ] **Step 4: Run the tests and confirm they pass.**
  Run: `build/bin/Release/slideio_tests.exe --gtest_filter=GDALTiffPyramid.*`

- [ ] **Step 5: Commit** `feat(gdal): group tiff directories into pyramids` (sources, CMake, test, both workflows).

---

### Task 2: Shared directory metadata

**Files:**
- Modify: `src/slideio/imagetools/smalltiffwrapper.hpp` / `.cpp`
- Test: `src/tests/main/test_gdal_tiff_grouping.cpp`

**Interfaces:**
- Produces: `static std::string SmallTiffWrapper::directoryMetadata(const TiffDirectory& dir);`, which is public and returns exactly the JSON that `SmallTiffPage::extractMetadata` builds today.

- [ ] **Step 1: Write failing test** `GDALTiffPyramid.directoryMetadataMatchesPage`. For a tiled directory with `description` and `software` set, check that the parsed JSON has `width`, `height`, `tiled==true`, `tileWidth`, `tileHeight`, `channels`, `dataType`, `compression`, `description` and `software`, and no `rowsPerStrip`. For a striped one, check that it has `rowsPerStrip` and `stripSize` and no `tileWidth`.
- [ ] **Step 2: Run and confirm it fails** because the function doesn't exist.
- [ ] **Step 3: Move the body of `extractMetadata` into `directoryMetadata`.** `extractMetadata` then calls it.
- [ ] **Step 4: Run** `--gtest_filter=GDALTiffPyramid.*:GDALDriver.metadata*` and confirm both pass. The `GDALDriver` corpus tests may skip.
- [ ] **Step 5: Commit** `refactor(imagetools): expose tiff directory metadata builder`.

---

### Task 3: `GDALTiffScene` and `GDALSlide` wiring

**Files:**
- Create: `src/slideio/drivers/gdal/gdaltiffscene.hpp`, `src/slideio/drivers/gdal/gdaltiffscene.cpp` (add both to CMake)
- Modify: `src/slideio/drivers/gdal/gdalslide.cpp`, `gdalscene.hpp`, `gdalscene.cpp`
- Create: `src/tests/main/gdaltiffwriter.hpp`, `src/tests/main/gdaltiffwriter.cpp`. Move `makeRgbImage`, `writeTiledRgbDirectory` and `writeTiledRgbTiff` out of the anonymous namespace in `test_gdal_driver.cpp` and add them to the tests' CMake.
- Test: `src/tests/main/test_gdal_tiff_pyramid.cpp`, added to CMake
- Docs: `software-docs/BREAKING_CHANGES.md`, `CHANGELOG.md`, `software-docs/TECH_DEBT.md`

**Interfaces:**
- Consumes: `groupTiffDirectories` and `GDALTiffSceneGroup` (Task 1), and `SmallTiffWrapper::directoryMetadata` (Task 2).
- Produces:
  ```cpp
  class GDALReadContext : public ReadContext {   // in gdaltiffscene.hpp
  public: explicit GDALReadContext(const std::string& filePath); TIFFKeeper keeper; };
  class SLIDEIO_GDAL_EXPORTS GDALTiffScene : public CVScene, public Tiler {
  public:
      GDALTiffScene(const std::string& filePath, const std::string& driverId,
                    std::vector<TiffDirectory> levels);
      // CVScene: getFilePath, getSceneIndex()==0, getDriverId, getNumChannels,
      // getChannelDataType, getResolution, getMagnification()==0, getName()=="",
      // getRect, getCompression, getMetadataFormat()==JSON, getRawMetadata,
      // getAcquisitionTime, getColorProfile -- all from levels[0]
      void readResampledBlockChannelsEx(...) override;
      void readResampledLevelBlockChannelsEx(int level, ...) override;
      // Tiler: getTileCount, getTileRect, readTile, initializeBlock -- userData is
      // {const TiffDirectory*, GDALReadContext*}
  private:
      ContextPool m_contextPool;   // last
  };
  ```
  Test writer helpers in `gdaltiffwriter.hpp`, namespace `gdaltest`:
  `cv::Mat makeRgbImage(int w, int h, int seed = 0)`, `cv::Mat makeGrayImage16(int w, int h, int seed = 0)`, and
  `void writeTiledDirectory(libtiff::TIFF*, const cv::Mat& image, int tileSize, bool planar, uint8_t padValue, uint32_t subFileType = 0, int numSubIfds = 0)`, which takes 1 or 3 channels, `CV_8U` or `CV_16U`, sets `TIFFTAG_SUBFILETYPE`, sets `TIFFTAG_SUBIFD` to `numSubIfds` zero offsets when it is above 0, and calls `TIFFWriteDirectory`. Also `void writeStripedDirectory(libtiff::TIFF*, const cv::Mat& image, uint32_t subFileType = 0, int numSubIfds = 0)`. The SubIFDs are the next `numSubIfds` `writeXxxDirectory` calls, as libtiff requires. Keep the old `writeTiledRgbDirectory` signature as a wrapper so the existing tests compile unchanged.

- [ ] **Step 1: Write failing tests** in suite `GDALTiffPyramid` (`test_gdal_tiff_pyramid.cpp`). Each writes into a `slideio::TempFile("tif")` and opens it with `slideio::GDALImageDriver`. Expected level rasters are made by `cv::resize(INTER_AREA)` of the base image, and those exact rasters are written, so reads compare with `NORM_INF == 0`.
  - `subIfdPyramidOpensAsOneScene`: an RGB tiled 1000x800 base (tile 256) with SubIFDs 500x400 and 250x200. Expect 1 scene and `getNumZoomLevels()==3`. Each `LevelInfo` has the right size, a tile size of 256, and scale = level width / 1000. `readResampledLevelBlockChannels`-style reads (through `CVScene::readResampledLevelBlockChannelsEx` via the public `Scene`) of the full extent of each level equal what was written.
  - `cogLayoutOpensAsOneScene`: the same pyramid as top-level IFDs with `subFileType=1`, plus a 1-channel `subFileType=4` mask page → 1 scene, 3 levels.
  - `uint16GrayCog` *(Review Focus 5)*: a 16-bit single-channel 600x400 base with reduced 300x200 → 1 scene, 2 levels, `DT_UInt16`, exact reads.
  - `planarPyramidChannelSubset` *(Review Focus 1)*: a planar RGB SubIFD pyramid. Reading level 1 with channels {2,0} equals the written level's channels 2 and 0, merged.
  - `oddSizedLevelsScaledReadAtEdge` *(Review Focus 2)*: a 1001x701 base with reduced 501x351 and 251x176, tile 64, pad value 255. `readBlock` of rect (751,501,250,200) scaled to (63,50) matches `cv::resize(INTER_AREA)` of the same rect taken from the written 251x176 level (scaled to level coordinates) within ±1. The result contains no pixel that equals the pad pattern in an all-255 row.
  - `scaledReadPicksReducedLevel`: on the SubIFD pyramid, a `readBlock` of the full 1000x800 rect into a 250x200 output equals level 2 exactly. That's evidence that level 2 was read, not level 0 resized.
  - `stripedBaseTiledLevels`: a striped 600x400 base with tiled reduced levels → 1 scene. Level 0 is the striped raster, and the level tile sizes are {600x400, 64x64}.
  - `plainMultiPageUnchanged`: two independent 600x500 tiled pages → 2 scenes, 1 level each. This is the existing `twoPagesOfOneTiffReadConcurrently` file shape.
  - `pyramidSceneOutlivesSlide` *(Review Focus 3)*: open, take scene 0, reset the slide, then read every level.
  - `autoDetectionLandsInGdal` *(Review Focus 4)*: `slideio::openSlide(path, "")` on the SubIFD pyramid → `getDriverId()=="GDAL"`, 1 scene, 3 levels.
- [ ] **Step 2: Run and confirm they fail.** The pyramid tests report multiple scenes or 1 level.
  Run: `build/bin/Release/slideio_tests.exe --gtest_filter=GDALTiffPyramid.*`
- [ ] **Step 3: Implement `GDALTiffScene`.** Level selection follows `SVSTiledScene::readResampledBlockChannelsEx` (grep it): `Tools::findZoomLevel`, scale the rect into the level, then call the level read. The level read borrows once and routes to `TileComposer::composeRect` for tiled levels. Striped levels go through `TiffTools::readStripedDir`, then crop and resize with `INTER_LINEAR`, as `GDALScene::readWholePageBlock` does today. `readTile` and `getTileRect` port the current `GDALScene` versions (edge crop, size and type check), taking the directory from `userData`. Use `readClampedBlock` the way `GDALScene::readResampledBlockChannelsEx` does today. `supportsConcurrentReads()` stays at its default `false` in this task.
- [ ] **Step 4: Rewire `GDALSlide`.** Call `ImageTools::openSmallImage`. If it `dynamic_cast`s to `SmallTiffWrapper`, copy its directories (`getNumPages`/`getDirectory`), release the wrapper so no handle outlives the constructor, and build one `GDALTiffScene` per `groupTiffDirectories` group. Otherwise build `GDALScene`s per page as today. Remove `readMutex` from `GDALSlide`, and from `GDALScene` remove the tiled-TIFF branch, `m_tiff`, `m_tiledDirectory`, the `Tiler` base and methods, and the `readSerialisationMutex` override. Raw slide metadata is the single scene's when `getNumScenes()==1`.
- [ ] **Step 5: Run the new suite and the existing GDAL suite.**
  Run: `build/bin/Release/slideio_tests.exe --gtest_filter=GDALTiffPyramid.*:GDALDriver.*`
  Expected: all PASS. Corpus tests may SKIP, but on a machine with the corpus they must PASS, including `GDALDriver.sceneOutlivesItsSlide` and the `тест/тест.tif` test.
- [ ] **Step 6: Update the docs.**
  - `BREAKING_CHANGES.md`, under a `feature/gdal-pyramidal-tiff` heading: TIFFs with SubIFDs or reduced-resolution pages open as fewer GDAL scenes, and mask pages are no longer scenes unless the file holds nothing else.
  - `CHANGELOG.md`: pyramid support for generic TIFFs in the GDAL driver.
  - `TECH_DEBT.md`: a new section saying striped GDAL TIFF levels are decoded whole on every block.
- [ ] **Step 7: Commit** `feat(gdal): open generic tiff pyramids as one scene`.

---

### Task 4: Concurrent reads

**Files:**
- Modify: `gdaltiffscene.hpp`, `gdalscene.hpp`
- Test: `src/tests/main/test_concurrency_contract.cpp`, `src/tests/main/test_gdal_tiff_pyramid.cpp`, `src/tests/main/test_gdal_driver.cpp`
- Docs: `CLAUDE.md`

**Interfaces:**
- Consumes: `GDALTiffScene` (Task 3) and test writers in `gdaltiffwriter.hpp`.
- Produces: `bool supportsConcurrentReads() const override { return true; }` on both scene classes.

- [ ] **Step 1: Write failing tests.**
  - Rename `ConcurrencyContract.gdalIsStillSerialised` to `gdalReadsConcurrently` and assert `scene->supportsConcurrentReads()` is true. That covers the FreeImage JPEG.
  - Add `GDALTiffPyramid.tiffSceneReportsConcurrent`.
  - Add `GDALTiffPyramid.concurrentRandomReadsMatchSerial`: on the SubIFD pyramid, 8 threads × 50 reads, each a random level, a random rect and a random output scale from a fixed seed per thread. Every result equals the same read done serially beforehand.
  - Add `GDALTiffPyramid.concurrentFreeImageReadsMatchSerial`: the same check on a 1200x900 RGB PNG written with `cv::imwrite` into a `TempFile("png")`, with a single level.
- [ ] **Step 2: Run and confirm** that both `supportsConcurrentReads` assertions fail.
- [ ] **Step 3: Add the two overrides.** Rewrite the comment above `GDALDriver.twoPagesOfOneTiffReadConcurrently` (grep `The pages of one file decode through one libtiff handle`) to say that each read borrows its own handle.
- [ ] **Step 4: Run** `--gtest_filter=GDALTiffPyramid.*:GDALDriver.*:ConcurrencyContract.*` and confirm they PASS. Run the concurrent test 20 times (`--gtest_repeat=20 --gtest_filter=GDALTiffPyramid.concurrent*`) and confirm there are no failures.
- [ ] **Step 5: Update `CLAUDE.md`.** In the "Concurrent today:" sentence, add GDAL to the list.
- [ ] **Step 6: Commit** `feat(gdal): report concurrent reads`.
