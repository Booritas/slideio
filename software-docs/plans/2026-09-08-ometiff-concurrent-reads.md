# OME-TIFF Concurrent Reads Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Let two `read_block` calls on one OME-TIFF `Scene` run at the same
time, so `OTScene` reports `supportsConcurrentReads() == true` — taking slideio
from eight concurrent formats to nine.

**Architecture:** `TiffData::m_tiff` — a raw `libtiff::TIFF*` cached at
construction and shared by every `TiffData` naming the same file — is the only
shared mutable state on the read path. It is replaced by a per-thread
`OTReadContext` holding its own `TIFFFiles` collection, handed out one borrower
at a time by a `ContextPool` on `OTScene`. A collection rather than a single
handle because one OME-TIFF tile read can span several `TiffData` that name
different files.

**Tech Stack:** C++17, CMake 3.10+, Conan v2, GoogleTest, OpenCV, libtiff,
tinyxml2. No new dependencies — `ContextPool`, `ReadContext` and `TIFFFiles`
all already exist.

**Spec:** `software-docs/specs/2026-09-08-ometiff-concurrent-reads-design.md`

## Global Constraints

- C++17. Match each file's surrounding style, comment density and naming.
- **`m_contextPool` must be declared LAST in `OTScene`**, after `m_tiffData`.
  `~ContextPool` blocks until every `Borrow` returns, but members are destroyed
  in reverse declaration order — a pool declared first is destroyed last, after
  the `TiffData` objects an in-flight read is dereferencing. Spec §4.4.
- Pool bound is `ContextPool::defaultMax()` — omit the second `ContextPool`
  constructor argument. **Never `kUnbounded`**: a libtiff handle is a scarce
  descriptor, and `kUnbounded` is reserved for contexts holding only scratch.
- **Exactly one borrow per outermost read**, acquired in
  `readResampledBlockChannelsEx` / `readResampledLevelBlockChannelsEx`, carried
  in `BlockInfo`, read by `readTile`, never re-acquired.
- `OTReadContext` is declared in the **scene header** next to the pool, never in
  `otstructs.hpp`. The SCN conversion put its wrapper in the plain-data header
  behind a forward declaration and had to be corrected.
- No new public API beyond the `supportsConcurrentReads()` override.
- `TIFFFiles` itself is **not** modified. It is already the right abstraction.
- Do not touch: `cvscene.*`, `FileReader`, `ContextPool`, `testtools.*`
  (except the one added call site in Task 5), any other driver, `Tiler`,
  `read_batch`, `TiffConverter`.
- Internal docs go in `software-docs/`, never `docs/` (the published site).
- Test output must be pristine.

## Environment — read this before running anything

- Build: `cmake --build build --config Release --target <target> -- -m`
- Run: `./build/bin/Release/<name>.exe --gtest_filter="..."`
- Targets needed here: `slideio_ometiff_tests`, `slideio_tests`.
- **Never run `python install.py` in any form.** The build tree is configured
  and warm; `install.py -a conan` aborts partway in this repo and would break
  it. The plan's commands above are the working ones for this machine.
- **Run every test command in the FOREGROUND.** Never `run_in_background`.
- Windows, Git Bash, Visual Studio 17 2022 multi-config.
- ThreadSanitizer is unavailable here (MSVC has no TSan, no Linux build). Do not
  attempt it. See Task 7 for what that means for the gate.
- Test images resolve through `TestTools::getTestImagePath(subfolder, image)`
  against `SLIDEIO_IMAGES_PATH`. Verified present: `ometiff/Multifile/`,
  `ometiff/Multifile2/`, `ometiff/Subresolutions/`, `ometiff/4D-Series/`.
- Work on a branch off `v2.10.0`, not on `v2.10.0` itself.

## File structure

| File | Responsibility after this plan |
|---|---|
| `src/slideio/drivers/ome-tiff/otscene.hpp` | Declares `OTReadContext` (context + its `TIFFFiles`), `OTScene`'s pool and `acquireContext()`, the `supportsConcurrentReads()` override; loses `m_files` |
| `src/slideio/drivers/ome-tiff/otscene.cpp` | `BlockInfo` gains a context pointer; both `…Ex` entry points acquire one borrow; `readTile` reads the collection from userData; `initialize()` uses a local borrow |
| `src/slideio/drivers/ome-tiff/tiffdata.hpp` | Loses `m_tiff`; `init` takes `TIFFFiles&`; read methods take the collection / handle |
| `src/slideio/drivers/ome-tiff/tiffdata.cpp` | Resolves its handle per read from the passed collection |
| `src/tests/ometiff/test_ometiff_driver.cpp` | Contract assertion inverted; byte-exactness tests added |
| `src/tests/main/test_concurrency_contract.cpp` | OME-TIFF removed from deferred-driver coverage |
| `software-docs/TECH_DEBT.md` | §17 closed; §16 corrected (wrong library); §15 gains the DCMTK flag |
| `software-docs/BREAKING_CHANGES.md` | The contract change and the driver-API changes |
| `CLAUDE.md` | OME-TIFF added to the concurrent-formats list |

---

## Task 1: `OTReadContext` and the pool, with the contract still `false`

Lands the mechanism without changing behaviour, so the existing suite is the
evidence. `supportsConcurrentReads()` stays `false` until Task 4 — this task
must not flip it.

**Files:**
- Modify: `src/slideio/drivers/ome-tiff/otscene.hpp`
- Modify: `src/slideio/drivers/ome-tiff/otscene.cpp`
- Modify: `src/slideio/drivers/ome-tiff/tiffdata.hpp`
- Modify: `src/slideio/drivers/ome-tiff/tiffdata.cpp`

**Interfaces:**
- Consumes: `slideio::ReadContext` (`core/tools/readcontext.hpp`);
  `slideio::ContextPool` with `ContextPool(Factory, int maxContexts = defaultMax())`,
  `Borrow acquire()`, `Borrow::as<T>()`, `static int defaultMax()`,
  `static constexpr int kUnbounded` (`core/tools/contextpool.hpp`);
  `slideio::TIFFFiles` with `libtiff::TIFF* getOrOpen(const std::string&)`
  (`imagetools/tifffiles.hpp`).
- Produces:
  - `class slideio::ometiff::OTReadContext : public slideio::ReadContext` with a public `slideio::TIFFFiles files;`
  - `ContextPool::Borrow OTScene::acquireContext()` (protected)
  - `void TiffData::init(const std::string& filePath, TIFFFiles& files, const std::string& dimOrder, int numChannels, int numZSlices, int numTFrames, tinyxml2::XMLElement* xmlTiffData)`
  - `void TiffData::readTile(const std::vector<int>& channelIndices, int zSlice, int tFrame, int zoomLevel, int tileIndex, TIFFFiles& files, std::vector<cv::Mat>& rasters) const`
  - `void TiffData::readTileChannels(const TiffDirectory& dir, int tileIndex, const std::vector<int>& channelIndices, libtiff::TIFF* tiff, cv::OutputArray raster) const`
  - `BlockInfo` (file-local in `otscene.cpp`) gains `OTReadContext* context = nullptr;`

- [ ] **Step 1: Declare the context and the pool**

In `otscene.hpp`, add the includes and declare the context immediately above
`OTScene`, inside `namespace slideio { namespace ometiff {`:

```cpp
#include "slideio/core/tools/contextpool.hpp"
#include "slideio/core/tools/readcontext.hpp"
```

```cpp
        /// Per-thread libtiff handles for one OME-TIFF read.
        ///
        /// A collection rather than a single handle, because one tile read can
        /// span several TiffData that name different files -- the file comes
        /// from each <TiffData> element's UUID/FileName attribute. TIFFFiles
        /// opens lazily, so a context only ever holds handles to the files the
        /// thread it served actually read.
        ///
        /// Unlike the sibling drivers' contexts, this constructor opens nothing:
        /// TIFFFiles::getOrOpen raises on a failed TIFFOpen rather than
        /// returning null, so no call site needs a null check either way.
        class OTReadContext : public ReadContext
        {
        public:
            TIFFFiles files;
        };
```

In `OTScene`, **delete** `TIFFFiles m_files;` and add — with the pool as the
**last** data member, after `m_driverId`:

```cpp
        protected:
            /// Borrows this scene's per-thread handles for the duration of one
            /// block read. Acquire once per read at the outermost entry point
            /// and pass the context down through BlockInfo; never re-acquire
            /// inside a read.
            ContextPool::Borrow acquireContext() { return m_contextPool.acquire(); }
        private:
            // Declared LAST deliberately. ~ContextPool blocks until every
            // outstanding Borrow is returned, and members are destroyed in
            // reverse declaration order -- so the pool must be declared after
            // m_tiffData, whose TiffData objects an in-flight read is reading.
            // Moving this line up reintroduces a use-after-free on close.
            ContextPool m_contextPool;
```

- [ ] **Step 2: Initialise the pool**

`OTScene`'s constructor (`otscene.cpp`, around line 33) assigns its members in
the body rather than an init list. Add the pool to a member-init list so it is
live before `initialize()` runs:

```cpp
OTScene::OTScene(const ImageData& imageData, int sceneIndex, const std::string& driverId)
    : m_contextPool([]() { return std::make_unique<OTReadContext>(); }) {
    m_imageXml = imageData.imageXml;
    // ... unchanged ...
    initialize();
}
```

The factory captures nothing: `TIFFFiles` needs no path, since it opens per
request. Every member is constructed before the constructor *body* runs, so
`initialize()` can safely call `acquireContext()` — no static factory is needed
here (unlike `SVSTiledScene::create`, which exists for a virtual-dispatch
reason that does not apply to `OTScene`).

- [ ] **Step 3: Give `TiffData` the collection instead of a cached handle**

In `tiffdata.hpp`: **delete** the `libtiff::TIFF* m_tiff;` member, and change
three signatures:

```cpp
            void init(const std::string& filePath, TIFFFiles& files, const std::string& dimOrder,
                      int numChannels, int numZSlices, int numTFrames,
                      tinyxml2::XMLElement* xmlTiffData);

            void readTile(const std::vector<int>& channelIndices, int zSlice, int tFrame,
                          int zoomLevel, int tileIndex, TIFFFiles& files,
                          std::vector<cv::Mat>& rasters) const;

            void readTileChannels(const TiffDirectory& dir, int tileIndex,
                                  const std::vector<int>& channelIndices,
                                  libtiff::TIFF* tiff, cv::OutputArray raster) const;
```

In `tiffdata.cpp`:

- `init` takes `TIFFFiles& files`. Delete the `if (files == nullptr)` guard —
  a reference cannot be null, so the check goes away rather than becoming a
  different check. Replace the two lines that cached the handle:

```cpp
    // Opened here, and dropped again -- the handle is not cached. init() needs
    // one to scan the directories; the read path resolves its own from the
    // borrowed context's collection.
    libtiff::TIFF* tiff = files.getOrOpen(m_filePath);
    m_directories.resize(m_planeCount);
    for (int plane = 0; plane < m_planeCount; ++plane) {
        TiffTools::scanTiffDir(tiff, m_firstIFD + plane, 0, m_directories[plane]);
    }
```

  Delete the `if (!m_tiff) { RAISE_RUNTIME_ERROR ... }` block that followed the
  old assignment: `getOrOpen` already raises on a failed open, so the check is
  now unreachable rather than merely redundant.

- `readTile` resolves the handle once and passes it down:

```cpp
    libtiff::TIFF* tiff = files.getOrOpen(m_filePath);
```

  placed immediately before the `for (int plane = ...)` loop, and the
  `readTileChannels(dir, tileIndex, localChannelIndices, localRaster)` call
  inside the loop becomes
  `readTileChannels(dir, tileIndex, localChannelIndices, tiff, localRaster)`.

- `readTileChannels` uses its `tiff` parameter in place of `m_tiff` at both
  sites: `TiffTools::readTile(tiff, dir, tileIndex, channelIndices, raster)`
  and `TiffTools::readStripedDir(tiff, dir, dirRaster)`.

- [ ] **Step 4: Thread the borrow through `OTScene`**

In `otscene.cpp`, add the field to the file-local `BlockInfo` struct (around
line 24):

```cpp
struct BlockInfo
{
    const LevelInfo* levelInfo = nullptr;
    int zSliceIndex = -1;
    int tFrameIndex = -1;
    std::vector<int> tiffDataIndices;
    OTReadContext* context = nullptr;
};
```

In **both** `readResampledBlockChannelsEx` and
`readResampledLevelBlockChannelsEx`, acquire once and record it — for the level
variant:

```cpp
    auto borrow = acquireContext();
    // ... existing validateLevel / channel completion / levelInfo lookup ...
    BlockInfo blockInfo = {&levelInfo, zSliceIndex, tFrameIndex, {}, &borrow.as<OTReadContext>()};
    collectTiffDataIndices(channelIndices, zSliceIndex, tFrameIndex, blockInfo.tiffDataIndices);
    TileComposer::composeRect(this, channelIndices, levelRect, blockSize, output, (void*)&blockInfo);
```

The borrow must outlive the `composeRect` call — declare it first in the
function, not inside a narrower scope.

In `readTile`, read the collection from the userData and pass it to each
`TiffData`:

```cpp
    TIFFFiles& files = blockInfo->context->files;
    for (int index : blockInfo->tiffDataIndices) {
        m_tiffData[index].readTile(channelIndices, zSlice, tFrame, zoomLevel,
                                   tileIndex, files, channelRasters);
    }
```

`getTileCount` and `getTileRect` need **no** change: both read only
`blockInfo->levelInfo` and never touch a handle or a `TiffData`.

- [ ] **Step 5: Give `initialize()` a local borrow**

`extractTiffData` currently passes `&m_files`. Change it to take the collection
and pass it through. In `otscene.cpp`:

```cpp
void OTScene::extractTiffData(tinyxml2::XMLElement* pixels, TIFFFiles& files) {
    // ... unchanged loop ...
            tiffData.init(m_filePath, files, m_dimensionOrder, m_numChannels,
                          m_numZSlices, m_numTFrames, xmlTiffData);
    // ... unchanged per-element catch ...
}
```

and in `initialize()`, where `extractTiffData(pixels)` is called:

```cpp
    auto borrow = acquireContext();
    extractTiffData(pixels, borrow.as<OTReadContext>().files);
```

Update the declaration in `otscene.hpp` to match. **Leave the per-element
`catch (std::exception&)` in `extractTiffData` exactly as it is** — it logs a
warning and skips that element, which is the pre-existing behaviour for an
unopenable member file, and this plan neither improves nor worsens it
(spec §4.3).

The borrow releases at the end of `initialize()`, returning a context to the
pool with its handles already warm.

- [ ] **Step 6: Build and run both suites**

```bash
cmake --build build --config Release --target slideio_ometiff_tests -- -m
cmake --build build --config Release --target slideio_tests -- -m
./build/bin/Release/slideio_ometiff_tests.exe
./build/bin/Release/slideio_tests.exe --gtest_filter="*ConcurrencyContract*"
```

Expected: `slideio_ometiff_tests` fully green, unchanged from before this task —
behaviour must be identical, since the contract is still `false` and the base
class still serialises. `ConcurrencyContract.*` still green, since OME-TIFF
still reports `false`.

`OTImageDriverTests.concurrentReadsAreStillSerialised` must still pass. If it
fails, you flipped the contract early — Task 4 does that, not this task.

- [ ] **Step 7: Confirm the cached handle is gone**

```bash
grep -rn "m_tiff\b" src/slideio/drivers/ome-tiff/
grep -rn "m_files" src/slideio/drivers/ome-tiff/
```

Expected: no output from either. If `m_files` still appears, it is in
`getNumTiffFiles()` — that accessor is Task 3's job, so leave a compile-fixing
placeholder only if you must, and say so in your report.

- [ ] **Step 8: Commit**

```bash
git add src/slideio/drivers/ome-tiff
git commit -m "give OME-TIFF a per-thread handle collection, still serialised"
```

---

## Task 2: Redefine `getNumTiffFiles()`

`OTScene::getNumTiffFiles()` returns `m_files.getNumberOfOpenFiles()`. With the
map per-context that number becomes per-context and would read 0 before the
first read, breaking a test for a reason unrelated to what the test checks.

**Files:**
- Modify: `src/slideio/drivers/ome-tiff/otscene.hpp`
- Modify: `src/slideio/drivers/ome-tiff/otscene.cpp`
- Test: `src/tests/ometiff/test_ometiff_driver.cpp`

**Interfaces:**
- Consumes: `TiffData::getFilePath()` (already public, returns `const std::string&`).
- Produces: `int OTScene::getNumTiffFiles() const` — now the count of **distinct
  files referenced**, computed from `m_tiffData`, independent of read history.

- [ ] **Step 1: Run the existing test to see it currently pass**

```bash
./build/bin/Release/slideio_ometiff_tests.exe --gtest_filter="*retina*" -v
```

The assertion at `test_ometiff_driver.cpp:299-301` is
`EXPECT_EQ(files, 1)` and `EXPECT_EQ(otScene->getNumTiffDataItems(), 128)` for
the scene `retina_large.ims Resolution Level 1`. Note the shape: 128 `TiffData`
items, **one** distinct file. That is what makes this scene the right test for a
"distinct files" accessor.

Expected after Task 1: this now fails or reads 0, because `m_files` is gone.
Record which.

- [ ] **Step 2: Reimplement the accessor**

In `otscene.hpp`, change the inline definition to a declaration:

```cpp
            /// The number of distinct files this scene's TiffData elements
            /// reference. Deterministic and independent of read history --
            /// deliberately not "handles currently open", which is now
            /// per-context and would read 0 before the first read.
            int getNumTiffFiles() const;
```

In `otscene.cpp`:

```cpp
int OTScene::getNumTiffFiles() const {
    std::set<std::string> paths;
    for (const TiffData& tiffData : m_tiffData) {
        paths.insert(tiffData.getFilePath());
    }
    return static_cast<int>(paths.size());
}
```

Add `#include <set>`.

- [ ] **Step 3: Run the test to verify it passes**

```bash
./build/bin/Release/slideio_ometiff_tests.exe --gtest_filter="*retina*" -v
```

Expected: PASS, with `files == 1` — the same value as before the change, now
for a reason that does not depend on when the read happened.

- [ ] **Step 4: Add a multi-file assertion**

The single-file case cannot distinguish "distinct files" from "1". Add to
`test_ometiff_driver.cpp`:

```cpp
TEST_F(OTImageDriverTests, numTiffFilesCountsDistinctFiles) {
	std::string filePath = TestTools::getTestImagePath("ometiff", "Multifile/multifile-Z1.ome.tiff");
	SLIDEIO_SKIP_IF_IMAGE_MISSING(filePath);
	OTImageDriver driver;
	std::shared_ptr<CVSlide> slide = driver.openFile(filePath);
	ASSERT_TRUE(slide);
	std::shared_ptr<CVScene> scene = slide->getScene(0);
	ASSERT_TRUE(scene);
	std::shared_ptr<OTScene> otScene = std::static_pointer_cast<OTScene>(scene);
	// A multi-file dataset: more than one distinct file, and no more distinct
	// files than TiffData elements.
	EXPECT_GT(otScene->getNumTiffFiles(), 1);
	EXPECT_LE(otScene->getNumTiffFiles(), otScene->getNumTiffDataItems());
}
```

Match the fixture name and the include/namespace style already used in that
file; do not introduce a new style.

- [ ] **Step 5: Run it**

```bash
./build/bin/Release/slideio_ometiff_tests.exe --gtest_filter="*numTiffFilesCountsDistinctFiles*" -v
```

Expected: PASS. If `getNumTiffFiles()` returns 1 here, the `Multifile` dataset's
`TiffData` elements all resolve to the same path — report that, because it would
mean the multi-file byte-exactness test in Task 5 is not exercising what it is
meant to and a different image is needed.

- [ ] **Step 6: Commit**

```bash
git add src/slideio/drivers/ome-tiff src/tests/ometiff/test_ometiff_driver.cpp
git commit -m "OTScene::getNumTiffFiles counts distinct referenced files"
```

---

## Task 3: Flip the contract

**Files:**
- Modify: `src/slideio/drivers/ome-tiff/otscene.hpp`
- Modify: `src/tests/ometiff/test_ometiff_driver.cpp`
- Modify: `src/tests/main/test_concurrency_contract.cpp`

**Interfaces:**
- Consumes: `CVScene::supportsConcurrentReads()` (public virtual, `false` in the base).
- Produces: `OTScene::supportsConcurrentReads() == true`.

- [ ] **Step 1: Replace the serialised assertion with its inverse**

`test_ometiff_driver.cpp:963` currently holds
`OTImageDriverTests.concurrentReadsAreStillSerialised`, asserting
`EXPECT_FALSE(scene->supportsConcurrentReads())` at `:971`. Replace that test
with:

```cpp
TEST_F(OTImageDriverTests, reportsConcurrentReadSupport) {
	std::string filePath = TestTools::getTestImagePath("ometiff", "Subresolutions/Leica-2.ome.tiff");
	SLIDEIO_SKIP_IF_IMAGE_MISSING(filePath);
	OTImageDriver driver;
	std::shared_ptr<CVSlide> slide = driver.openFile(filePath);
	ASSERT_TRUE(slide);
	std::shared_ptr<CVScene> scene = slide->getScene(0);
	ASSERT_TRUE(scene);
	EXPECT_TRUE(scene->supportsConcurrentReads());
}
```

Keep whatever image path the removed test used if it differs from the above —
read it before replacing, and prefer the existing one, since it is known present.

- [ ] **Step 2: Run it to verify it fails**

```bash
cmake --build build --config Release --target slideio_ometiff_tests -- -m
./build/bin/Release/slideio_ometiff_tests.exe --gtest_filter="*reportsConcurrentReadSupport*" -v
```

Expected: FAIL — `Actual: false`, `Expected: true`. The base class still
returns `false`.

- [ ] **Step 3: Add the override**

In `otscene.hpp`, in `OTScene`'s public section near the other overrides:

```cpp
            bool supportsConcurrentReads() const override { return true; }
```

- [ ] **Step 4: Run it to verify it passes**

```bash
cmake --build build --config Release --target slideio_ometiff_tests -- -m
./build/bin/Release/slideio_ometiff_tests.exe --gtest_filter="*reportsConcurrentReadSupport*" -v
```

Expected: PASS.

- [ ] **Step 5: Remove OME-TIFF from the deferred-driver coverage**

`src/tests/main/test_concurrency_contract.cpp` asserts the four deferred drivers
still report `false`. OME-TIFF's assertion lives in the ometiff suite (that
driver is not linked into `slideio_tests`), so check whether that file mentions
OME-TIFF at all:

```bash
grep -n "ometiff\|OTScene\|OME" src/tests/main/test_concurrency_contract.cpp
```

If it does, remove only the OME-TIFF part and leave ZVI, DCM and GDAL asserted
`false`. If it does not, note that in your report and change nothing — the
inversion in Step 1 was the whole job.

- [ ] **Step 6: Run both suites**

```bash
cmake --build build --config Release --target slideio_tests -- -m
./build/bin/Release/slideio_ometiff_tests.exe
./build/bin/Release/slideio_tests.exe --gtest_filter="*ConcurrencyContract*" -v
```

Expected: both green. `ConcurrencyContract.*` must still assert ZVI, DCM and
GDAL are `false`.

- [ ] **Step 7: Commit**

```bash
git add src/slideio/drivers/ome-tiff src/tests
git commit -m "OME-TIFF reports concurrent reads"
```

---

## Task 4: Byte-exactness on a single-file scene

The gate. `TestTools::concurrentReadIdentityTest` computes a single-threaded
baseline per ROI, then has 16 threads read every ROI repeatedly and compares
**every** read byte-for-byte against its baseline. The failure it exists to
catch is wrong pixels, not an exception.

**Files:**
- Test: `src/tests/ometiff/test_ometiff_driver.cpp`

**Interfaces:**
- Consumes: `TestTools::concurrentReadIdentityTestAllPaths(const std::string& filePath, slideio::ImageDriver& driver, int sceneIndex = 0, int numRois = 4, int numThreads = 16, int readsPerThread = 4)` — runs the identity test once per entry shape (`AllChannels`, `SingleChannel`, `ChannelSubset`, `Level`).
- Produces: nothing later tasks consume.

- [ ] **Step 1: Write the failing test**

Add to `test_ometiff_driver.cpp`:

```cpp
TEST_F(OTImageDriverTests, concurrentReadsAreByteIdentical) {
	std::string filePath = TestTools::getTestImagePath("ometiff", "Subresolutions/Leica-2.ome.tiff");
	SLIDEIO_SKIP_IF_IMAGE_MISSING(filePath);
	OTImageDriver driver;
	// AllPaths, not the plain variant: OME-TIFF's per-channel logic walks
	// TiffData coordinate ranges and filters by isInRange, so a channel subset
	// reaches per-read state an all-channels read never touches -- and the
	// level-addressed path is a separate entry point that acquires its own borrow.
	TestTools::concurrentReadIdentityTestAllPaths(filePath, driver);
}
```

- [ ] **Step 2: Run it**

```bash
cmake --build build --config Release --target slideio_ometiff_tests -- -m
./build/bin/Release/slideio_ometiff_tests.exe --gtest_filter="*concurrentReadsAreByteIdentical*" -v
```

Expected: PASS. Report the wall-clock time.

This test is expected to pass immediately — Tasks 1 and 3 did the work, and this
task is the gate proving they were correct. **If it fails, that is the finding
this whole plan exists to surface**: stop, do not adjust the test, and report
the mismatch count with the entry shape that produced it. A mismatch here means
shared mutable state survived Task 1.

- [ ] **Step 3: Prove the gate can fail**

A passing concurrency test is only evidence if it could have failed. Confirm
this one detects the defect it is meant to detect:

Temporarily give every context the *same* `TIFFFiles` — e.g. make
`OTReadContext::files` a reference to one scene-level instance, or have the
factory hand back a shared context — rebuild, and run the test. It should
report mismatches or throw. Then restore, rebuild, and confirm it passes again.

```bash
git diff --stat   # must be empty afterwards
```

Record both outputs in your report. If the sabotaged build still passes, the
test is not exercising concurrency and that is a defect in the test, not good
news.

- [ ] **Step 4: Commit**

```bash
git add src/tests/ometiff/test_ometiff_driver.cpp
git commit -m "byte-exactness gate for concurrent OME-TIFF reads"
```

---

## Task 5: Byte-exactness on a multi-file dataset

**This is the task the whole design is shaped around.** A single-file scene
opens exactly one file per context, which leaves the *collection* — the reason
`OTReadContext` holds a `TIFFFiles` rather than one handle — untested.

**Files:**
- Test: `src/tests/ometiff/test_ometiff_driver.cpp`

**Interfaces:**
- Consumes: `TestTools::concurrentReadIdentityTestAllPaths(...)` as in Task 4;
  `TestTools::concurrentReadIdentityTestAllScenes(const std::string& filePath, slideio::ImageDriver& driver, int numRois = 4, int numThreads = 16, int readsPerThread = 4)`.
- Produces: nothing later tasks consume.

- [ ] **Step 1: Write the multi-file test**

```cpp
TEST_F(OTImageDriverTests, concurrentReadsAreByteIdenticalMultifile) {
	std::string filePath = TestTools::getTestImagePath("ometiff", "Multifile/multifile-Z1.ome.tiff");
	SLIDEIO_SKIP_IF_IMAGE_MISSING(filePath);
	OTImageDriver driver;
	// The case this design exists for: one read can span several TiffData that
	// name different files, so each context must hold a collection of handles
	// rather than one. A single-file scene never exercises that.
	TestTools::concurrentReadIdentityTestAllScenes(filePath, driver);
}
```

`AllScenes` rather than `AllPaths` here: OME-TIFF slides routinely carry several
scenes, and this dataset's scenes are what span the files.

- [ ] **Step 2: Run it**

```bash
cmake --build build --config Release --target slideio_ometiff_tests -- -m
./build/bin/Release/slideio_ometiff_tests.exe --gtest_filter="*Multifile*" -v
```

Expected: PASS. Report the wall-clock time and how many scenes it covered.

- [ ] **Step 3: Verify it really is multi-file**

The test is only meaningful if a read spans more than one file. Confirm it:

```bash
./build/bin/Release/slideio_ometiff_tests.exe --gtest_filter="*numTiffFilesCountsDistinctFiles*" -v
```

Task 2's test asserts `getNumTiffFiles() > 1` on this same image. If that
passes, this dataset genuinely spans files. If it does not, say so — and try
`Multifile2/multifile-Z1.ome.tiff` or `tubhiswt-4D/` instead, reporting which
you used and why.

- [ ] **Step 4: Commit**

```bash
git add src/tests/ometiff/test_ometiff_driver.cpp
git commit -m "byte-exactness gate for concurrent reads across a multi-file OME-TIFF"
```

---

## Task 6: Full suite run and descriptor sanity

**Files:** none — verification only.

**Interfaces:** none.

- [ ] **Step 1: Run every affected suite in the foreground**

```bash
cmake --build build --config Release --target slideio_ometiff_tests -- -m
cmake --build build --config Release --target slideio_tests -- -m
./build/bin/Release/slideio_ometiff_tests.exe
./build/bin/Release/slideio_tests.exe
```

Expected: both green. If anything fails, establish whether it pre-exists by
`git stash`ing and re-running the same filter, and report that comparison. Do
not "fix" unrelated pre-existing failures.

- [ ] **Step 2: Sanity-check the descriptor arithmetic**

Spec §4.5 says the realistic peak is `pool × files-per-read`, not
`pool × files-per-scene`, because `getOrOpen` opens lazily. Check that claim
rather than trusting it:

Add a temporary `SLIDEIO_LOG(INFO)` in `TIFFFiles::getOrOpen` on the branch that
actually opens (not the cache-hit branch), run the multi-file byte-exactness
test, and count the lines. With `defaultMax()` contexts and a handful of files
per read, the total opens should be bounded by roughly
`contexts × distinct files touched` — not by `contexts × reads`.

If opens scale with the *number of reads*, the lazy cache is not working as
assumed and that is a real finding worth reporting. Remove the log and confirm
`git diff --stat` is empty before finishing.

- [ ] **Step 3: Report, do not commit**

Nothing to commit in this task. Report both suites' totals, the open-count
observation, and the two byte-exactness runtimes from Tasks 4 and 5.

---

## Task 7: Documentation

The contract change is invisible at compile time, so these documents are the
only place a caller learns it moved. This task also corrects two `TECH_DEBT.md`
entries, one of which is wrong in a way that misdirects whoever picks it up.

**Files:**
- Modify: `software-docs/TECH_DEBT.md`
- Modify: `software-docs/BREAKING_CHANGES.md`
- Modify: `CLAUDE.md`

**Interfaces:** none — documentation only. There is no RED/GREEN for this task;
say so in your report rather than inventing evidence.

- [ ] **Step 1: Close `TECH_DEBT.md` §17**

§17 claims `TIFFFiles::getOrOpen` races on the read path and calls OME-TIFF "a
different failure class". Both are wrong: `getOrOpen`'s only caller is
`TiffData::init` at construction. Its first suggested fix — "give `TIFFFiles`
its own lock" — would also not have worked, because a lock protects the map
while leaving the handles it hands out shared.

Rewrite the entry as resolved: state that OME-TIFF now reports concurrent reads,
that the real blocker was `TiffData::m_tiff` (a cached handle shared by every
`TiffData` naming one file), and point at
`software-docs/specs/2026-09-08-ometiff-concurrent-reads-design.md`. Keep the
entry rather than deleting it, and mark its status resolved, so the correction
to the record survives. Update the table of contents to match.

- [ ] **Step 2: Correct `TECH_DEBT.md` §16 — the wrong library**

§16 tells whoever picks up the driver named GDAL to "add a `ReadContext`
subclass holding the GDAL dataset. GDAL datasets are not re-entrant."

**That driver contains no GDAL.** `GDALScene::m_imagePage` is a
`SmallImagePage*` (`gdalscene.hpp:42`), obtained from `GDALSlide`'s
`m_image->readPage(...)` (`gdalslide.cpp:22`); the only implementation of
`SmallImagePage` in the tree is `FIWrapper::Page` (`fiwrapper.hpp:25`), and
`fiwrapper.hpp` includes `<FreeImage.h>`. Verify before writing:

```bash
grep -rn "GDALOpen\|gdal_priv\|GDALAllRegister" src/
```

Expected: no output. Then rewrite the entry's mechanism claim: the real question
is FreeImage's thread-safety — its plugin registry and `FreeImage_Initialise`
are process-global — plus whatever state `FIWrapper` and `FIWrapper::Page` hold.
Leave the driver id and the format name alone; only the mechanism claim changes.
Keep the entry open — that driver is not being converted here.

- [ ] **Step 3: Add the DCMTK detail to `TECH_DEBT.md` §15**

§15's claim about DCM is accurate; name the mechanism. `DCMFile::createImage`
constructs `DicomImage(dataset, xfer, CIF_UsePartialAccessToPixelData,
firstFrame, numFrames)`, and `CIF_UsePartialAccessToPixelData` is precisely the
flag that makes DCMTK retain partial pixel-data state inside the shared
`DcmDataset` between `DicomImage` constructions — which is why per-thread
`DCMFile` replicas are the only route there. One or two sentences; do not
restructure the entry.

§14 (ZVI) was verified accurate and needs no change.

- [ ] **Step 4: `BREAKING_CHANGES.md`**

Add an entry under the release this lands in:

- OME-TIFF scenes now report concurrent reads, taking the count from eight
  formats to nine. No public-API signature changes; the break is behavioural —
  code relying on reads of one scene being mutually exclusive to protect **its
  own** state must take its own lock. Point at
  `CVScene::supportsConcurrentReads()` as the query.
- Exported driver-API changes, which `CLAUDE.md` makes unconditional to record:
  `TiffData::init` takes `TIFFFiles&` rather than `TIFFFiles*`;
  `TiffData::readTile` and `readTileChannels` gain parameters;
  `OTScene::getNumTiffFiles()` changes meaning from "handles currently open" to
  "distinct files referenced"; and `OTScene` loses its `TIFFFiles m_files`
  member, a layout change for anything holding an `OTScene` by value.

Group source breaks separately from the layout/ABI change, as the existing
entries do.

- [ ] **Step 5: Record the outstanding ThreadSanitizer requirement**

ThreadSanitizer could not run on this machine (MSVC has no TSan, and there is no
Linux build here), so the byte-exactness gates in Tasks 4 and 5 carried the
weight locally. Spec §5 requires the Linux run be treated as **required before
this ships, not eventual**, because the eight-format claim in
`BREAKING_CHANGES.md` becomes a nine-format claim the moment Step 4 lands — a
public guarantee resting on a gate that is probabilistic rather than a
sanitizer.

The `tsan-linux` CI job added by the parallel-read-block work covers only the
mechanism-level suites (`FileReader.*` and `ContextPool.*`); it does not run
`slideio_ometiff_tests`, and cannot, because that suite needs the image corpus
that CI does not carry.

So add a line to the `BREAKING_CHANGES.md` entry from Step 4, or a short
`TECH_DEBT.md` entry if that reads better in this repo, recording that
`slideio_ometiff_tests` has not been run under ThreadSanitizer and that a Linux
run is outstanding. State it plainly rather than implying uniform coverage —
this is the same platform gap the parallel-read-block spec's §6 records for the
other eight formats, and it is honest to name it in the same terms rather than
leave a reader to assume TSan covered OME-TIFF.

- [ ] **Step 6: `CLAUDE.md`**

The **Key Design Patterns** concurrency-contract bullet ends with a list of
concurrent formats: "Concurrent today: SVS, PHTIFF, AFI, PKE, SCN, NDPI, CZI,
VSI." Add OME-TIFF. Change nothing else in that bullet.

- [ ] **Step 7: Check the counts agree everywhere**

An inconsistent format list is worse than none.

```bash
grep -rn "SVS, PHTIFF, AFI, PKE, SCN, NDPI, CZI, VSI" CLAUDE.md software-docs/
grep -rn "eight format\|nine format" software-docs/
```

Every list must now include OME-TIFF, and every count must say nine.

- [ ] **Step 8: Commit**

```bash
git add software-docs/TECH_DEBT.md software-docs/BREAKING_CHANGES.md CLAUDE.md
git commit -m "document OME-TIFF's concurrent reads; correct the GDAL and OME-TIFF debt entries"
```

---

## Dependencies between tasks

```
1 (context + pool, still false)
      |
      +--> 2 (getNumTiffFiles)  ... 1 removes m_files, so 2 must follow
      |
      +--> 3 (flip the contract)
              |
              +--> 4 (byte-exactness, single file)
              +--> 5 (byte-exactness, multi-file)
                       |
                       +--> 6 (full suites + descriptor check)
                                |
                                +--> 7 (documentation)
```

Task 1 must land first — it removes `m_files`, which Task 2's accessor depends
on being gone, and it is what makes Task 3's flip safe. Tasks 4 and 5 are
independent of each other but both require Task 3. Task 6 requires both gates.
Task 7 requires everything, because it states which formats are concurrent.

Task 2 can be done before or after Task 3; doing it immediately after Task 1
keeps the suite green at every commit, which is why it is numbered here.
