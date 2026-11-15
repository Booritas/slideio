# Open options and MRXS crop-to-content Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Let callers pass driver-namespaced string options to `openSlide`, and use the first one, `mrxs.crop_to_content`, to present an MRXS slide cropped to its scanned area.

**Architecture:** A new `openSlide(path, driver, options)` overload goes to `ImageDriverManager`. The manager validates key prefixes and hands the chosen driver its own keys, with the prefix stripped, through a new non-pure `ImageDriver::openFile(path, options)`. Scenes report the options they were opened with (`CVScene::getOpenOptions`), so the converter's per-thread re-open reproduces them. MRXS computes a crop box once per open slide in `MRXSDataset`, and `MRXSScene` translates both read paths into nominal coordinates.

**Tech Stack:** C++17, CMake, Google Test, OpenCV, pybind11 (slideio-python), Python 3 + Pillow (fixture generator).

**Spec:** `software-docs/specs/2026-11-15-open-slide-options-design.md`. Read it first. Section numbers below (§n) refer to it.

## Global Constraints

- Work on branch `feature/open-slide-options` in `slideio`, and on a branch of the same name in `slideio-python` (Task 7).
- The existing `SLIDEIO_EXPORTS std::shared_ptr<Slide> openSlide(const std::string& path, const std::string& driver= "")` stays byte-for-byte as declared. Its body only forwards. Never add a default argument to the new overload.
- `core/openoptions.hpp`, `imagedriver.hpp`, `cvscene.hpp`, `imagedrivermanager.hpp` and every MRXS header stay internal. Do not add any of them to a `public-headers.cmake`.
- `slideio.hpp` gains `#include <map>`. Public headers include the standard headers for every `std::` name they use.
- Key rules (§5): every key is `<driver-id>.<name>`. The prefix is everything before the first `.`, compared case-insensitively with `ImageDriver::getID()`, with no aliases. The name is case-sensitive. A key with no prefix, or an empty one, is rejected. An unknown driver prefix is rejected. Keys for other drivers are ignored. Booleans accept `true`/`false`/`1`/`0`, case-insensitive.
- Errors use `RAISE_RUNTIME_ERROR` and contain these substrings, which the tests assert:
  - no prefix: `names no driver`
  - unknown prefix: `unknown driver '<prefix>'`
  - option unknown to the driver: `has no open option '<lowercase-id>.<name>'`
  - bad boolean: `is not a boolean`

  Every message includes the full key as the caller wrote it, or as rebuilt from `<lowercase-id>.<name>`.
- **`mrxs.crop_to_content` defaults to `true`** (§7.0). Both `MRXSImageDriver::openFile` entry points crop when the key is absent. `MRXSSlide::openFile` takes the flag with no default. `MRXSDataset::open` keeps `false` as its default (internal, nominal geometry). `MRXSScene::getOpenOptions()` always states the key explicitly, as `"true"` or `"false"`.
- This ships as part of **2.11.0**: no version bump, and CHANGELOG entries go under the existing `[2.11.0]` section. The pushed `v2.11.0` tag is moved only in Task 9, with the user's explicit go-ahead.
- MRXS crop (§7): C = `bounds0` rounded outward, intersected with the level-0 canvas. Level box: `x0 = floor(C.x/d)`, `y0 = floor(C.y/d)`, `x1 = min(ceil((C.x+C.w)/d), level width)`, `y1 = min(ceil((C.y+C.h)/d), level height)`. No drawables or an empty intersection means no crop.
- Cropped reads are bit-identical to uncropped reads shifted by the crop origin (level reads: the level box origin; scaled reads: C's origin). Tests compare with `cv::norm(a, b, cv::NORM_INF) == 0`.
- Every new `slideio_mrxs_tests` suite is corpus-free and must not be named `MRXSCorpus*`.
- The committed fixtures in `src/tests/mrxs/data/` are exact bytes (`-text`). Only the new fixture and `manifest.json` may change when regenerating.
- Org policy: AI-generated code is human-reviewed before commit. At each **Commit** step, show the user the diff and wait for their go-ahead, unless they have waived this for the branch.
- Build: `py install.py -a build-only -c release` from the repo root. Test binaries are in the release bin directory (`build/release/bin/` on Linux/macOS, under `build/` on Windows). Below, that directory is written `<bin>`.

## Review Focus

1. **Case of option keys:** `MRXS.crop_to_content` works (the prefix is case-insensitive), while `mrxs.Crop_To_Content` throws `has no open option` instead of being silently ignored. The test is in Task 6.
2. **A read overhanging the cropped scene's edge** shows background beyond the edge, and pixels equal to the uncropped read inside it. The test is in Task 6.
3. **Associated images** (Label, Macro, Thumbnail) are unchanged by the crop. The test is in Task 6.
4. **A bad key on a path that does not exist** reports the key, not a file error: validation runs before any file is touched. The test is in Task 2.
5. **No option is the same as `true`, and `false` gives exactly the full canvas:** no `mrxs.crop` metadata, and `getOpenOptions()` reporting `false`, so a converter re-open stays uncropped. The tests are in Task 6.

---

### Task 1: Option-key helpers and the `ImageDriver` options entry point

**Files:**
- Create: `src/slideio/core/openoptions.hpp`, `src/slideio/core/openoptions.cpp`
- Modify: `src/slideio/core/imagedriver.hpp`, `src/slideio/core/imagedriver.cpp`, `src/slideio/core/CMakeLists.txt` (source list)
- Test: `src/tests/main/test_openoptions.cpp` (add to the list in `src/tests/main/CMakeLists.txt`)

**Interfaces:**
- Produces (namespace `slideio`, in `openoptions.hpp`):
  - `using DriverOptions = std::map<std::string, std::string>;`
  - `struct OptionKey { std::string driver; std::string name; };`
  - `SLIDEIO_CORE_EXPORTS OptionKey splitOptionKey(const std::string& key);` splits at the first `.`. With no `.`, `driver` is empty and `name` is the whole key.
  - `SLIDEIO_CORE_EXPORTS std::string optionKeyOf(const std::string& driverId, const std::string& name);` returns the lowercased id + `"."` + name.
  - `SLIDEIO_CORE_EXPORTS bool parseBoolOption(const std::string& key, const std::string& value);`
  - On `ImageDriver`: `virtual std::shared_ptr<CVSlide> openFile(const std::string& filePath, const DriverOptions& options);`. The options arrive with the prefix already stripped.

- [ ] **Step 1: Write the failing tests**

```cpp
TEST(OpenOptions, splitsAtTheFirstDot) {
    EXPECT_EQ("mrxs", splitOptionKey("mrxs.crop_to_content").driver);
    EXPECT_EQ("crop_to_content", splitOptionKey("mrxs.crop_to_content").name);
    EXPECT_EQ("a", splitOptionKey("a.b.c").driver);
    EXPECT_EQ("b.c", splitOptionKey("a.b.c").name);
    EXPECT_EQ("", splitOptionKey("nodot").driver);
    EXPECT_EQ("", splitOptionKey(".x").driver);
}
TEST(OpenOptions, keyOfLowercasesTheDriverId) {
    EXPECT_EQ("ometiff.x", optionKeyOf("OMETIFF", "x"));
}
TEST(OpenOptions, booleansAcceptFourSpellingsInAnyCase) {
    for (auto t : {"true", "TRUE", "True", "1"}) EXPECT_TRUE(parseBoolOption("k.b", t));
    for (auto f : {"false", "False", "0"}) EXPECT_FALSE(parseBoolOption("k.b", f));
    for (auto bad : {"yes", "", "2", " true"}) {
        try { parseBoolOption("k.b", bad); FAIL() << bad; }
        catch (const slideio::RuntimeError& e) {
            EXPECT_NE(std::string::npos, std::string(e.what()).find("is not a boolean"));
            EXPECT_NE(std::string::npos, std::string(e.what()).find("k.b"));
        }
    }
}
// TestDriver: getID() "TESTDRV", openFile(path) counts calls and returns nullptr.
TEST(OpenOptions, aDriverWithoutOptionsForwardsAnEmptyMap) {
    TestDriver d; d.openFile("p", DriverOptions{});
    EXPECT_EQ(1, d.calls);
}
TEST(OpenOptions, aDriverWithoutOptionsRejectsAnyKeyBeforeOpening) {
    TestDriver d;
    try { d.openFile("p", DriverOptions{{"x", "1"}}); FAIL(); }
    catch (const slideio::RuntimeError& e) {
        EXPECT_NE(std::string::npos, std::string(e.what()).find("has no open option 'testdrv.x'"));
    }
    EXPECT_EQ(0, d.calls);
}
```

The `TestDriver` needs `using ImageDriver::openFile;` so that both overloads are visible on it.

- [ ] **Step 2: Run the tests and confirm they fail**

Run: `py install.py -a build-only -c release`
Expected: the build fails because `openoptions.hpp` does not exist.

- [ ] **Step 3: Implement**

Implement the helpers in `openoptions.cpp`. The base `ImageDriver::openFile(path, options)` in `imagedriver.cpp` throws on the first key, naming `optionKeyOf(getID(), key)`, and otherwise returns `openFile(filePath)`. Doxygen on the new virtual: it states that a driver honouring options overrides this one *and* `CVScene::getOpenOptions()` (Task 3).

- [ ] **Step 4: Run the tests and confirm they pass**

Run: `<bin>/slideio_tests --gtest_filter="OpenOptions.*"`
Expected: 5 tests PASS.

- [ ] **Step 5: Commit**

```bash
git add src/slideio/core/openoptions.* src/slideio/core/imagedriver.* src/slideio/core/CMakeLists.txt src/tests/main/test_openoptions.cpp src/tests/main/CMakeLists.txt
git commit -m "feat(core): option-key helpers and an options-taking ImageDriver::openFile"
```

---

### Task 2: Prefix validation in the manager, and the public `openSlide` overload

**Files:**
- Modify: `src/slideio/slideio/imagedrivermanager.hpp`, `imagedrivermanager.cpp`, `slideio.hpp`, `slideio.cpp`, `slide.hpp`
- Test: `src/tests/main/test_imagedrivermanager.cpp`

**Interfaces:**
- Consumes: Task 1's `DriverOptions`, `splitOptionKey`, `ImageDriver::openFile(path, options)`.
- Produces:
  - `static std::shared_ptr<CVSlide> ImageDriverManager::openSlide(const std::string& filePath, const std::string& driver, const DriverOptions& options);`. The two-argument version forwards with `{}`.
  - In `slideio.hpp`: `using SlideOpenOptions = std::map<std::string, std::string>;` and `SLIDEIO_EXPORTS std::shared_ptr<Slide> openSlide(const std::string& path, const std::string& driver, const SlideOpenOptions& options);`.
  - In `slide.hpp`: a friend declaration for that overload, beside the existing one.

- [ ] **Step 1: Write the failing tests**

```cpp
namespace { std::string errorOf(const std::function<void()>& f) {
    try { f(); } catch (const std::exception& e) { return e.what(); } return ""; } }

TEST(ImageDriverManagerOptions, aKeyWithoutADriverPrefixIsRejected) {
    const auto msg = errorOf([] { ImageDriverManager::openSlide("missing.svs", "SVS", {{"crop", "1"}}); });
    EXPECT_NE(std::string::npos, msg.find("names no driver"));
    EXPECT_NE(std::string::npos, msg.find("crop"));
}
TEST(ImageDriverManagerOptions, anUnknownDriverPrefixIsRejected) {
    const auto msg = errorOf([] { ImageDriverManager::openSlide("missing.mrxs", "AUTO", {{"mrsx.crop_to_content", "1"}}); });
    EXPECT_NE(std::string::npos, msg.find("unknown driver 'mrsx'"));
}
TEST(ImageDriverManagerOptions, theChosenDriverReceivesItsKeysCaseInsensitivelyByPrefix) {
    // SVS takes no options: its base openFile rejects the key before touching the file.
    const auto msg = errorOf([] { ImageDriverManager::openSlide("missing.svs", "SVS", {{"Svs.x", "1"}}); });
    EXPECT_NE(std::string::npos, msg.find("has no open option 'svs.x'"));
}
TEST(ImageDriverManagerOptions, validationRunsBeforeTheFileIsTouched) {   // Review Focus 4
    const auto msg = errorOf([] { ImageDriverManager::openSlide("Z:/no/such/dir/x.mrxs", "AUTO", {{"nosuch.x", "1"}}); });
    EXPECT_NE(std::string::npos, msg.find("unknown driver 'nosuch'"));
}
```

The tests that need a readable file (keys for other drivers are ignored, and the two-argument call equals the three-argument call with an empty map) live in Task 6, on the MRXS fixtures.

- [ ] **Step 2: Run the tests and confirm they fail**

Run: build, then `<bin>/slideio_tests --gtest_filter="ImageDriverManagerOptions.*"`
Expected: compile error, because there is no three-argument `openSlide`.

- [ ] **Step 3: Implement**

Order in the manager's `openSlide`, before driver selection: reject empty prefixes, then reject prefixes that match no registered `getID()` (case-insensitive). Then select the driver exactly as today, build the stripped map of its own keys, and call `driver->openFile(filePath, stripped)`. The public overload is a copy of the existing `slideio::openSlide` body with options passed through. The existing one forwards to it with `{}`. Doxygen on the overload documents §5 and lists `mrxs.crop_to_content`.

- [ ] **Step 4: Run the tests and confirm they pass**

Run: `<bin>/slideio_tests --gtest_filter="ImageDriverManager*"`
Expected: all PASS, including the existing `ImageDriverManager` tests.

- [ ] **Step 5: Confirm the public header check still passes**

Run: build the `public_headers_check` target (part of the normal build).
Expected: `slideio.hpp` compiles standalone with `<map>`.

- [ ] **Step 6: Commit**

```bash
git commit -am "feat(slideio): openSlide overload taking driver-namespaced open options"
```

---

### Task 3: Scenes report their open options, and the converter re-opens with them

**Files:**
- Modify: `src/slideio/core/cvscene.hpp`, `src/slideio/converter/tiffconverter.cpp` (`tryReopenScene`)
- Test: `src/tests/converter/test_sourcescene_conversion.cpp`

**Interfaces:**
- Consumes: Task 2's three-argument `openSlide`.
- Produces: `virtual DriverOptions CVScene::getOpenOptions() const { return {}; }`. It returns full *prefixed* keys, ready to pass back to `openSlide`.

- [ ] **Step 1: Write the failing test**

This goes next to `aSceneMatchingItsReopenedFileIsReopened`, reusing `ReopenableScene`, `writeComputedOmeTiff`, `cloneOf` and `isShared`:

```cpp
namespace {
    class ScenesWithOptions : public ReopenableScene {
    public:
        using ReopenableScene::ReopenableScene;
        DriverOptions getOpenOptions() const override { return {{"ometiff.no_such_option", "1"}}; }
    };
}
TEST(SourceSceneConversion, aScenesOpenOptionsReachTheReopen) {
    // OMETIFF takes no options, so passing them makes the re-open throw and the
    // converter share the scene. Without them, the same file is re-opened (see
    // aSceneMatchingItsReopenedFileIsReopened).
    const std::string path = writeComputedOmeTiff("slideio_clone_options.ome.tiff");
    {
        auto scene = std::make_shared<ScenesWithOptions>(path, 0);
        std::pair<std::shared_ptr<Slide>, std::shared_ptr<Scene>> clone;
        ASSERT_NO_THROW(clone = cloneOf(scene));
        EXPECT_TRUE(isShared(clone, scene));
    }
    std::filesystem::remove(path);
}
```

- [ ] **Step 2: Run the test and confirm it fails**

Run: `<bin>/slideio_converter_tests --gtest_filter="SourceSceneConversion.aScenesOpenOptionsReachTheReopen"`
Expected: compile error (`getOpenOptions` is not virtual on `CVScene`). Once it is declared, the assertion fails because the scene is re-opened instead of shared.

- [ ] **Step 3: Implement**

Add the virtual. In `tryReopenScene`, call `openSlide(filePath, driverId, origin->getOpenOptions())`. Add the options to the existing warning text only if that is natural. No other converter change is needed.

- [ ] **Step 4: Run the converter suite and confirm it passes**

Run: `<bin>/slideio_converter_tests`
Expected: all PASS.

- [ ] **Step 5: Commit**

```bash
git commit -am "feat(converter): re-open a scene with the options it was opened with"
```

---

### Task 4: An MRXS fixture whose content starts at a positive offset

**Files:**
- Modify: `auxfiles/mrxs-fixtures/generate.py` (`base_specs()`)
- Create (generated): `src/tests/mrxs/data/offset-content.mrxs`, `src/tests/mrxs/data/offset-content/`, `src/tests/mrxs/data/offset-content.expected.json`
- Modify (generated): `src/tests/mrxs/data/manifest.json`

**Interfaces:**
- Produces: fixture `offset-content` with `bounds_level0 == [27.0, 9.0, 45.0, 45.0]`. Tasks 5 and 6 rely on these numbers. Its level-0 canvas is 72 × 54.

- [ ] **Step 1: Add the spec**

Append to `base_specs()` before `return specs`, with a one-line comment in the file's style:

```python
    # Content starting at (27, 9): neither offset divides by the coarser downsamples, so a
    # crop to the content box has non-zero, non-integer-aligned level origins.
    specs.append({
        "name": "offset-content", "slide_id": "0FF5E70FF5E70FF5E70FF5E70FF5E70F",
        "Nx": 8, "Ny": 6, "D": 1, "mpp": [0.25, 0.25], "fill_bgr": 0x00405060,
        "levels": [dict(exp=0, W=9, H=9, overlap_x=0, overlap_y=0, format="PNG"),
                   dict(exp=1, W=9, H=9, overlap_x=0, overlap_y=0, format="PNG"),
                   dict(exp=1, W=9, H=9, overlap_x=0, overlap_y=0, format="PNG")],
        "images0": [(x, y) for (x, y) in grid(8, 6, 1) if x >= 3 and y >= 1 and (x, y) != (6, 4)],
        "index_file": "Index.dat", "data_files": ["Data0000.dat"],
    })
```

- [ ] **Step 2: Regenerate**

Run: `py -I auxfiles/mrxs-fixtures/generate.py`
Expected: `wrote N fixtures to ...`, where N is one more than before.

- [ ] **Step 3: Check that only the new fixture changed**

Run: `git status --short src/tests/mrxs/data`
Expected: only `offset-content*` (untracked) and `manifest.json` (modified). If any existing fixture changed (e.g. a different Pillow version re-encoded the JPEGs), restore it with `git checkout -- <path>` and note the Pillow version in the commit message.

Run: `grep bounds_level0 src/tests/mrxs/data/offset-content.expected.json`
Expected: `"bounds_level0": [27.0,9.0,45.0,45.0]`. If not, adjust `images0` until it does; the later tasks depend on these numbers.

- [ ] **Step 4: Run the manifest-driven suites against the new fixture**

Run: `<bin>/slideio_mrxs_tests --gtest_filter=-MRXSCorpus*`
Expected: all PASS. `MRXSGeometry`, `MRXSIndex` and `MRXSRender` iterate `manifest.json`, so the new fixture is checked against the generator's independent expectations.

- [ ] **Step 5: Commit**

```bash
git add auxfiles/mrxs-fixtures/generate.py src/tests/mrxs/data/offset-content* src/tests/mrxs/data/manifest.json
git commit -m "test(mrxs): fixture with content starting at a positive offset"
```

---

### Task 5: The MRXS crop box

**Files:**
- Modify: `src/slideio/drivers/mrxs/mrxsdataset.hpp`, `mrxsdataset.cpp`
- Test: `src/tests/mrxs/test_mrxs_crop.cpp` (new; add to the source list in `src/tests/mrxs/CMakeLists.txt`)

**Interfaces:**
- Consumes: `Geometry` (`bounds0`, `levels[l].width/height/downsample`), and the fixtures from Task 4.
- Produces (namespace `slideio::mrxs`):
  - `struct CropBox { cv::Rect level0; std::vector<cv::Rect> levels; };`. `levels[l]` is in nominal level-*l* pixels, and `levels[0] == level0`.
  - `SLIDEIO_MRXS_EXPORTS std::optional<CropBox> computeCropBox(const Geometry& geometry);`
  - `MRXSDataset` gains `bool cropRequested = false;` and `std::optional<CropBox> crop;`. `crop` is set only when the crop was requested *and* `computeCropBox` found a box.
  - `MRXSDataset::open(const std::string& entryPath, const RuntimeOptions& options = RuntimeOptions(), bool cropToContent = false)`. The new parameter goes last, so the existing callers in `test_mrxs_cache.cpp` keep compiling.

- [ ] **Step 1: Write the failing tests**

```cpp
TEST(MRXSCropBox, isTheContentBoundsWhenTheyLieInsideTheCanvas) {
    const auto g = MRXSDataset::open(entry("offset-content"))->geometry;
    const auto box = computeCropBox(g);
    ASSERT_TRUE(box.has_value());
    EXPECT_EQ(cv::Rect(27, 9, 45, 45), box->level0);
    ASSERT_EQ(g.levels.size(), box->levels.size());
    EXPECT_EQ(box->level0, box->levels[0]);
    auto expect = [&](int l, int d, int x0, int y0, int cx1, int cy1) {
        const int x1 = std::min<int64_t>(cx1, g.levels[l].width), y1 = std::min<int64_t>(cy1, g.levels[l].height);
        EXPECT_EQ(int64_t(d), g.levels[l].downsample);
        EXPECT_EQ(cv::Rect(x0, y0, x1 - x0, y1 - y0), box->levels[l]);
    };
    expect(1, 2, 13, 4, 36, 27);    // floor(27/2), floor(9/2), ceil(72/2), ceil(54/2)
    expect(2, 4, 6, 2, 18, 14);     // floor(27/4), floor(9/4), ceil(72/4), ceil(54/4)
}
TEST(MRXSCropBox, isClippedToTheCanvas) {
    const auto g = MRXSDataset::open(entry("rawmap-d2"))->geometry;   // bounds (-3,-2,93,70)
    const auto box = computeCropBox(g);
    ASSERT_TRUE(box.has_value());
    EXPECT_EQ(cv::Rect(0, 0, std::min<int64_t>(90, g.levels[0].width),
                       std::min<int64_t>(68, g.levels[0].height)), box->level0);
}
TEST(MRXSCropBox, isAbsentWithoutDrawablesOrOutsideTheCanvas) {
    auto g = MRXSDataset::open(entry("nomap-d1"))->geometry;
    g.bounds0.reset();
    EXPECT_FALSE(computeCropBox(g).has_value());
    g.bounds0 = cv::Rect2d(double(g.levels[0].width) + 5, 0, 10, 10);
    EXPECT_FALSE(computeCropBox(g).has_value());
}
TEST(MRXSCropBox, theDatasetCarriesOneOnlyWhenAsked) {
    EXPECT_FALSE(MRXSDataset::open(entry("offset-content"))->crop.has_value());
    const auto cropped = MRXSDataset::open(entry("offset-content"), RuntimeOptions(), true);
    EXPECT_TRUE(cropped->cropRequested);
    ASSERT_TRUE(cropped->crop.has_value());
    EXPECT_EQ(cv::Rect(27, 9, 45, 45), cropped->crop->level0);
}
```

Copy `entry()` and `dataDir()` from `test_mrxs_geometry.cpp`'s anonymous namespace.

- [ ] **Step 2: Run the tests and confirm they fail**

Run: build. Expected: compile error, because there is no `computeCropBox`.

- [ ] **Step 3: Implement**

`computeCropBox` rounds `bounds0` outward exactly as `describe` does for `contentBounds` (`floor` of the origin, `ceil` of the far edge), intersects the result with the canvas, and then applies the per-level formula from Global Constraints. `open` calls it only when `cropToContent` is true.

- [ ] **Step 4: Run the tests and confirm they pass**

Run: `<bin>/slideio_mrxs_tests --gtest_filter="MRXSCropBox.*:MRXSCache*"`
Expected: all PASS.

- [ ] **Step 5: Commit**

```bash
git commit -am "feat(mrxs): compute the crop box from the content bounds"
```

---

### Task 6: MRXS `crop_to_content` (driver option, scene, reads, metadata, re-open)

**Files:**
- Modify: `src/slideio/drivers/mrxs/mrxsimagedriver.hpp/.cpp`, `mrxsslide.hpp/.cpp`, `mrxsscene.hpp/.cpp`, `mrxsdescription.cpp`
- Test: `src/tests/mrxs/test_mrxs_crop.cpp`

**Interfaces:**
- Consumes: Task 1's `parseBoolOption` and `optionKeyOf`; Task 3's `CVScene::getOpenOptions`; Task 5's `MRXSDataset::crop`, `cropRequested` and the 3-argument `open`.
- Produces:
  - `MRXSImageDriver`: `using ImageDriver::openFile;` plus an override of `openFile(const std::string&, const DriverOptions&)`. The only name it accepts is `crop_to_content`, and it is `true` when absent. `openFile(path)` calls `MRXSSlide::openFile(path, true)`.
  - `static std::shared_ptr<MRXSSlide> MRXSSlide::openFile(const std::string& filePath, bool cropToContent);`, with no default.
  - `MRXSScene::getOpenOptions()`: `{{"mrxs.crop_to_content", cropRequested ? "true" : "false"}}`, always present.
  - `describe()`: adds `mrxs["crop"] = {x, y, w, h}` (C, in nominal level-0 pixels) only when `dataset.crop` is set. `contentBounds` and `nominalSize` are unchanged.

- [ ] **Step 1: Write the failing tests**

Helpers in the test file:
- `croppedOf(name)`, which opens through `MRXSImageDriver().openFile(path, DriverOptions{{"crop_to_content", "true"}})`;
- `plainOf(name)`, which opens through `MRXSImageDriver().openFile(path, DriverOptions{{"crop_to_content", "false"}})`;
- `read(scene, rect, size)` and `readLevel(scene, level, rect)`, which wrap `readResampledBlockChannels` / `readResampledLevelBlockChannelsEx` with all channels;
- `same(a, b)`, which returns `cv::norm(a, b, cv::NORM_INF) == 0`.

```cpp
TEST(MRXSCrop, theCroppedSceneIsTheBoxStartingAtZero) {
    EXPECT_EQ(cv::Rect(0, 0, 45, 45), croppedOf("offset-content")->getScene(0)->getRect());
}
TEST(MRXSCrop, everyLevelReportsItsBoxSize) {
    const auto plain = plainOf("offset-content")->getScene(0), cropped = croppedOf("offset-content")->getScene(0);
    const auto box = *computeCropBox(MRXSDataset::open(entry("offset-content"))->geometry);
    ASSERT_EQ(plain->getNumZoomLevels(), cropped->getNumZoomLevels());
    for (int l = 0; l < cropped->getNumZoomLevels(); ++l) {
        EXPECT_EQ(box.levels[l].width, cropped->getZoomLevelInfo(l)->getSize().width);
        EXPECT_EQ(box.levels[l].height, cropped->getZoomLevelInfo(l)->getSize().height);
        EXPECT_EQ(plain->getZoomLevelInfo(l)->getScale(), cropped->getZoomLevelInfo(l)->getScale());
    }
}
TEST(MRXSCrop, levelReadsEqualUncroppedReadsShiftedByTheLevelBox) {
    // whole level box, and the sub-rect (1, 2, 5, 4), at every level
    // same(readLevel(cropped, l, r), readLevel(plain, l, r + box.levels[l].tl()))
}
TEST(MRXSCrop, scaledReadsEqualUncroppedReadsShiftedByTheCropOrigin) {
    // (0,0,45,45)->(17,13) and (3,5,30,20)->(11,7): non-integer scales, coarser levels
    // same(read(cropped, r, s), read(plain, r + cv::Point(27, 9), s))
}
TEST(MRXSCrop, aReadOverhangingTheEdgeShowsBackgroundBeyondIt) {             // Review Focus 2
    const cv::Mat img = readLevel(cropped, 0, cv::Rect(40, 40, 10, 10));
    EXPECT_TRUE(same(img(cv::Rect(0, 0, 5, 5)), readLevel(plain, 0, cv::Rect(67, 49, 5, 5))));
    // every pixel with x >= 5 or y >= 5 equals the scene's background (getChannelBackground per channel)
}
TEST(MRXSCrop, rawmapIsClippedToTheCanvas) {
    const auto plainRect = plainOf("rawmap-d2")->getScene(0)->getRect();
    EXPECT_EQ(cv::Rect(0, 0, std::min(90, plainRect.width), std::min(68, plainRect.height)),
              croppedOf("rawmap-d2")->getScene(0)->getRect());
}
TEST(MRXSCrop, metadataCarriesTheCropOnSlideAndScene) {
    const auto slide = croppedOf("offset-content");
    for (const slideio::Metadata* m : {&slide->getMetadata(), &slide->getScene(0)->getMetadata()}) {
        EXPECT_EQ(27, (*m)["mrxs"]["crop"][0].asInt()); EXPECT_EQ(9, (*m)["mrxs"]["crop"][1].asInt());
        EXPECT_EQ(45, (*m)["mrxs"]["crop"][2].asInt()); EXPECT_EQ(45, (*m)["mrxs"]["crop"][3].asInt());
        EXPECT_EQ(27, (*m)["mrxs"]["contentBounds"][0].asInt());   // still nominal
    }
}
TEST(MRXSCrop, noOptionIsTheSameAsTrue) {                                   // Review Focus 5
    MRXSImageDriver driver;
    for (const auto& slide : {driver.openFile(entry("offset-content")),
                              driver.openFile(entry("offset-content"), DriverOptions{})}) {
        EXPECT_EQ(cv::Rect(0, 0, 45, 45), slide->getScene(0)->getRect());
        EXPECT_EQ((DriverOptions{{"mrxs.crop_to_content", "true"}}), slide->getScene(0)->getOpenOptions());
    }
}
TEST(MRXSCrop, falseGivesTheFullCanvas) {                                   // Review Focus 5
    const auto off = plainOf("offset-content");
    EXPECT_EQ(cv::Rect(0, 0, 72, 54), off->getScene(0)->getRect());
    EXPECT_FALSE(off->getMetadata()["mrxs"].hasMember("crop"));   // use the Metadata API's member test
    EXPECT_EQ((DriverOptions{{"mrxs.crop_to_content", "false"}}), off->getScene(0)->getOpenOptions());
}
TEST(MRXSCrop, associatedImagesAreUnchanged) {                              // Review Focus 3
    for (const char* name : {"Label", "Macro", "Thumbnail"})
        EXPECT_EQ(plainOf("offset-content")->getAuxImage(name)->getRect(),
                  croppedOf("offset-content")->getAuxImage(name)->getRect());
}
TEST(MRXSCrop, unknownNamesAndMalformedBooleansAreRejected) {               // Review Focus 1 (driver half)
    // {"Crop_To_Content","true"} -> "has no open option 'mrxs.Crop_To_Content'"
    // {"crop_to_content","yes"}  -> "is not a boolean"
}
TEST(MRXSCrop, openSlideRoutesOptionsAndIgnoresOtherDrivers) {              // Review Focus 1 (prefix half)
    for (const char* driver : {"MRXS", "AUTO"}) {
        auto s = slideio::openSlide(entry("offset-content"), driver,
                                    {{"MRXS.crop_to_content", "true"}, {"svs.anything", "1"}});
        EXPECT_EQ(45, std::get<2>(s->getScene(0)->getRect()));   // Scene::getRect returns a tuple
    }
    EXPECT_EQ(slideio::openSlide(entry("offset-content"))->getScene(0)->getRect(),
              slideio::openSlide(entry("offset-content"), "AUTO", {})->getScene(0)->getRect());
}
TEST(MRXSCrop, openOptionsRoundTripAndTheConverterReopensCropped) {
    const auto cropped = croppedOf("offset-content")->getScene(0);
    EXPECT_EQ((DriverOptions{{"mrxs.crop_to_content", "true"}}), cropped->getOpenOptions());
    converter::OMETIFFJp2KConverterParameters parameters;
    converter::TiffConverter converter;
    converter.createFileLayout(cropped, parameters);
    const auto clone = converter.cloneScene();
    ASSERT_NE(nullptr, clone.second);
    EXPECT_NE(cropped, slideio::SceneInternal::getCVScene(clone.second));   // re-opened, not shared
    EXPECT_EQ(cv::Rect(0, 0, 45, 45), slideio::SceneInternal::getCVScene(clone.second)->getRect());
}
```

Adjust the accessor spellings (`getZoomLevelInfo`, `Metadata` member test, `Scene::getRect` tuple) to the real APIs. The assertions are what must hold.

Add a corpus test to `test_mrxs_corpus_geometry.cpp`. It must keep the `MRXSCorpus` prefix so CI's filter excludes it:

```cpp
TEST(MRXSCorpusCrop, theDefaultSceneIsTheClippedContentBounds) {
    // for each corpus slide the suite already iterates: open with no options;
    // its rect is (0, 0, w, h), where w x h is mrxs.contentBounds of the
    // crop_to_content=false open, intersected with that open's full canvas
}
```

- [ ] **Step 2: Make the existing nominal-geometry tests opt out**

Every existing test that opens through `MRXSImageDriver`, `MRXSSlide::openFile` or `slideio::openSlide` and asserts the nominal canvas (rects, level sizes, renders against `*.expected.json`, the OpenSlide references) now passes `crop_to_content=false`. That means the `slideOf` helper in `test_mrxs_scene.cpp` and the open calls in `test_mrxs_integration.cpp`, `test_mrxs_render.cpp`, `test_mrxs_bundle.cpp`, `test_mrxs_corpus_geometry.cpp` and `test_mrxs_corpus_render.cpp`. Calls that go straight to `MRXSDataset::open` keep its `false` default and need nothing. Tests indifferent to geometry (detection, error paths, aux images) stay as they are. The only change is how they open: no assertion changes.

- [ ] **Step 3: Run the tests and confirm they fail**

Run: `<bin>/slideio_mrxs_tests --gtest_filter="MRXSCrop.*"`
Expected: compile errors, then failures on rect and level sizes.

- [ ] **Step 4: Implement the driver, slide and metadata**

The driver override validates names (any name other than `crop_to_content` throws `has no open option` with `optionKeyOf("MRXS", name)`), parses the value with `parseBoolOption(optionKeyOf(...), value)` (`true` when the key is absent), and calls `MRXSSlide::openFile(path, crop)`. That function passes `crop` to `MRXSDataset::open(path, RuntimeOptions(), crop)`. `describe()` adds `crop`.

- [ ] **Step 5: Implement the scene**

- `getRect()` and the `m_levels` sizes come from `crop->levels[l]` when `crop` is set.
- Extract the current body of `readResampledLevelBlockChannelsEx`, after validation, into a private `readNominalLevel(int level, const cv::Rect& nominalRect, const cv::Rect& nominalBounds, const cv::Size& blockSize, const std::vector<int>& channelIndices, cv::OutputArray output)`. Its `readClampedBlock` uses `nominalBounds` where it now uses the full level rect.
- Level read: without a crop, call `readNominalLevel(level, levelRect, fullLevel)`. With a crop, call `readNominalLevel(level, levelRect + box.tl(), box)`.
- Scaled read: without a crop, leave the code unchanged. With a crop, call `readClampedBlock(Rect(0, 0, C.w, C.h), blockRect, blockSize, channelIndices, reader)`. The reader selects the level for `(validRect, partSize)` exactly as the uncropped path does, scales `validRect + C.tl()`, and calls `readNominalLevel(level, scaled, fullLevel, partSize, ...)`. Clamping to the *full* level here, not the box, is what makes the result bit-identical to the shifted uncropped read. The level-0 clamp to C has already cut away everything outside the crop.
- `getOpenOptions()` as specified in Interfaces.

- [ ] **Step 6: Run the MRXS suite and confirm it passes**

Run: `<bin>/slideio_mrxs_tests --gtest_filter=-MRXSCorpus*`
Expected: all PASS, including every pre-existing test.

- [ ] **Step 7: Run the corpus suites if the corpus is present**

Run: `SLIDEIO_SKIP_MISSING_IMAGES=1 <bin>/slideio_mrxs_tests --gtest_filter=MRXSCorpus*`
Expected: PASS or SKIP. The OpenSlide comparisons still hold with `false`, and `MRXSCorpusCrop` passes.

- [ ] **Step 8: Run the main suite's MRXS detection entries**

Run: `SLIDEIO_SKIP_MISSING_IMAGES=1 <bin>/slideio_tests --gtest_filter="*ImageDriverManager*"`
Expected: PASS or SKIP. Detection is unaffected by the default.

- [ ] **Step 9: Commit**

```bash
git commit -am "feat(mrxs): mrxs.crop_to_content open option"
```

---

### Task 7: Python bindings (`slideio-python` repository)

**Files (in `D:\Projects\slideio\slideio-python`):**
- Modify: `src/pyglobals.hpp`, `src/pyglobals.cpp`, `src/pybind.cpp`, `slideio/wrappers/py_slideio.py`
- Test: `tests/test_open_options.py` (new, corpus-free), `tests/test_mrxs.py` (corpus)

**Interfaces:**
- Consumes: Task 2's `slideio::openSlide(path, driver, SlideOpenOptions)`.
- Produces:
  - `pyOpenSlide(const std::string& path, const std::string& driver, const std::map<std::string, std::string>& options)`. The pybind `open_slide` gains `py::arg("options") = std::map<std::string, std::string>{}`, with `pybind11/stl.h` included.
  - `def _option_strings(options) -> dict` in `py_slideio.py`. `None` gives `{}`. `bool` gives `"true"`/`"false"` (checked before `int`, because `bool` is an `int`). `int` and `float` give `str(v)`, `str` passes through, and anything else raises `TypeError` naming the key.
  - `open_slide(path: str, driver: str = 'AUTO', options: dict = None)` and `Slide.__init__(self, path: str, driver: str, options: dict = None)`.

- [ ] **Step 1: Write the failing tests**

```python
class TestOpenOptions(unittest.TestCase):
    def test_values_become_strings(self):
        from slideio.wrappers.py_slideio import _option_strings
        self.assertEqual({"mrxs.crop_to_content": "true", "x.n": "3", "x.s": "s", "x.f": "false"},
                         _option_strings({"mrxs.crop_to_content": True, "x.n": 3, "x.s": "s", "x.f": False}))
        self.assertEqual({}, _option_strings(None))
    def test_unsupported_value_type_raises_type_error(self):
        from slideio.wrappers.py_slideio import _option_strings
        with self.assertRaises(TypeError):
            _option_strings({"a.b": [1]})
    def test_unknown_driver_prefix_is_reported_before_the_file(self):
        with self.assertRaises(Exception) as ctx:
            slideio.open_slide("no/such/file.mrxs", "AUTO", {"nosuch.x": True})
        self.assertIn("unknown driver 'nosuch'", str(ctx.exception))
```

In `test_mrxs.py`:
- `test_cmu_slide` and any other test asserting the full canvas (check `test_cmu_scaled_read`) open with `options={"mrxs.crop_to_content": False}`. Their expected values stay as they are: they are OpenSlide's.
- Add `test_cmu_slide_is_cropped_by_default`. With no options, `scene.rect` starts at (0, 0), lies within `(7436, 15494)`, is strictly smaller on at least one axis, and equals the rect from `{"mrxs.crop_to_content": True}`. Every zoom level is no larger than the uncropped one.

- [ ] **Step 2: Run the tests and confirm they fail**

Build the bindings against the locally built slideio from Tasks 1–6 the usual way. Watch the MSVC runtime on `PATH`, because a stale one makes every slideio DLL fail `DllMain`.
Run: `py -m pytest tests/test_open_options.py -v`
Expected: FAIL (`_option_strings` does not exist).

- [ ] **Step 3: Implement**

Implement as specified in Interfaces, and add docstrings that name `mrxs.crop_to_content`.

- [ ] **Step 4: Run the tests and confirm they pass**

Run: `py -m pytest tests/test_open_options.py tests/test_mrxs.py tests/test_public_api.py -v`
Expected: PASS; with the corpus missing, the corpus tests SKIP or are excluded as they are today.

- [ ] **Step 5: Commit (in slideio-python)**

```bash
git -C D:/Projects/slideio/slideio-python commit -am "feat: options argument for open_slide and Slide"
```

---

### Task 8: Documentation

**Files:**
- Modify: `CHANGELOG.md`, `CLAUDE.md`, `software-docs/specs/2026-11-15-open-slide-options-design.md` (status line)
- External: `Booritas/slideio-doc` (the `open_slide` reference and the MRXS page). **Check in with the user before pushing to that repository.**

- [ ] **Step 1: Update CHANGELOG.md**

Edit the existing `## [2.11.0]` section; do not add a new one:
- Extend the **MRXS** line under *Four new drivers*: the scene is cropped to the scanned area by default, as QuPath shows it. `mrxs.crop_to_content=false` gives the full canvas OpenSlide reports, and `mrxs.crop` metadata gives the box's offset.
- Add an `### Added` bullet: **Open options**, meaning `openSlide(path, driver, options)` / `open_slide(..., options=...)`, with driver-namespaced keys validated at open.

No **Breaking** entry, because MRXS has never been released. If the release date in the heading changes when the tag moves (Task 9), update it then.

- [ ] **Step 2: Update CLAUDE.md**

Add a *Key Design Patterns* bullet. It says that open options are driver-namespaced and validated in `ImageDriverManager`. It also says that a driver honouring options overrides both `ImageDriver::openFile(path, options)` and `CVScene::getOpenOptions()`: without the second, the converter's re-open silently falls back to one shared scene.

- [ ] **Step 3: Update the spec's status line**

Set it to "implemented on feature/open-slide-options (date); manual review of AI-written code pending before merge".

- [ ] **Step 4: Draft the slideio-doc changes**

Draft them in that repository's working copy and show the user the diff. Do not push.

- [ ] **Step 5: Commit**

```bash
git commit -am "docs: open options and mrxs.crop_to_content"
```

---

### Task 9: Put the change into the 2.11.0 release

`v2.11.0` is already pushed to `origin` (at `73ea70ab`), with a **draft** GitHub Release built from it (`isDraft: true`, not published). Shipping this work as 2.11.0 means re-pointing that tag. That rewrites a pushed ref, and anyone who has fetched the tag keeps the old one. **Nothing in this task runs without the user's explicit go-ahead naming the `v2.11.0` tag**, and the user decides the order of the merge and the re-tag. Merging into `master` is a protected-branch change and also waits for the user.

- [ ] **Step 1: Confirm the release is still a draft**

Run: `gh release view v2.11.0 --json isDraft,publishedAt`
Expected: `{"isDraft":true,"publishedAt":null}`. If it has been published, stop: the default can no longer change silently, and §8 of the spec no longer holds.

- [ ] **Step 2: Present the re-tag commands, and wait for the user**

Once the branch is merged, the commands are as follows. They are listed for the user, not run:

```bash
git tag -f v2.11.0 <merge-commit>
git push --force origin refs/tags/v2.11.0
```

The tag push triggers `release.yml`, which rebuilds the artifacts. Decide with the user whether to delete the stale draft release first, so the workflow attaches to a fresh one.

- [ ] **Step 3: Release slideio-python 2.11.0 with the `options` argument**

Release it together with the C++ library, following that repository's usual process. Check in with the user before tagging there too.
