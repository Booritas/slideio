# Explicit Zoom-Level Reading Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Let a caller read a rectangle from a named pyramid level, with the rectangle expressed in that level's own pixel coordinates, from both C++ and Python.

**Architecture:** Every pyramid driver already computes a level index and a level-space rectangle inside `readResampledBlockChannelsEx` and then throws them away. This plan adds one virtual to `CVScene` that accepts those two values directly, gives it a correct base-class default so no driver is obliged to override, and then splits each pyramid driver's existing method at the seam that is already there — the old entry point keeps computing the same values and forwards them. Two buffer-based methods on `slideio::Scene` and one `read_block_from_level` on the Python `Scene` sit on top.

**Tech Stack:** C++17, OpenCV, Google Test, CMake + Conan, pybind11 (separate repo).

**Spec:** `software-docs/specs/2026-08-16-explicit-level-reading-design.md` — read it before starting. This plan implements it and does not repeat its reasoning.

## Global Constraints

- **Branch:** `v2.9.0`. Do not commit to `master`.
- **C++ standard:** C++17. No C++20 constructs.
- **License header:** every new `.hpp`/`.cpp` under `src/slideio/` starts with the three-line header used by every existing file in the tree:
  ```cpp
  // This file is part of slideio project.
  // It is subject to the license terms in the LICENSE file found in the top-level directory
  // of this distribution and at http://slideio.com/license.html.
  ```
  Test files under `src/tests/` do not carry it consistently; follow the neighbouring file.
- **Errors:** use `RAISE_RUNTIME_ERROR << ...` from `slideio/base/exceptions.hpp`. It throws `slideio::RuntimeError`.
- **Naming:** the virtual is `readResampledLevelBlockChannelsEx`, the `CVScene` wrappers are `readResampledLevelBlockChannels` and `readResampledLevel4DBlockChannels`, the `Scene` methods carry the same two names, the Python method is `read_block_from_level`. Do not invent variants.
- **Level coordinates:** `levelRect` is never scaled by the caller and never re-scaled by the receiving driver. This is the entire point of the change.
- **Background fill:** out-of-level regions are filled by `initializeSceneBlock`, which sets `255` for `DataType::DT_Byte` and `0` otherwise. Do not hand-roll a different background.
- **Behaviour preservation:** no existing test may need its expected values edited. If one does, the extraction was wrong — stop and report rather than adjust the test.
- **Build:** `python3 install.py -a install -c release` for a full build; `python3 install.py -a build-only -c release` after the first configure. On Windows the binaries land in `build/`; on Linux/macOS in `build/release/bin/`.
- **Human review:** every task's code must be reviewed by a person before it lands. This plan produces code, not merges.

---

### Task 1: The `CVScene` level contract and its base-class default

**Files:**
- Modify: `src/slideio/core/cvscene.hpp` (declarations, after `getZoomLevelInfo` at line 201; new `protected` helper; new `private` helper)
- Modify: `src/slideio/core/cvscene.cpp` (implementations; refactor of `readResampled4DBlockChannels` at lines 73-160)
- Modify: `src/tests/testlib/testscene.hpp` (level table, request log, optional deterministic raster)
- Create: `src/tests/main/test_levelreading.cpp`
- Modify: `src/tests/main/CMakeLists.txt` (add the new source to `TEST_SOURCES`)

**Interfaces:**
- Consumes: nothing from earlier tasks.
- Produces:
  - `virtual void CVScene::readResampledLevelBlockChannelsEx(int level, const cv::Rect& levelRect, const cv::Size& blockSize, const std::vector<int>& channelIndices, int zSliceIndex, int tFrameIndex, cv::OutputArray output)`
  - `void CVScene::readResampledLevelBlockChannels(int level, const cv::Rect& levelRect, const cv::Size& blockSize, const std::vector<int>& channelIndices, cv::OutputArray output)`
  - `void CVScene::readResampledLevel4DBlockChannels(int level, const cv::Rect& levelRect, const cv::Size& blockSize, const std::vector<int>& channelIndices, const cv::Range& zSliceRange, const cv::Range& timeFrameRange, cv::OutputArray output)`
  - `protected: void CVScene::validateLevel(int level) const`
  - `TestScene::addLevel(const slideio::LevelInfo&)`, `TestScene::clearLevels()`, `TestScene::setRenderCoordinates(bool)`, `TestScene::requests()` returning `const std::vector<TestScene::Request>&` where `struct Request { cv::Rect rect; cv::Size size; int zSlice; int tFrame; };`

- [ ] **Step 1: Give `TestScene` a level table, a request log and an opt-in raster**

`TestScene` today creates an uninitialised output and records nothing, which is enough for the metadata tests that use it but not enough to assert *where* a read went. Add the three capabilities below. The raster is opt-in (`m_render`, default `false`) so that every existing user of `TestScene` sees byte-for-byte the behaviour it sees now.

In `src/tests/testlib/testscene.hpp`, add `#include "slideio/core/levelinfo.hpp"` and `#include <vector>` at the top, then inside the class:

```cpp
public:
    // Records the arguments of every readResampledBlockChannelsEx call, so a test can
    // assert on the request the base-class level path made and not only on its output.
    struct Request
    {
        cv::Rect rect;
        cv::Size size;
        int zSlice;
        int tFrame;
    };

    void addLevel(const slideio::LevelInfo& level) { m_levels.push_back(level); }
    void clearLevels() { m_levels.clear(); }
    const std::vector<Request>& requests() const { return m_requests; }
    void clearRequests() { m_requests.clear(); }
    // Off by default: the tests that predate the level api rely on an untouched output.
    void setRenderCoordinates(bool render) { m_render = render; }
```

Replace the body of `readResampledBlockChannelsEx` with:

```cpp
    void readResampledBlockChannelsEx(const cv::Rect& blockRect, const cv::Size& blockSize,
        const std::vector<int>& componentIndices, int zSliceIndex, int tFrameIndex, cv::OutputArray output) override {
        m_requests.push_back({blockRect, blockSize, zSliceIndex, tFrameIndex});
        if (!output.needed()) return;
        const int channels = componentIndices.empty() ? m_numChannels : (int)componentIndices.size();
        output.create(blockSize, CV_8UC(channels));
        if (!m_render) return;
        // Each pixel encodes the scene coordinate it came from, so a test can tell which
        // part of the scene a block was actually served from.
        cv::Mat raster = output.getMat();
        for (int y = 0; y < blockSize.height; ++y) {
            uint8_t* row = raster.ptr<uint8_t>(y);
            for (int x = 0; x < blockSize.width; ++x) {
                const int sceneX = blockRect.x + (blockRect.width * x) / blockSize.width;
                const int sceneY = blockRect.y + (blockRect.height * y) / blockSize.height;
                for (int c = 0; c < channels; ++c) {
                    row[x * channels + c] = static_cast<uint8_t>((sceneX + sceneY * 7 + c * 31) & 0xFF);
                }
            }
        }
    }
```

and add to the private members:

```cpp
    std::vector<Request> m_requests;
    bool m_render = false;
```

- [ ] **Step 2: Write the failing tests**

Create `src/tests/main/test_levelreading.cpp`:

```cpp
// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#include <gtest/gtest.h>
#include <opencv2/core.hpp>
#include "tests/testlib/testscene.hpp"
#include "slideio/base/exceptions.hpp"
#include "slideio/core/levelinfo.hpp"

using namespace slideio;

namespace
{
    // A 100x100 byte scene with a two level pyramid: level 0 at full size, level 1 at half.
    // Deterministic and file-free, so these tests exercise the contract rather than a driver.
    std::shared_ptr<TestScene> makeTwoLevelScene()
    {
        auto scene = std::make_shared<TestScene>();
        scene->setRect(cv::Rect(0, 0, 100, 100));
        scene->setNumChannels(3);
        scene->setChannelDataType(DataType::DT_Byte);
        scene->setRenderCoordinates(true);
        scene->addLevel(LevelInfo(0, {100, 100}, 1.0, 20., {100, 100}));
        scene->addLevel(LevelInfo(1, {50, 50}, 0.5, 10., {50, 50}));
        return scene;
    }
}

TEST(LevelReadingTests, rejectsAnOutOfRangeLevel)
{
    auto scene = makeTwoLevelScene();
    cv::Mat raster;
    EXPECT_THROW(scene->readResampledLevelBlockChannels(-1, cv::Rect(0, 0, 10, 10), cv::Size(10, 10), {}, raster),
                 RuntimeError);
    EXPECT_THROW(scene->readResampledLevelBlockChannels(2, cv::Rect(0, 0, 10, 10), cv::Size(10, 10), {}, raster),
                 RuntimeError);
}

TEST(LevelReadingTests, rejectsASceneWithoutLevels)
{
    auto scene = makeTwoLevelScene();
    scene->clearLevels();
    cv::Mat raster;
    EXPECT_THROW(scene->readResampledLevelBlockChannels(0, cv::Rect(0, 0, 10, 10), cv::Size(10, 10), {}, raster),
                 RuntimeError);
}

// Level 0 has scale 1, so the default implementation must pass the rectangle through
// untouched. Anything else would mean the identity case is not identity.
TEST(LevelReadingTests, defaultImplementationPassesLevelZeroThrough)
{
    auto scene = makeTwoLevelScene();
    cv::Mat raster;
    scene->readResampledLevelBlockChannels(0, cv::Rect(10, 20, 30, 40), cv::Size(30, 40), {}, raster);
    ASSERT_EQ(1u, scene->requests().size());
    EXPECT_EQ(cv::Rect(10, 20, 30, 40), scene->requests()[0].rect);
    EXPECT_EQ(cv::Size(30, 40), scene->requests()[0].size);
    EXPECT_EQ(cv::Size(30, 40), raster.size());
    EXPECT_EQ(CV_8UC3, raster.type());
}

// Level 1 has scale 0.5, so a level-1 rectangle at (10,10) covers scene pixels from (20,20).
// This is the conversion the caller no longer has to write, done once and in one place.
TEST(LevelReadingTests, defaultImplementationMapsADeeperLevelOntoSceneCoordinates)
{
    auto scene = makeTwoLevelScene();
    cv::Mat raster;
    scene->readResampledLevelBlockChannels(1, cv::Rect(10, 10, 20, 20), cv::Size(20, 20), {}, raster);
    ASSERT_EQ(1u, scene->requests().size());
    EXPECT_EQ(cv::Rect(20, 20, 40, 40), scene->requests()[0].rect);
    EXPECT_EQ(cv::Size(20, 20), scene->requests()[0].size);
    EXPECT_EQ(cv::Size(20, 20), raster.size());
}

// The right and bottom edge tiles of any level overhang it. The caller gets an array of the
// size it asked for, the in-bounds part carries data, and the rest is background -- so every
// tile in a grid has the same shape and a viewer's cache does not need a special case.
TEST(LevelReadingTests, defaultImplementationClampsAndBackgroundFillsAnOverhangingRect)
{
    auto scene = makeTwoLevelScene();
    cv::Mat raster;
    // Level 1 is 50x50. This rect starts at (40,40) and is 20 wide, so half of it is outside.
    scene->readResampledLevelBlockChannels(1, cv::Rect(40, 40, 20, 20), cv::Size(20, 20), {}, raster);
    ASSERT_EQ(cv::Size(20, 20), raster.size());
    ASSERT_EQ(1u, scene->requests().size());
    // Only the valid 10x10 part was read, from scene coordinates (80,80).
    EXPECT_EQ(cv::Rect(80, 80, 20, 20), scene->requests()[0].rect);
    EXPECT_EQ(cv::Size(10, 10), scene->requests()[0].size);
    // DT_Byte background is 255. The overhanging columns and rows must all carry it.
    for (int y = 0; y < 20; ++y) {
        for (int x = 0; x < 20; ++x) {
            if (x < 10 && y < 10) continue;
            const cv::Vec3b pixel = raster.at<cv::Vec3b>(y, x);
            EXPECT_EQ(255, pixel[0]) << "x " << x << " y " << y;
            EXPECT_EQ(255, pixel[1]) << "x " << x << " y " << y;
            EXPECT_EQ(255, pixel[2]) << "x " << x << " y " << y;
        }
    }
}

// A rect entirely outside the level is not an error and must not reach the driver at all.
TEST(LevelReadingTests, defaultImplementationReturnsBackgroundForARectOutsideTheLevel)
{
    auto scene = makeTwoLevelScene();
    cv::Mat raster;
    scene->readResampledLevelBlockChannels(1, cv::Rect(200, 200, 20, 20), cv::Size(20, 20), {}, raster);
    ASSERT_EQ(cv::Size(20, 20), raster.size());
    EXPECT_TRUE(scene->requests().empty());
    cv::Mat background(20, 20, CV_8UC3, cv::Scalar(255, 255, 255));
    EXPECT_EQ(0, cv::countNonZero(cv::abs(raster - background).reshape(1)));
}

// Resampling stays inside the named level: the request the driver receives keeps the
// level-derived scene rect and only the output size shrinks.
TEST(LevelReadingTests, defaultImplementationResamplesWithinTheNamedLevel)
{
    auto scene = makeTwoLevelScene();
    cv::Mat raster;
    scene->readResampledLevelBlockChannels(1, cv::Rect(0, 0, 40, 40), cv::Size(10, 10), {}, raster);
    ASSERT_EQ(1u, scene->requests().size());
    EXPECT_EQ(cv::Rect(0, 0, 80, 80), scene->requests()[0].rect);
    EXPECT_EQ(cv::Size(10, 10), scene->requests()[0].size);
    EXPECT_EQ(cv::Size(10, 10), raster.size());
}

// The 4D entry point assembles one plane per slice, each of them a level read.
TEST(LevelReadingTests, level4DBlockReadsOnePlanePerSlice)
{
    auto scene = makeTwoLevelScene();
    scene->setNumZSlices(3);
    cv::Mat raster;
    scene->readResampledLevel4DBlockChannels(1, cv::Rect(0, 0, 20, 20), cv::Size(20, 20), {},
                                             cv::Range(0, 3), cv::Range(0, 1), raster);
    ASSERT_EQ(3u, scene->requests().size());
    for (int slice = 0; slice < 3; ++slice) {
        EXPECT_EQ(cv::Rect(0, 0, 40, 40), scene->requests()[slice].rect) << "slice " << slice;
        EXPECT_EQ(slice, scene->requests()[slice].zSlice) << "slice " << slice;
    }
    ASSERT_EQ(3, raster.dims);
    EXPECT_EQ(20, raster.size[0]);
    EXPECT_EQ(20, raster.size[1]);
    EXPECT_EQ(3, raster.size[2]);
}
```

Add `test_levelreading.cpp` to `TEST_SOURCES` in `src/tests/main/CMakeLists.txt`, after `test_boundedqueue.cpp`.

- [ ] **Step 3: Run the tests to verify they fail**

Run: `python3 install.py -a build-only -c release` then
`./build/release/bin/slideio_tests --gtest_filter="LevelReadingTests.*"`
(Windows: `./build/slideio_tests.exe --gtest_filter="LevelReadingTests.*"`)

Expected: compilation failure — `readResampledLevelBlockChannels` is not a member of `CVScene`.

- [ ] **Step 4: Declare the new methods on `CVScene`**

In `src/slideio/core/cvscene.hpp`, immediately after `virtual const LevelInfo* getZoomLevelInfo(int level) const;` (line 201):

```cpp
        /**@brief reads a raster rectangle from an explicitly selected zoom level.
         *
         * Unlike the readBlock family, this method does not choose a zoom level: it reads
         * from the level named by @p level and from no other. It exists for callers that
         * already know the level they want -- tiled viewers above all -- and that would
         * otherwise have to convert their rectangle to scene coordinates and let the
         * library convert it back, rounding twice on the way.
         *
         * @param level : zoom level index, in the range [0, getNumZoomLevels()).
         * @param levelRect : rectangle in the pixel coordinate system of @p level. It is
         * not scene coordinates and receives no scaling. The part of it lying outside the
         * level is filled with the background value.
         * @param blockSize : size of the returned block. Resampling is performed from
         * @p level only; a blockSize that would suit a different level does not cause one
         * to be selected. blockSize equal to levelRect.size() means no resampling at all.
         * @param channelIndices : vector of indices of channels to be extracted. Empty
         * means every channel.
         * @param zSliceIndex : index of the z slice to read.
         * @param tFrameIndex : index of the time frame to read.
         * @param output : reference to a cv::OutputArray object.
         *
         * The default implementation clamps @p levelRect to the level, maps it to scene
         * coordinates through LevelInfo::getScale() and delegates to
         * readResampledBlockChannelsEx. For a single level scene that mapping is the
         * identity. Drivers with a real pyramid override it to read the named level
         * directly.
         */
        virtual void readResampledLevelBlockChannelsEx(int level, const cv::Rect& levelRect,
            const cv::Size& blockSize, const std::vector<int>& channelIndices,
            int zSliceIndex, int tFrameIndex, cv::OutputArray output);
        /**@brief reads a plane raster rectangle from an explicitly selected zoom level.
         *
         * Parameters are those of #readResampledLevelBlockChannelsEx, with the z slice and
         * the time frame fixed at 0.
         */
        void readResampledLevelBlockChannels(int level, const cv::Rect& levelRect,
            const cv::Size& blockSize, const std::vector<int>& channelIndices,
            cv::OutputArray output);
        /**@brief reads a multi-dimensional raster block from an explicitly selected zoom level.
         *
         * @param zSliceRange : range of z-slices to be read.
         * @param timeFrameRange : range of time frames to be read.
         * Other parameters are those of #readResampledLevelBlockChannelsEx.
         */
        void readResampledLevel4DBlockChannels(int level, const cv::Rect& levelRect,
            const cv::Size& blockSize, const std::vector<int>& channelIndices,
            const cv::Range& zSliceRange, const cv::Range& timeFrameRange,
            cv::OutputArray output);
```

In the existing `protected:` section, next to `getValidChannelIndices`:

```cpp
        /**@brief throws unless level names a zoom level of this scene. */
        void validateLevel(int level) const;
```

In the `private:` section at the bottom of the class:

```cpp
        /**@brief assembles a 4D block plane by plane.
         *
         * Shared by readResampled4DBlockChannels and readResampledLevel4DBlockChannels,
         * which differ only in how a single plane is read.
         */
        void assemble4DBlock(const cv::Size& blockSize, const std::vector<int>& channelIndicesIn,
            const cv::Range& zSliceRange, const cv::Range& timeFrameRange,
            const std::function<void(int zSliceIndex, int tFrameIndex, cv::OutputArray plane)>& readPlane,
            cv::OutputArray output);
```

Add `#include <functional>` to the include block at the top of the header.

- [ ] **Step 5: Implement `validateLevel` and the default level read**

In `src/slideio/core/cvscene.cpp`, add these includes after the existing ones:

```cpp
#include "slideio/core/tools/tools.hpp"
#include <cmath>
```

Then, after `CVScene::getZoomLevelInfo` (line 178):

```cpp
void CVScene::validateLevel(int level) const
{
    const int numLevels = getNumZoomLevels();
    if (numLevels <= 0) {
        RAISE_RUNTIME_ERROR << "Scene '" << getName() << "' of driver '" << getDriverId()
            << "' does not report any zoom level and cannot be read by level";
    }
    if (level < 0 || level >= numLevels) {
        RAISE_RUNTIME_ERROR << "Invalid zoom level: " << level
            << " Expected range: [0," << numLevels << ")";
    }
}

void CVScene::readResampledLevelBlockChannelsEx(int level, const cv::Rect& levelRect,
    const cv::Size& blockSize, const std::vector<int>& channelIndices,
    int zSliceIndex, int tFrameIndex, cv::OutputArray output)
{
    validateLevel(level);
    const LevelInfo* levelInfo = getZoomLevelInfo(level);
    const cv::Rect levelBounds(0, 0, levelInfo->getSize().width, levelInfo->getSize().height);
    const cv::Rect validRect = levelRect & levelBounds;

    // The whole block is background first: what follows only overwrites the part of it
    // that the level actually covers.
    initializeSceneBlock(blockSize, channelIndices, output);
    if (validRect.empty() || blockSize.width <= 0 || blockSize.height <= 0
        || levelRect.width <= 0 || levelRect.height <= 0) {
        return;
    }

    // Where the surviving part of the rectangle lands in the output. Derived from the
    // offsets rather than from the width so that a rectangle clipped on both sides keeps
    // both of them.
    const double scaleX = static_cast<double>(blockSize.width) / static_cast<double>(levelRect.width);
    const double scaleY = static_cast<double>(blockSize.height) / static_cast<double>(levelRect.height);
    cv::Rect target;
    target.x = static_cast<int>(std::floor((validRect.x - levelRect.x) * scaleX));
    target.y = static_cast<int>(std::floor((validRect.y - levelRect.y) * scaleY));
    target.width = std::min(static_cast<int>(std::ceil(validRect.width * scaleX)),
                            blockSize.width - target.x);
    target.height = std::min(static_cast<int>(std::ceil(validRect.height * scaleY)),
                             blockSize.height - target.y);
    if (target.width <= 0 || target.height <= 0) {
        return;
    }

    // Level coordinates back to scene coordinates. getScale() is the level's width as a
    // fraction of the base level, so dividing by it undoes the conversion the driver's own
    // read path performs in the other direction.
    const double levelScale = levelInfo->getScale();
    if (levelScale <= 0.) {
        RAISE_RUNTIME_ERROR << "Zoom level " << level << " reports a non-positive scale: " << levelScale;
    }
    cv::Rect sceneRect;
    Tools::scaleRect(validRect, 1. / levelScale, 1. / levelScale, sceneRect);

    cv::Mat part;
    readResampledBlockChannelsEx(sceneRect, target.size(), channelIndices, zSliceIndex, tFrameIndex, part);
    cv::Mat block = output.getMat();
    part.copyTo(block(target));
}

void CVScene::readResampledLevelBlockChannels(int level, const cv::Rect& levelRect,
    const cv::Size& blockSize, const std::vector<int>& channelIndices, cv::OutputArray output)
{
    RefCounterGuard guard(this);
    std::lock_guard<std::mutex> lock(m_readBlockMutex);
    readResampledLevelBlockChannelsEx(level, levelRect, blockSize, channelIndices, 0, 0, output);
}
```

- [ ] **Step 6: Extract the 4D plane loop and add the level 4D entry point**

Replace the body of `CVScene::readResampled4DBlockChannels` (currently lines 73-160 of `cvscene.cpp`) with a call to the new helper, and add the helper plus the level variant. The helper is the existing loop moved verbatim — including the fact that the `planeMatrix` branch takes `m_readBlockMutex` and the multi-plane branch does not. That asymmetry is preserved on purpose: this is a behaviour-preserving extraction, and changing the locking here would make it something else. It is recorded as debt in Task 11.

```cpp
void CVScene::assemble4DBlock(const cv::Size& blockSize, const std::vector<int>& channelIndicesIn,
    const cv::Range& zSliceRange, const cv::Range& timeFrameRange,
    const std::function<void(int, int, cv::OutputArray)>& readPlane,
    cv::OutputArray output)
{
    std::vector<int> channelIndices(channelIndicesIn);
    if(channelIndices.empty()) {
        const int sceneNumChannels = getNumChannels();
        channelIndices.resize(sceneNumChannels);
        std::iota(channelIndices.begin(), channelIndices.end(), 0);
    }
    const int sliceCount = zSliceRange.end - zSliceRange.start;
    const int frameCount = timeFrameRange.end - timeFrameRange.start;
    const int channelCount = static_cast<int>(channelIndices.size());
    const int width = blockSize.width;
    const int height = blockSize.height;
    bool planeMatrix = sliceCount == 1 && frameCount == 1;
    int zDimIndex = 2;
    int tDimIndex = 3;
    if (sliceCount == 1) {
        zDimIndex = -1;
        tDimIndex = 2;
    }
    if (frameCount == 1) {
        tDimIndex = -1;
    }
    const int zLocalIndex = zDimIndex - 2;
    const int tLocalIndex = tDimIndex - 2;

    std::vector<int> dims = { height, width };
    if (zDimIndex > 0)
        dims.push_back(sliceCount);
    if (tDimIndex > 0)
        dims.push_back(frameCount);

    const slideio::DataType dt = getChannelDataType(0);
    const int cvDt = CVTools::toOpencvType(dt);
    std::vector<int> indices;

    if (planeMatrix) {
        output.create(height, width, CV_MAKE_TYPE(cvDt, channelCount));
    }
    else {
        output.create((int)dims.size(), dims.data(), CV_MAKE_TYPE(cvDt, channelCount));
    }
    cv::Mat& dataRaster = output.getMatRef();
    std::vector<cv::Range> subDims(2);
    subDims[0] = cv::Range(0, height);
    subDims[1] = cv::Range(0, width);

    if (zDimIndex > 0) {
        subDims.emplace_back(0, 0);
        indices.push_back(0);
    }
    if (tDimIndex > 0) {
        subDims.emplace_back(0, 0);
        indices.push_back(0);
    }

    for (int tfIndex = timeFrameRange.start; tfIndex < timeFrameRange.end; ++tfIndex)
    {
        if (tDimIndex > 0) {
            const int frameCounter = tfIndex - timeFrameRange.start;
            subDims[tDimIndex] = cv::Range(frameCounter, frameCounter + 1);
            indices[tLocalIndex] = frameCounter;
        }

        for (int zSlieceIndex = zSliceRange.start; zSlieceIndex < zSliceRange.end; ++zSlieceIndex)
        {
            if (zDimIndex > 0) {
                const int sliceCounter = zSlieceIndex - zSliceRange.start;
                subDims[zDimIndex] = cv::Range(sliceCounter, sliceCounter + 1);
                indices[zLocalIndex] = sliceCounter;
            }
            if (planeMatrix) {
                std::lock_guard<std::mutex> lock(m_readBlockMutex);
                readPlane(zSlieceIndex, tfIndex, dataRaster);
            }
            else {
                cv::Mat sliceRaster;
                readPlane(zSlieceIndex, tfIndex, sliceRaster);
                CVTools::insertSliceInMultidimMatrix(dataRaster, sliceRaster, indices);
            }
        }
    }
}

void CVScene::readResampled4DBlockChannels(const cv::Rect& blockRect, const cv::Size& blockSize,
    const std::vector<int>& channelIndicesIn, const cv::Range& zSliceRange,
    const cv::Range& timeFrameRange,
    cv::OutputArray output)
{
    RefCounterGuard guard(this);
    std::vector<int> channelIndices(channelIndicesIn);
    if (channelIndices.empty()) {
        const int sceneNumChannels = getNumChannels();
        channelIndices.resize(sceneNumChannels);
        std::iota(channelIndices.begin(), channelIndices.end(), 0);
    }
    assemble4DBlock(blockSize, channelIndices, zSliceRange, timeFrameRange,
        [this, &blockRect, &blockSize, &channelIndices](int zSliceIndex, int tFrameIndex, cv::OutputArray plane) {
            readResampledBlockChannelsEx(blockRect, blockSize, channelIndices, zSliceIndex, tFrameIndex, plane);
        },
        output);
}

void CVScene::readResampledLevel4DBlockChannels(int level, const cv::Rect& levelRect,
    const cv::Size& blockSize, const std::vector<int>& channelIndicesIn,
    const cv::Range& zSliceRange, const cv::Range& timeFrameRange, cv::OutputArray output)
{
    RefCounterGuard guard(this);
    validateLevel(level);
    std::vector<int> channelIndices(channelIndicesIn);
    if (channelIndices.empty()) {
        const int sceneNumChannels = getNumChannels();
        channelIndices.resize(sceneNumChannels);
        std::iota(channelIndices.begin(), channelIndices.end(), 0);
    }
    assemble4DBlock(blockSize, channelIndices, zSliceRange, timeFrameRange,
        [this, level, &levelRect, &blockSize, &channelIndices](int zSliceIndex, int tFrameIndex, cv::OutputArray plane) {
            readResampledLevelBlockChannelsEx(level, levelRect, blockSize, channelIndices,
                                              zSliceIndex, tFrameIndex, plane);
        },
        output);
}
```

Note that the channel list is completed *before* `assemble4DBlock` in both callers and passed into the lambda, so the callback receives the same completed list the assembler uses. This matters: the old code completed it once and used the completed list for both the matrix type and the driver call.

- [ ] **Step 7: Run the new tests**

Run: `python3 install.py -a build-only -c release` then
`./build/release/bin/slideio_tests --gtest_filter="LevelReadingTests.*"`
Expected: PASS, 8 tests.

- [ ] **Step 8: Run the full main suite to prove the 4D refactor changed nothing**

Run: `./build/release/bin/slideio_tests`
Expected: PASS, with the same pass/skip counts as before the change. Any test that now needs different expected values means the extraction was not faithful — stop and report.

- [ ] **Step 9: Commit**

```bash
git add src/slideio/core/cvscene.hpp src/slideio/core/cvscene.cpp \
        src/tests/testlib/testscene.hpp src/tests/main/test_levelreading.cpp \
        src/tests/main/CMakeLists.txt
git commit -m "add a level-addressed read to CVScene with a working default"
```

---

### Task 2: Level tables for `GDALScene` and `PKESmallScene`

**Files:**
- Modify: `src/slideio/drivers/gdal/gdalscene.cpp:15-21` (constructor)
- Modify: `src/slideio/drivers/pke/pkesmallscene.cpp:14-63` (constructor)
- Modify: `src/tests/main/test_gdal_driver.cpp` (new test)
- Modify: `src/tests/pke/test_pke_driver.cpp` (new test)

**Interfaces:**
- Consumes: `CVScene::readResampledLevelBlockChannels` from Task 1.
- Produces: `getNumZoomLevels() == 1` for both scene classes, so no in-tree scene reports zero levels.

Every other scene class registers at least one level; these two are the exceptions, which would make the level API unusable on formats that are otherwise perfectly readable. `SVSSmallScene` (`svssmallscene.cpp:46-52`) is the model to copy.

- [ ] **Step 1: Write the failing tests**

In `src/tests/main/test_gdal_driver.cpp`, append:

```cpp
// GDAL images have no pyramid, but a scene with no level at all cannot be addressed by
// level, so it reports the single level it is. Reading that level is the same read as
// reading the scene.
TEST(GDALImageDriverTests, singleZoomLevelAndLevelRead)
{
    std::string path = TestTools::getTestImagePath("gdal", "img_2448x2448_3x8bit_SRC_RGB_ducks.png");
    slideio::GDALImageDriver driver;
    std::shared_ptr<slideio::CVSlide> slide = driver.openFile(path);
    ASSERT_TRUE(slide != nullptr);
    std::shared_ptr<slideio::CVScene> scene = slide->getScene(0);
    ASSERT_TRUE(scene != nullptr);

    ASSERT_EQ(1, scene->getNumZoomLevels());
    const slideio::LevelInfo* level = scene->getZoomLevelInfo(0);
    ASSERT_TRUE(level != nullptr);
    EXPECT_EQ(0, level->getLevel());
    EXPECT_DOUBLE_EQ(1.0, level->getScale());
    EXPECT_EQ(2448, level->getSize().width);
    EXPECT_EQ(2448, level->getSize().height);

    const cv::Rect roi(100, 200, 300, 400);
    cv::Mat viaBlock, viaLevel;
    scene->readBlock(roi, viaBlock);
    scene->readResampledLevelBlockChannels(0, roi, roi.size(), {}, viaLevel);
    ASSERT_EQ(viaBlock.size(), viaLevel.size());
    ASSERT_EQ(viaBlock.type(), viaLevel.type());
    EXPECT_EQ(0, cv::countNonZero(cv::abs(viaBlock - viaLevel).reshape(1)));
}

// A rect running off the right and bottom edges is the ordinary edge-tile case. GDAL crops
// with cv::Mat(raster, rect), which throws out of range, so the clamp in the base class is
// what keeps this from being an exception.
TEST(GDALImageDriverTests, levelReadClampsAnOverhangingRect)
{
    std::string path = TestTools::getTestImagePath("gdal", "img_2448x2448_3x8bit_SRC_RGB_ducks.png");
    slideio::GDALImageDriver driver;
    std::shared_ptr<slideio::CVSlide> slide = driver.openFile(path);
    std::shared_ptr<slideio::CVScene> scene = slide->getScene(0);
    ASSERT_TRUE(scene != nullptr);

    cv::Mat raster;
    ASSERT_NO_THROW(scene->readResampledLevelBlockChannels(
        0, cv::Rect(2400, 2400, 256, 256), cv::Size(256, 256), {}, raster));
    ASSERT_EQ(cv::Size(256, 256), raster.size());
    // The overhang is background: 255 for a byte image.
    EXPECT_EQ(cv::Vec3b(255, 255, 255), raster.at<cv::Vec3b>(200, 200));
}
```

In `src/tests/pke/test_pke_driver.cpp`, append an equivalent test against a small (auxiliary) PKE scene, following whichever fixture and image that file already uses for its small scenes, asserting `getNumZoomLevels() == 1` and that a level-0 read equals the corresponding `readBlock`.

- [ ] **Step 2: Run the tests to verify they fail**

Run: `./build/release/bin/slideio_tests --gtest_filter="GDALImageDriverTests.singleZoomLevel*:GDALImageDriverTests.levelReadClamps*"`
Expected: FAIL — `ASSERT_EQ(1, scene->getNumZoomLevels())` reports 0.

- [ ] **Step 3: Populate the GDAL level table**

In `src/slideio/drivers/gdal/gdalscene.cpp`, add `#include "slideio/core/levelinfo.hpp"` and `#include "slideio/imagetools/smallimage.hpp"` if not already present, then replace the constructor body:

```cpp
slideio::GDALScene::GDALScene(SmallImagePage* page, const std::string& path, const std::string& driverId) :
    m_imagePage(page),
    m_filePath(path),
	m_driverId(driverId)
{
    m_filePath = path;
    // A gdal image has no pyramid, but a scene with no level cannot be addressed by level
    // at all, so it registers the single level it is. Same shape as SVSSmallScene.
    if (m_imagePage != nullptr) {
        const cv::Size imageSize = m_imagePage->getSize();
        LevelInfo level;
        level.setLevel(0);
        level.setScale(1.);
        level.setMagnification(getMagnification());
        level.setSize({imageSize.width, imageSize.height});
        level.setTileSize({imageSize.width, imageSize.height});
        m_levels.push_back(level);
    }
}
```

- [ ] **Step 4: Populate the PKESmallScene level table**

In `src/slideio/drivers/pke/pkesmallscene.cpp`, at the end of the constructor body, mirroring `svssmallscene.cpp:46-52`:

```cpp
    // A small scene is a single directory with no pyramid; it registers the one level it is
    // so that it can be addressed by level like every other scene.
    LevelInfo level;
    level.setLevel(0);
    level.setScale(1.);
    level.setMagnification(m_magnification);
    level.setTileSize({ m_directory.tileWidth, m_directory.tileHeight });
    level.setSize({ m_directory.width, m_directory.height });
    m_levels.push_back(level);
```

Add `#include "slideio/core/levelinfo.hpp"` if it is not reachable already.

- [ ] **Step 5: Run the tests**

Run: `./build/release/bin/slideio_tests --gtest_filter="GDALImageDriverTests.*"` and
`./build/release/bin/slideio_pke_tests`
Expected: PASS.

- [ ] **Step 6: Commit**

```bash
git add src/slideio/drivers/gdal/gdalscene.cpp src/slideio/drivers/pke/pkesmallscene.cpp \
        src/tests/main/test_gdal_driver.cpp src/tests/pke/test_pke_driver.cpp
git commit -m "register the single zoom level of gdal and pke small scenes"
```

---

### Task 3: `slideio::Scene` public methods

**Files:**
- Modify: `src/slideio/slideio/scene.hpp` (declarations, after `readResampled4DBlockChannels` at line 260)
- Modify: `src/slideio/slideio/scene.cpp` (implementations, after line 278)
- Modify: `src/tests/main/test_levelreading.cpp` (tests through the public api)

**Interfaces:**
- Consumes: `CVScene::readResampledLevelBlockChannels`, `CVScene::readResampledLevel4DBlockChannels` (Task 1); a scene with at least one level for every driver (Task 2).
- Produces:
  - `void Scene::readResampledLevelBlockChannels(int level, const std::tuple<int,int,int,int>& levelRect, const std::tuple<int,int>& blockSize, const std::vector<int>& channelIndices, void* buffer, size_t bufferSize)`
  - `void Scene::readResampledLevel4DBlockChannels(int level, const std::tuple<int,int,int,int>& levelRect, const std::tuple<int,int>& blockSize, const std::vector<int>& channelIndices, const std::tuple<int,int>& zSliceRange, const std::tuple<int,int>& timeFrameRange, void* buffer, size_t bufferSize)`

- [ ] **Step 1: Write the failing tests**

Append to `src/tests/main/test_levelreading.cpp`, adding `#include "slideio/slideio/slideio.hpp"`, `#include "slideio/slideio/scene.hpp"`, `#include "slideio/slideio/slide.hpp"` and `#include "tests/testlib/testtools.hpp"` to its includes:

```cpp
// The public Scene api reaches the same read as the CVScene one, through a caller supplied
// buffer. The gdal png is a one level scene, so a level 0 read is the whole image.
TEST(LevelReadingTests, publicSceneApiReadsALevelIntoABuffer)
{
    const std::string path = TestTools::getTestImagePath("gdal", "img_2448x2448_3x8bit_SRC_RGB_ducks.png");
    std::shared_ptr<slideio::Slide> slide = slideio::openSlide(path, "GDAL");
    ASSERT_TRUE(slide != nullptr);
    std::shared_ptr<slideio::Scene> scene = slide->getScene(0);
    ASSERT_TRUE(scene != nullptr);
    ASSERT_EQ(1, scene->getNumZoomLevels());

    const std::tuple<int, int, int, int> rect(100, 200, 300, 400);
    const std::tuple<int, int> size(300, 400);
    const int bufferSize = scene->getBlockSize(size, 0, 3, 1, 1);

    std::vector<uint8_t> viaBlock(bufferSize), viaLevel(bufferSize);
    scene->readBlock(rect, viaBlock.data(), viaBlock.size());
    scene->readResampledLevelBlockChannels(0, rect, size, {}, viaLevel.data(), viaLevel.size());
    EXPECT_EQ(viaBlock, viaLevel);
}

TEST(LevelReadingTests, publicSceneApiRejectsATooSmallBuffer)
{
    const std::string path = TestTools::getTestImagePath("gdal", "img_2448x2448_3x8bit_SRC_RGB_ducks.png");
    std::shared_ptr<slideio::Slide> slide = slideio::openSlide(path, "GDAL");
    std::shared_ptr<slideio::Scene> scene = slide->getScene(0);
    ASSERT_TRUE(scene != nullptr);

    std::vector<uint8_t> tooSmall(16);
    EXPECT_THROW(scene->readResampledLevelBlockChannels(
                     0, {0, 0, 300, 400}, {300, 400}, {}, tooSmall.data(), tooSmall.size()),
                 slideio::RuntimeError);
}

TEST(LevelReadingTests, publicSceneApiRejectsAnOutOfRangeLevel)
{
    const std::string path = TestTools::getTestImagePath("gdal", "img_2448x2448_3x8bit_SRC_RGB_ducks.png");
    std::shared_ptr<slideio::Slide> slide = slideio::openSlide(path, "GDAL");
    std::shared_ptr<slideio::Scene> scene = slide->getScene(0);
    ASSERT_TRUE(scene != nullptr);

    const std::tuple<int, int> size(64, 64);
    std::vector<uint8_t> buffer(scene->getBlockSize(size, 0, 3, 1, 1));
    EXPECT_THROW(scene->readResampledLevelBlockChannels(
                     5, {0, 0, 64, 64}, size, {}, buffer.data(), buffer.size()),
                 slideio::RuntimeError);
}
```

- [ ] **Step 2: Run the tests to verify they fail**

Run: `./build/release/bin/slideio_tests --gtest_filter="LevelReadingTests.publicSceneApi*"`
Expected: compilation failure — `readResampledLevelBlockChannels` is not a member of `slideio::Scene`.

- [ ] **Step 3: Declare the methods on `Scene`**

In `src/slideio/slideio/scene.hpp`, after `readResampled4DBlockChannels` (line 260):

```cpp
        /**@brief reads a raster rectangle from an explicitly selected zoom level into a memory buffer.
         *
         * Unlike the readBlock family, this method reads from the level named by @p level
         * and no other, so a caller that already knows which level it wants does not have
         * to express its rectangle in scene coordinates and let the library convert it back.
         *
         * @param level : zoom level index, in the range [0, getNumZoomLevels()).
         * @param levelRect : rectangle in the coordinate system of @p level, as a
         * std::tuple(x,y,width,height). The part of it outside the level is background filled.
         * @param blockSize : size of the block after resizing, as a std::tuple<width,height>.
         * Resizing is performed from @p level only.
         * @param channelIndices : vector of indices of channels to be extracted. Empty means all.
         * @param buffer : pointer to an allocated memory buffer. Its size can be computed
         * with #getBlockSize using @p blockSize.
         * @param bufferSize : size of the memory buffer in bytes.
         *
         * Memory layout of the buffer is described in the #readBlock method.
         */
        void readResampledLevelBlockChannels(int level, const std::tuple<int,int,int,int>& levelRect,
            const std::tuple<int,int>& blockSize, const std::vector<int>& channelIndices,
            void* buffer, size_t bufferSize);
        /**@brief reads a multi-dimensional raster block from an explicitly selected zoom level.
         *
         * @param zSliceRange : range of z-slices to be read, as a
         * std::tuple<indexOfFirstSliceToRead,numberOfSlicesToRead>.
         * @param timeFrameRange : range of time frames to be read.
         * Other parameters are those of #readResampledLevelBlockChannels.
         *
         * Memory layout of the buffer is described in the #read4DBlock method.
         */
        void readResampledLevel4DBlockChannels(int level, const std::tuple<int,int,int,int>& levelRect,
            const std::tuple<int,int>& blockSize, const std::vector<int>& channelIndices,
            const std::tuple<int,int>& zSliceRange, const std::tuple<int,int>& timeFrameRange,
            void* buffer, size_t bufferSize);
```

- [ ] **Step 4: Implement them**

In `src/slideio/slideio/scene.cpp`, after `readResampled4DBlockChannels` (line 278). Both bodies mirror their non-level neighbours exactly, differing only in which `CVScene` method they call:

```cpp
void Scene::readResampledLevelBlockChannels(int level, const std::tuple<int, int, int, int>& levelRect,
    const std::tuple<int, int>& blockSize, const std::vector<int>& channelIndices,
    void* buffer, size_t bufferSize)
{
    SLIDEIO_LOG(INFO) << "Scene::readResampledLevelBlockChannels level " << level;
    cv::Rect rect = tupleToRect(levelRect);
    cv::Size size = tupleToSize(blockSize);
    const int numChannels = (channelIndices.empty()?m_scene->getNumChannels():static_cast<int>(channelIndices.size()));
    const int refChannel = (channelIndices.empty()?0:channelIndices[0]);
    const int blockMemSize = getBlockSize(blockSize, refChannel, numChannels, 1, 1);
    const auto dt = m_scene->getChannelDataType(refChannel);
    const int cvType = CVTools::cvTypeFromDataType(dt);

    if(blockMemSize>bufferSize)
    {
        RAISE_RUNTIME_ERROR << "Supplied memory buffer is too small. Received: " << bufferSize
            << ". Required: " << blockMemSize;
    }
    cv::Mat raster(size.height, size.width, CV_MAKETYPE(cvType, numChannels), buffer);
    raster = cv::Scalar(0);
    m_scene->readResampledLevelBlockChannels(level, rect, size, channelIndices, raster);

    if(buffer!=raster.data)
    {
        RAISE_RUNTIME_ERROR << "Unexpected data reallocation by reading of file " << getFilePath();
    }
}

void Scene::readResampledLevel4DBlockChannels(int level, const std::tuple<int, int, int, int>& levelRect,
    const std::tuple<int, int>& blockSize, const std::vector<int>& channelIndices,
    const std::tuple<int, int>& zSliceRange, const std::tuple<int, int>& timeFrameRange,
    void* buffer, size_t bufferSize)
{
    SLIDEIO_LOG(INFO) << "Scene::readResampledLevel4DBlockChannels level " << level;
    cv::Rect rect = tupleToRect(levelRect);
    cv::Size size = tupleToSize(blockSize);
    cv::Range sliceRange = tupleToRange(zSliceRange);
    cv::Range frameRange = tupleToRange(timeFrameRange);

    const int numChannels = (channelIndices.empty()?m_scene->getNumChannels():static_cast<int>(channelIndices.size()));
    const int numSlices = sliceRange.size();
    const int numFrames = frameRange.size();
    const int refChannel = (channelIndices.empty()?0:channelIndices[0]);
    const int numPlanes = numChannels*numSlices*numFrames;
    const int blockMemSize = getBlockSize(blockSize, refChannel, numChannels, numSlices, numFrames);
    const int planeMemSize = getBlockSize(blockSize, refChannel, numChannels, 1, 1);
    const auto cvType = m_scene->getChannelDataType(refChannel);

    if(blockMemSize>bufferSize) {
        RAISE_RUNTIME_ERROR << "Supplied memory buffer is too small. Received: " << bufferSize
            << ". Required: " << blockMemSize;
    }

    cv::Mat raster(size.height, size.width, CV_MAKETYPE(static_cast<int>(cvType), numPlanes), buffer);
    if (numSlices==1 && numFrames==1) {
        m_scene->readResampledLevel4DBlockChannels(level, rect, size, channelIndices, sliceRange, frameRange, raster);
        if (buffer != raster.data) {
            RAISE_RUNTIME_ERROR << "Unexpected memory reallocation";
        }
    }
    else {
        cv::Mat mdRaster;
        std::vector<int> indices;
        int sliceIndex(-1), frameIndex(-1);

        if(numSlices > 1) {
            sliceIndex = 0;
            indices.push_back(0);
        }
        if( numFrames > 1) {
            frameIndex = sliceIndex + 1;
            indices.push_back(0);
        }
        uint8_t* planeBegin = static_cast<uint8_t*>(buffer);
        m_scene->readResampledLevel4DBlockChannels(level, rect, size, channelIndices, sliceRange, frameRange, mdRaster);
        for (int tfIndex = frameRange.start; tfIndex < frameRange.end; ++tfIndex)
        {
            if(frameIndex>=0) {
                indices[frameIndex] = tfIndex - frameRange.start;
            }
            for (int zSliceIndex = sliceRange.start; zSliceIndex < sliceRange.end; ++zSliceIndex, planeBegin+=planeMemSize)
            {
                if (sliceIndex >= 0) {
                    indices[sliceIndex] = zSliceIndex - sliceRange.start;
                }
                cv::Mat sliceRaster;
                CVTools::extractSliceFromMultidimMatrix(mdRaster, indices, sliceRaster);
                if( !sliceRaster.isContinuous()) {
                    RAISE_RUNTIME_ERROR << "Unexpected non-continuous matrix";
                }
                memcpy(planeBegin, sliceRaster.data, planeMemSize);
            }
        }
    }
}
```

- [ ] **Step 5: Run the tests**

Run: `./build/release/bin/slideio_tests --gtest_filter="LevelReadingTests.*"`
Expected: PASS, 11 tests.

- [ ] **Step 6: Commit**

```bash
git add src/slideio/slideio/scene.hpp src/slideio/slideio/scene.cpp src/tests/main/test_levelreading.cpp
git commit -m "expose level-addressed reading on the public Scene api"
```

---

### Task 4: SVS and PHTIFF driver override

**Files:**
- Modify: `src/slideio/drivers/svs/svstiledscene.hpp` (declare the override; rename `findZoomDirectory`)
- Modify: `src/slideio/drivers/svs/svstiledscene.cpp:119-153`
- Modify: `src/tests/phtiff/test_phtiff_driver.cpp` (level read tests)

**Interfaces:**
- Consumes: `CVScene::readResampledLevelBlockChannelsEx` and `CVScene::validateLevel` (Task 1).
- Produces: `SVSTiledScene::readResampledLevelBlockChannelsEx` override; `int SVSTiledScene::findZoomLevelIndex(double zoom) const` replacing `const TiffDirectory& findZoomDirectory(double zoom) const`.

This is the model every later driver task follows. The old entry point keeps computing the same level and the same level rectangle; the new one holds the body that already exists.

- [ ] **Step 1: Write the failing tests**

Append to `src/tests/phtiff/test_phtiff_driver.cpp`. The existing `zoomLevelsOfPhilips3ExcludeTilePadding` already pins this file at 9 levels, so the multi-level assertions below are grounded:

```cpp
// The point of the level api: a rect given in level coordinates is read from that level
// with no conversion. Reading the whole of a level by level, and reading the whole scene
// resampled to that level's size, have to show the same picture -- the second goes through
// the conversion this test's subject avoids, so agreement means the two entry points
// address the pyramid the same way.
TEST_F(PhTiffImageDriverTests, readLevelMatchesTheResampledSceneRead) {
	if (!TestTools::isFullTestEnabled()) {
		GTEST_SKIP() << "Skip private test because full dataset is not enabled";
	}
	PhSlideAndScene opened = phOpenScene(ph2::FILE_NAME);
	ASSERT_TRUE(opened.scene != nullptr);
	const int numLevels = opened.scene->getNumZoomLevels();
	ASSERT_LE(4, numLevels);

	// Level 0 is too large to read whole; start where the level fits in memory comfortably.
	for (int level = 4; level < numLevels; ++level) {
		const LevelInfo* info = opened.scene->getZoomLevelInfo(level);
		ASSERT_TRUE(info != nullptr) << "level " << level;
		const cv::Size levelSize(info->getSize().width, info->getSize().height);
		cv::Mat viaLevel, viaScene;
		opened.scene->readResampledLevelBlockChannels(level, cv::Rect(cv::Point(0, 0), levelSize),
		                                              levelSize, {}, viaLevel);
		opened.scene->readResampledBlock(opened.scene->getRect(), levelSize, viaScene);
		ASSERT_EQ(levelSize, viaLevel.size()) << "level " << level;
		ASSERT_EQ(levelSize, viaScene.size()) << "level " << level;
		// The two reads differ only by the rounding of the scene-coordinate round trip, so
		// they are near identical rather than identical.
		EXPECT_LE(0.95, ImageTools::computeSimilarity2(viaLevel, viaScene)) << "level " << level;
	}
}

// Reading a level tile by tile and stitching the tiles gives the level. This is the test of
// the coordinate interpretation: an off-by-one in the level rect shows as a seam.
TEST_F(PhTiffImageDriverTests, readLevelTileByTileReconstructsTheLevel) {
	if (!TestTools::isFullTestEnabled()) {
		GTEST_SKIP() << "Skip private test because full dataset is not enabled";
	}
	PhSlideAndScene opened = phOpenScene(ph2::FILE_NAME);
	ASSERT_TRUE(opened.scene != nullptr);
	const int level = opened.scene->getNumZoomLevels() - 1;
	const LevelInfo* info = opened.scene->getZoomLevelInfo(level);
	ASSERT_TRUE(info != nullptr);
	const cv::Size levelSize(info->getSize().width, info->getSize().height);
	const cv::Size tileSize(info->getTileSize().width, info->getTileSize().height);
	ASSERT_LT(0, tileSize.width);
	ASSERT_LT(0, tileSize.height);

	cv::Mat whole;
	opened.scene->readResampledLevelBlockChannels(level, cv::Rect(cv::Point(0, 0), levelSize),
	                                              levelSize, {}, whole);
	cv::Mat stitched(levelSize, whole.type(), cv::Scalar::all(0));
	for (int y = 0; y < levelSize.height; y += tileSize.height) {
		for (int x = 0; x < levelSize.width; x += tileSize.width) {
			const cv::Rect tileRect(x, y, tileSize.width, tileSize.height);
			cv::Mat tile;
			opened.scene->readResampledLevelBlockChannels(level, tileRect, tileSize, {}, tile);
			ASSERT_EQ(tileSize, tile.size()) << "tile " << x << "," << y;
			// Only the part of the tile inside the level belongs in the mosaic; the rest is
			// the background the contract promises for an overhanging tile.
			const cv::Rect valid = tileRect & cv::Rect(cv::Point(0, 0), levelSize);
			tile(cv::Rect(0, 0, valid.width, valid.height)).copyTo(stitched(valid));
		}
	}
	EXPECT_DOUBLE_EQ(0., phMaxAbsDiff(stitched, whole));
}

// The level api never selects a level of its own: asking level N for a half sized block has
// to resample level N rather than fall through to level N+1, which already holds that size.
TEST_F(PhTiffImageDriverTests, readLevelDoesNotEscalateToAFinerLevel) {
	if (!TestTools::isFullTestEnabled()) {
		GTEST_SKIP() << "Skip private test because full dataset is not enabled";
	}
	PhSlideAndScene opened = phOpenScene(ph2::FILE_NAME);
	ASSERT_TRUE(opened.scene != nullptr);
	const int level = opened.scene->getNumZoomLevels() - 2;
	const LevelInfo* coarse = opened.scene->getZoomLevelInfo(level + 1);
	ASSERT_TRUE(coarse != nullptr);
	const cv::Size coarseSize(coarse->getSize().width, coarse->getSize().height);
	const LevelInfo* fine = opened.scene->getZoomLevelInfo(level);
	ASSERT_TRUE(fine != nullptr);
	const cv::Size fineSize(fine->getSize().width, fine->getSize().height);

	// Read the fine level down to the coarse level's size, and read the coarse level whole.
	cv::Mat resampledFine, coarseRead;
	opened.scene->readResampledLevelBlockChannels(level, cv::Rect(cv::Point(0, 0), fineSize),
	                                              coarseSize, {}, resampledFine);
	opened.scene->readResampledLevelBlockChannels(level + 1, cv::Rect(cv::Point(0, 0), coarseSize),
	                                              coarseSize, {}, coarseRead);
	ASSERT_EQ(coarseSize, resampledFine.size());
	ASSERT_EQ(coarseSize, coarseRead.size());
	// They show the same region, so they are similar -- but philips regenerated each level
	// with its own quality 80 jpeg, so a downscale of the finer level is not the coarser
	// level byte for byte. If it were identical, the read had been served from the coarse
	// level and the resampling never happened.
	EXPECT_LE(0.85, ImageTools::computeSimilarity2(resampledFine, coarseRead));
	EXPECT_LT(0., phMaxAbsDiff(resampledFine, coarseRead));
}

TEST_F(PhTiffImageDriverTests, readLevelRejectsAnOutOfRangeLevel) {
	if (!TestTools::isFullTestEnabled()) {
		GTEST_SKIP() << "Skip private test because full dataset is not enabled";
	}
	PhSlideAndScene opened = phOpenScene(ph2::FILE_NAME);
	ASSERT_TRUE(opened.scene != nullptr);
	const int numLevels = opened.scene->getNumZoomLevels();
	cv::Mat raster;
	EXPECT_THROW(opened.scene->readResampledLevelBlockChannels(-1, cv::Rect(0, 0, 64, 64),
	                                                           cv::Size(64, 64), {}, raster),
	             slideio::RuntimeError);
	EXPECT_THROW(opened.scene->readResampledLevelBlockChannels(numLevels, cv::Rect(0, 0, 64, 64),
	                                                           cv::Size(64, 64), {}, raster),
	             slideio::RuntimeError);
}
```

- [ ] **Step 2: Run the tests to verify they fail**

Run: `./build/release/bin/slideio_phtiff_tests --gtest_filter="PhTiffImageDriverTests.readLevel*"`
Expected: with the full dataset enabled, `readLevelDoesNotEscalateToAFinerLevel` and `readLevelTileByTileReconstructsTheLevel` fail — without the override, the base default converts to scene coordinates and lets `findZoomDirectory` pick the level again. Without the full dataset they skip; enable it before starting this task.

- [ ] **Step 3: Declare the override and rename the finder**

In `src/slideio/drivers/svs/svstiledscene.hpp`, add to the public section next to `readResampledBlockChannelsEx`:

```cpp
        void readResampledLevelBlockChannelsEx(int level, const cv::Rect& levelRect,
            const cv::Size& blockSize, const std::vector<int>& channelIndices,
            int zSliceIndex, int tFrameIndex, cv::OutputArray output) override;
```

and change the private declaration `const TiffDirectory& findZoomDirectory(double zoom) const;` to:

```cpp
        // Returns the index of the level serving a zoom, not the directory: the level index
        // is what the level-addressed read path takes.
        int findZoomLevelIndex(double zoom) const;
```

- [ ] **Step 4: Split the read method**

In `src/slideio/drivers/svs/svstiledscene.cpp`, replace lines 119-153 with:

```cpp
void SVSTiledScene::readResampledBlockChannelsEx(const cv::Rect& blockRect, const cv::Size& blockSize,
                                                 const std::vector<int>& channelIndices, int zSliceIndex,
                                                 int tFrameIndex, cv::OutputArray output) {
    const double zoomX = static_cast<double>(blockSize.width) / static_cast<double>(blockRect.width);
    const double zoomY = static_cast<double>(blockSize.height) / static_cast<double>(blockRect.height);
    const int level = findZoomLevelIndex(std::max(zoomX, zoomY));
    const slideio::TiffDirectory& dir = m_directories[level];
    const double zoomDirX = static_cast<double>(dir.width) / static_cast<double>(m_directories[0].width);
    const double zoomDirY = static_cast<double>(dir.height) / static_cast<double>(m_directories[0].height);
    cv::Rect levelRect;
    Tools::scaleRect(blockRect, zoomDirX, zoomDirY, levelRect);
    readResampledLevelBlockChannelsEx(level, levelRect, blockSize, channelIndices,
                                      zSliceIndex, tFrameIndex, output);
}

void SVSTiledScene::readResampledLevelBlockChannelsEx(int level, const cv::Rect& levelRect,
                                                      const cv::Size& blockSize,
                                                      const std::vector<int>& channelIndices,
                                                      int zSliceIndex, int tFrameIndex,
                                                      cv::OutputArray output) {
    if (zSliceIndex != 0 || tFrameIndex != 0) {
        RAISE_RUNTIME_ERROR << "SVSDriver: 3D and 4D images are not supported";
    }
    validateLevel(level);
    auto hFile = getFileHandle();
    if (hFile == nullptr) {
        RAISE_RUNTIME_ERROR << "SVSDriver: Invalid file header by raster reading operation";
    }
    const slideio::TiffDirectory& dir = m_directories[level];
    std::vector<int> channels(channelIndices);
    if (channels.empty()) {
        channels.resize(dir.channels);
        std::iota(channels.begin(), channels.end(), 0);
    }
    TileComposer::composeRect(this, channels, levelRect, blockSize, output, (void*)&dir);
}

int SVSTiledScene::findZoomLevelIndex(double zoom) const {
    const cv::Rect sceneRect = getRect();
    const double sceneWidth = static_cast<double>(sceneRect.width);
    const auto& directories = m_directories;
    return Tools::findZoomLevel(zoom, (int)m_directories.size(), [&directories, sceneWidth](int index) {
        return directories[index].width / sceneWidth;
    });
}
```

Note the two deliberate carry-overs from the old code: the file-handle check and the channel completion move into the level method, because both are needed by any caller of it; and `m_directories` is indexed by the level index directly, which is valid because `initialize()` builds `m_levels` one-for-one from `m_directories` (`svstiledscene.cpp:91-103`).

- [ ] **Step 5: Run the tests**

Run: `./build/release/bin/slideio_phtiff_tests` and `./build/release/bin/slideio_tests --gtest_filter="*SVS*:*AFI*"`
Expected: PASS, including every pre-existing test with unchanged expectations.

- [ ] **Step 6: Commit**

```bash
git add src/slideio/drivers/svs/svstiledscene.hpp src/slideio/drivers/svs/svstiledscene.cpp \
        src/tests/phtiff/test_phtiff_driver.cpp
git commit -m "read a named zoom level directly in the svs tiled scene"
```

---

### Task 5: NDPI driver override

**Files:**
- Modify: `src/slideio/drivers/ndpi/ndpiscene.hpp`
- Modify: `src/slideio/drivers/ndpi/ndpiscene.cpp:239-289`
- Modify: `src/tests/ndpi/test_ndpi_driver.cpp`

**Interfaces:**
- Consumes: `CVScene::readResampledLevelBlockChannelsEx`, `CVScene::validateLevel` (Task 1).
- Produces: `NDPIScene::readResampledLevelBlockChannelsEx` override; `int NDPIScene::findZoomLevelIndex(double zoom) const`.

NDPI differs from SVS in three ways. `m_levels[lv]` corresponds to `directories[m_startDir + lv]`, not to `directories[lv]` (`ndpiscene.cpp:155-166`). The level search lives on `NDPIFile`, not on the scene, and returns a directory rather than an index (`ndpifile.cpp:55-63`) — the new `findZoomLevelIndex` is that function's body with the final index-to-reference step removed, so it selects provably the same level. And the `SingleStripe` branch crops with `cv::Mat(raster, dirBlockRect)`, which throws when the rectangle overhangs; the clamp below is what makes the background-fill contract hold for that branch.

- [ ] **Step 1: Write the failing tests**

Append to `src/tests/ndpi/test_ndpi_driver.cpp`. The existing `zoomLevels` test pins the hamamatsu file at 3 levels:

```cpp
// Reading a level whole, by level, has to show the same picture as reading the whole scene
// resampled to that level's size. The second goes through the scene-coordinate round trip
// the level api avoids, so agreement means both address the pyramid the same way.
TEST_F(NDPIImageDriverTests, readLevelMatchesTheResampledSceneRead)
{
    if (!TestTools::isFullTestEnabled())
    {
        GTEST_SKIP() << "Skip private test because full dataset is not enabled";
    }
    slideio::NDPIImageDriver driver;
    const std::string filePath = TestTools::getFullTestImagePath("hamamatsu", "2017-02-27 15.29.08.ndpi");
    auto slide = driver.openFile(filePath);
    auto scene = slide->getScene(0);
    ASSERT_TRUE(scene != nullptr);
    const int numLevels = scene->getNumZoomLevels();
    ASSERT_EQ(3, numLevels);

    for (int level = 1; level < numLevels; ++level)
    {
        const slideio::LevelInfo* info = scene->getZoomLevelInfo(level);
        ASSERT_TRUE(info != nullptr) << "level " << level;
        const cv::Size levelSize(info->getSize().width, info->getSize().height);
        cv::Mat viaLevel, viaScene;
        scene->readResampledLevelBlockChannels(level, cv::Rect(cv::Point(0, 0), levelSize), levelSize, {}, viaLevel);
        scene->readResampledBlock(scene->getRect(), levelSize, viaScene);
        ASSERT_EQ(levelSize, viaLevel.size()) << "level " << level;
        EXPECT_LE(0.95, slideio::ImageTools::computeSimilarity2(viaLevel, viaScene)) << "level " << level;
    }
}

// The right and bottom edge of a level: the rect overhangs, the read must not throw, and the
// overhang comes back as background. The ndpi SingleStripe branch crops with cv::Mat(m,rect),
// which is exactly the operation that throws when the rect is not contained.
TEST_F(NDPIImageDriverTests, readLevelClampsAnOverhangingRect)
{
    if (!TestTools::isFullTestEnabled())
    {
        GTEST_SKIP() << "Skip private test because full dataset is not enabled";
    }
    slideio::NDPIImageDriver driver;
    const std::string filePath = TestTools::getFullTestImagePath("hamamatsu", "2017-02-27 15.29.08.ndpi");
    auto slide = driver.openFile(filePath);
    auto scene = slide->getScene(0);
    ASSERT_TRUE(scene != nullptr);
    const int level = scene->getNumZoomLevels() - 1;
    const slideio::LevelInfo* info = scene->getZoomLevelInfo(level);
    ASSERT_TRUE(info != nullptr);
    const cv::Size levelSize(info->getSize().width, info->getSize().height);

    const cv::Rect overhanging(levelSize.width - 64, levelSize.height - 64, 256, 256);
    cv::Mat raster;
    ASSERT_NO_THROW(scene->readResampledLevelBlockChannels(level, overhanging, cv::Size(256, 256), {}, raster));
    ASSERT_EQ(cv::Size(256, 256), raster.size());
    // Anything past the level's own 64x64 corner is background: 255 for a byte image.
    const cv::Vec3b outside = raster.at<cv::Vec3b>(200, 200);
    EXPECT_EQ(255, outside[0]);
    EXPECT_EQ(255, outside[1]);
    EXPECT_EQ(255, outside[2]);
}
```

Add `#include "slideio/imagetools/imagetools.hpp"` to the file's includes if it is not already there.

- [ ] **Step 2: Run the tests to verify they fail**

Run: `./build/release/bin/slideio_ndpi_tests --gtest_filter="NDPIImageDriverTests.readLevel*"`
Expected: FAIL — without the override, the base default's conversion lets `findZoomDirectory` reselect.

- [ ] **Step 3: Declare the override**

In `src/slideio/drivers/ndpi/ndpiscene.hpp`, next to `readResampledBlockChannelsEx`:

```cpp
    void readResampledLevelBlockChannelsEx(int level, const cv::Rect& levelRect,
        const cv::Size& blockSize, const std::vector<int>& channelIndices,
        int zSliceIndex, int tFrameIndex, cv::OutputArray output) override;
    // The index of the level serving a zoom. NDPIFile::findZoomDirectory performs this
    // same search and then returns the directory; the level path needs the index itself.
    int findZoomLevelIndex(double zoom) const;
```

`findZoomDirectory(const cv::Rect&, const cv::Size&)` stays where it is. It has no caller left after this task, but it is public and referenced by `src/slideio/drivers/ndpi/test_ndpiscene.cpp`; removing it is unrelated cleanup.

- [ ] **Step 4: Split the read method**

In `src/slideio/drivers/ndpi/ndpiscene.cpp`, replace `readResampledBlockChannelsEx` (lines 261-289) with:

```cpp
void NDPIScene::readResampledBlockChannelsEx(const cv::Rect& imageBlockRect, const cv::Size& requiredBlockSize,
        const std::vector<int>& channelIndices, int zSliceIndex, int tFrameIndex, cv::OutputArray output)
{
    const double zoomX = static_cast<double>(requiredBlockSize.width) / static_cast<double>(imageBlockRect.width);
    const double zoomY = static_cast<double>(requiredBlockSize.height) / static_cast<double>(imageBlockRect.height);
    const int level = findZoomLevelIndex(std::max(zoomX, zoomY));
    const auto& directories = m_pfile->directories();
    const slideio::NDPITiffDirectory& dir = directories[m_startDir + level];

    cv::Rect dirBlockRect;
    scaleBlockToDirectory(imageBlockRect, dir, dirBlockRect);
    readResampledLevelBlockChannelsEx(level, dirBlockRect, requiredBlockSize, channelIndices,
                                      zSliceIndex, tFrameIndex, output);
}

void NDPIScene::readResampledLevelBlockChannelsEx(int level, const cv::Rect& levelRect,
        const cv::Size& blockSize, const std::vector<int>& channelIndices,
        int zSliceIndex, int tFrameIndex, cv::OutputArray output)
{
    if (zSliceIndex != 0 || tFrameIndex != 0) {
        RAISE_RUNTIME_ERROR << "NDPIScene: 3D and 4D images are not supported";
    }
    validateLevel(level);
    const auto& directories = m_pfile->directories();
    const slideio::NDPITiffDirectory& dir = directories[m_startDir + level];

    NDPITiffTools::setCurrentDirectory(m_pfile->getTiffHandle(), dir);
    NDPIUserData data(&dir, getFilePath());
    const auto dirType = dir.getType();
    if (dirType == NDPITiffDirectory::Type::Tiled
        || dirType == NDPITiffDirectory::Type::SingleStripeMCU
        || dirType == NDPITiffDirectory::Type::Striped) {
        // TileComposer intersects every tile with the block, so an overhanging block simply
        // finds no tile there and keeps the background.
        TileComposer::composeRect(this, channelIndices, levelRect, blockSize, output, (void*)&data);
    } else if (dirType == NDPITiffDirectory::Type::SingleStripe) {
        cv::Mat raster;
        NDPITiffTools::readStripedDir(m_pfile->getTiffHandle(), dir, raster);
        // cv::Mat(raster, rect) throws unless the rect is contained, and an edge tile of a
        // level is not: clamp, read the part that exists, and leave the rest background.
        const cv::Rect valid = levelRect & cv::Rect(0, 0, raster.cols, raster.rows);
        initializeSceneBlock(blockSize, channelIndices, output);
        if (valid.empty()) {
            return;
        }
        const double scaleX = static_cast<double>(blockSize.width) / static_cast<double>(levelRect.width);
        const double scaleY = static_cast<double>(blockSize.height) / static_cast<double>(levelRect.height);
        cv::Rect target;
        target.x = static_cast<int>(std::floor((valid.x - levelRect.x) * scaleX));
        target.y = static_cast<int>(std::floor((valid.y - levelRect.y) * scaleY));
        target.width = std::min(static_cast<int>(std::ceil(valid.width * scaleX)), blockSize.width - target.x);
        target.height = std::min(static_cast<int>(std::ceil(valid.height * scaleY)), blockSize.height - target.y);
        if (target.width <= 0 || target.height <= 0) {
            return;
        }
        cv::Mat block(raster, valid);
        cv::Mat blockResized;
        cv::resize(block, blockResized, target.size());
        cv::Mat extracted;
        Tools::extractChannels(blockResized, channelIndices, extracted);
        cv::Mat out = output.getMat();
        extracted.copyTo(out(target));
    } else {
        RAISE_RUNTIME_ERROR << "NDPIScene::readResampledLevelBlockChannelsEx: Unexpected directory type: "
            << dir.getType();
    }
}

int NDPIScene::findZoomLevelIndex(double zoom) const
{
    // The body of NDPIFile::findZoomDirectory (ndpifile.cpp:55-63) without its final
    // index-to-reference step, so the level it names is the level that function would.
    const auto& directories = m_pfile->directories();
    const int sceneWidth = m_rect.width;
    const int begin = m_startDir;
    return Tools::findZoomLevel(zoom, m_endDir - m_startDir,
        [&directories, sceneWidth, begin](int index) {
            return static_cast<double>(directories[index + begin].width) / static_cast<double>(sceneWidth);
        });
}
```

Add `#include <cmath>` and `#include "slideio/core/tools/tools.hpp"` if they are not reachable.

- [ ] **Step 5: Run the tests**

Run: `./build/release/bin/slideio_ndpi_tests`
Expected: PASS, all pre-existing tests with unchanged expectations plus the two new ones.

- [ ] **Step 6: Commit**

```bash
git add src/slideio/drivers/ndpi/ndpiscene.hpp src/slideio/drivers/ndpi/ndpiscene.cpp \
        src/tests/ndpi/test_ndpi_driver.cpp
git commit -m "read a named zoom level directly in the ndpi scene"
```

---

### Task 6: OME-TIFF and PKE driver overrides

**Files:**
- Modify: `src/slideio/drivers/ome-tiff/otscene.hpp`, `src/slideio/drivers/ome-tiff/otscene.cpp:367-383`
- Modify: `src/slideio/drivers/pke/pketiledscene.hpp`, `src/slideio/drivers/pke/pketiledscene.cpp:181-201`
- Modify: `src/tests/ometiff/test_ometiff_driver.cpp`, `src/tests/pke/test_pke_driver.cpp`

**Interfaces:**
- Consumes: Task 1's virtual and `validateLevel`.
- Produces: `OTScene::readResampledLevelBlockChannelsEx`, `PKETiledScene::readResampledLevelBlockChannelsEx`.

Both drivers already index a level table; the split is mechanical. PKE keeps a second indirection, `m_zoomDirectoryIndices[level]`, mapping a level index to a directory index.

- [ ] **Step 1: Write the failing tests**

In each of `src/tests/ometiff/test_ometiff_driver.cpp` and `src/tests/pke/test_pke_driver.cpp`, add one test per file with this body, adapted to that file's fixture name, driver class and test image (reuse whichever multi-level image the file's existing pyramid tests already open, and keep that file's existing skip guard):

```cpp
    const int numLevels = scene->getNumZoomLevels();
    ASSERT_LE(2, numLevels);
    for (int level = 1; level < numLevels; ++level)
    {
        const slideio::LevelInfo* info = scene->getZoomLevelInfo(level);
        ASSERT_TRUE(info != nullptr) << "level " << level;
        const cv::Size levelSize(info->getSize().width, info->getSize().height);
        cv::Mat viaLevel, viaScene;
        scene->readResampledLevelBlockChannels(level, cv::Rect(cv::Point(0, 0), levelSize), levelSize, {}, viaLevel);
        scene->readResampledBlock(scene->getRect(), levelSize, viaScene);
        ASSERT_EQ(levelSize, viaLevel.size()) << "level " << level;
        EXPECT_LE(0.95, slideio::ImageTools::computeSimilarity2(viaLevel, viaScene)) << "level " << level;
    }
    // An overhanging rect is the ordinary edge tile and must come back background filled.
    const slideio::LevelInfo* last = scene->getZoomLevelInfo(numLevels - 1);
    const cv::Size lastSize(last->getSize().width, last->getSize().height);
    cv::Mat edge;
    ASSERT_NO_THROW(scene->readResampledLevelBlockChannels(
        numLevels - 1, cv::Rect(lastSize.width - 32, lastSize.height - 32, 128, 128),
        cv::Size(128, 128), {}, edge));
    EXPECT_EQ(cv::Size(128, 128), edge.size());
```

Name the tests `readLevelMatchesTheResampledSceneRead` in both files.

- [ ] **Step 2: Run the tests to verify they fail**

Run: `./build/release/bin/slideio_ometiff_tests --gtest_filter="*readLevel*"` and
`./build/release/bin/slideio_pke_tests --gtest_filter="*readLevel*"`
Expected: FAIL.

- [ ] **Step 3: Split the OME-TIFF read method**

Declare in `otscene.hpp`:

```cpp
    void readResampledLevelBlockChannelsEx(int level, const cv::Rect& levelRect,
        const cv::Size& blockSize, const std::vector<int>& channelIndices,
        int zSliceIndex, int tFrameIndex, cv::OutputArray output) override;
```

Replace `otscene.cpp:367-383` with:

```cpp
void OTScene::readResampledBlockChannelsEx(const cv::Rect& blockRect, const cv::Size& blockSize,
                                           const std::vector<int>& componentIndices, int zSliceIndex, int tFrameIndex,
                                           cv::OutputArray output) {
    const double zoomX = static_cast<double>(blockSize.width) / static_cast<double>(blockRect.width);
    const double zoomY = static_cast<double>(blockSize.height) / static_cast<double>(blockRect.height);
    const int zoomIndex = findZoomLevel(std::max(zoomX, zoomY));
    const LevelInfo& levelInfo = m_levels[zoomIndex];
    const double zoomDirX = static_cast<double>(levelInfo.getSize().width) / static_cast<double>(m_imageSize.width);
    const double zoomDirY = static_cast<double>(levelInfo.getSize().height) / static_cast<double>(m_imageSize.height);
    cv::Rect levelRect;
    Tools::scaleRect(blockRect, zoomDirX, zoomDirY, levelRect);
    readResampledLevelBlockChannelsEx(zoomIndex, levelRect, blockSize, componentIndices,
                                      zSliceIndex, tFrameIndex, output);
}

void OTScene::readResampledLevelBlockChannelsEx(int level, const cv::Rect& levelRect,
                                                const cv::Size& blockSize,
                                                const std::vector<int>& componentIndices,
                                                int zSliceIndex, int tFrameIndex, cv::OutputArray output) {
    validateLevel(level);
    auto channelIndices = Tools::completeChannelList(componentIndices, m_numChannels);
    const LevelInfo& levelInfo = m_levels[level];
    BlockInfo blockInfo = {&levelInfo, zSliceIndex, tFrameIndex, {}};
    collectTiffDataIndices(channelIndices, zSliceIndex, tFrameIndex, blockInfo.tiffDataIndices);
    TileComposer::composeRect(this, channelIndices, levelRect, blockSize, output, (void*)&blockInfo);
}
```

- [ ] **Step 4: Split the PKE read method**

Declare in `pketiledscene.hpp`:

```cpp
    void readResampledLevelBlockChannelsEx(int level, const cv::Rect& levelRect,
        const cv::Size& blockSize, const std::vector<int>& channelIndices,
        int zSliceIndex, int tFrameIndex, cv::OutputArray output) override;
```

Replace `pketiledscene.cpp:181-201` with:

```cpp
void PKETiledScene::readResampledBlockChannelsEx(const cv::Rect& blockRect, const cv::Size& blockSize,
    const std::vector<int>& channelIndices, int zSliceIndex, int tFrameIndex, cv::OutputArray output)
{
    const double zoomX = static_cast<double>(blockSize.width) / static_cast<double>(blockRect.width);
    const double zoomY = static_cast<double>(blockSize.height) / static_cast<double>(blockRect.height);
    const int zoomIndex = findZoomLevel(std::max(zoomX, zoomY));
    const int dirIndex = m_zoomDirectoryIndices[zoomIndex];
    const TiffDirectory& dir = m_directories[dirIndex];
    const double zoomDirX = static_cast<double>(dir.width) / static_cast<double>(m_directories[0].width);
    const double zoomDirY = static_cast<double>(dir.height) / static_cast<double>(m_directories[0].height);
    cv::Rect levelRect;
    Tools::scaleRect(blockRect, zoomDirX, zoomDirY, levelRect);
    readResampledLevelBlockChannelsEx(zoomIndex, levelRect, blockSize, channelIndices,
                                      zSliceIndex, tFrameIndex, output);
}

void PKETiledScene::readResampledLevelBlockChannelsEx(int level, const cv::Rect& levelRect,
    const cv::Size& blockSize, const std::vector<int>& channelIndices,
    int zSliceIndex, int tFrameIndex, cv::OutputArray output)
{
	if (zSliceIndex != 0 || tFrameIndex != 0) {
		RAISE_RUNTIME_ERROR << "PKEDriver: 3D and 4D images are not supported";
	}
    validateLevel(level);
    auto hFile = getFileHandle();
    if (hFile == nullptr)
        throw std::runtime_error("PKEDriver: Invalid file header by raster reading operation");
    // The composer's userData is the level index; getTileCount and readTile resolve it to a
    // directory through m_zoomDirectoryIndices themselves.
    int levelIndex = level;
    TileComposer::composeRect(this, channelIndices, levelRect, blockSize, output, (void*)&levelIndex);
}
```

- [ ] **Step 5: Run the tests**

Run: `./build/release/bin/slideio_ometiff_tests` and `./build/release/bin/slideio_pke_tests`
Expected: PASS, all pre-existing tests unchanged.

- [ ] **Step 6: Commit**

```bash
git add src/slideio/drivers/ome-tiff/otscene.hpp src/slideio/drivers/ome-tiff/otscene.cpp \
        src/slideio/drivers/pke/pketiledscene.hpp src/slideio/drivers/pke/pketiledscene.cpp \
        src/tests/ometiff/test_ometiff_driver.cpp src/tests/pke/test_pke_driver.cpp
git commit -m "read a named zoom level directly in the ome-tiff and pke scenes"
```

---

### Task 7: CZI and DICOM WSI driver overrides

**Files:**
- Modify: `src/slideio/drivers/czi/cziscene.hpp`, `src/slideio/drivers/czi/cziscene.cpp:114-132`
- Modify: `src/slideio/drivers/dcm/wsiscene.hpp`, `src/slideio/drivers/dcm/wsiscene.cpp:141-158`
- Modify: `src/tests/main/test_czi_driver.cpp`, `src/tests/main/test_dcm_driver.cpp`

**Interfaces:**
- Consumes: Task 1's virtual and `validateLevel`.
- Produces: `CZIScene::readResampledLevelBlockChannelsEx`, `WSIScene::readResampledLevelBlockChannelsEx`.

Both drivers pass `userData.relativeZoom = levelZoom / zoom` to the composer. Because `levelRect.width == blockRect.width * levelZoom` and `blockSize.width == blockRect.width * zoom`, that quantity equals `levelRect.width / blockSize.width` — computable from the level-space arguments alone, which is why the split is possible without threading the original rectangle through.

- [ ] **Step 1: Write the failing tests**

Add one test to each of `src/tests/main/test_czi_driver.cpp` and `src/tests/main/test_dcm_driver.cpp`, named `readLevelMatchesTheResampledSceneRead`, using that file's existing fixture, driver class, test image and skip guard, with the loop body from Task 6 Step 1. CZI is multi-dimensional, so also assert that a level read of a single slice matches the corresponding `readResampled4DBlockChannels` slice on a file with more than one z slice, if that file's existing tests already establish one:

```cpp
    // The level path assembles slices through the same helper as the scene path, so a single
    // slice read has to agree with the corresponding plane of the scene read.
    if (scene->getNumZSlices() > 1) {
        const slideio::LevelInfo* info = scene->getZoomLevelInfo(0);
        const cv::Size levelSize(info->getSize().width, info->getSize().height);
        cv::Mat viaLevel, viaScene;
        scene->readResampledLevel4DBlockChannels(0, cv::Rect(cv::Point(0, 0), levelSize), levelSize, {},
                                                 cv::Range(1, 2), cv::Range(0, 1), viaLevel);
        scene->readResampled4DBlockChannels(scene->getRect(), levelSize, {},
                                            cv::Range(1, 2), cv::Range(0, 1), viaScene);
        ASSERT_EQ(viaScene.size(), viaLevel.size());
        EXPECT_LE(0.95, slideio::ImageTools::computeSimilarity2(viaLevel, viaScene));
    }
```

- [ ] **Step 2: Run the tests to verify they fail**

Run: `./build/release/bin/slideio_tests --gtest_filter="*readLevelMatchesTheResampledSceneRead*"`
Expected: FAIL.

- [ ] **Step 3: Split the CZI read method**

Declare in `cziscene.hpp`:

```cpp
    void readResampledLevelBlockChannelsEx(int level, const cv::Rect& levelRect,
        const cv::Size& blockSize, const std::vector<int>& componentIndices,
        int zSliceIndex, int tFrameIndex, cv::OutputArray output) override;
```

Replace `cziscene.cpp:114-132` with:

```cpp
void CZIScene::readResampledBlockChannelsEx(const cv::Rect& blockRect, const cv::Size& blockSize,
                                            const std::vector<int>& componentIndices, int zSliceIndex, int tFrameIndex, cv::OutputArray output)
{
    const double zoomX = static_cast<double>(blockSize.width) / static_cast<double>(blockRect.width);
    const double zoomY = static_cast<double>(blockSize.height) / static_cast<double>(blockRect.height);
    const double zoom = std::max(zoomX, zoomY);
    const std::vector<ZoomLevel>& zoomLevels = m_zoomLevels;
    const int level = Tools::findZoomLevel(zoom, static_cast<int>(m_zoomLevels.size()), [&zoomLevels](int index){
        return zoomLevels[index].zoom;
    });
    const double levelZoom = zoomLevels[level].zoom;
    cv::Rect levelRect;
    Tools::scaleRect(blockRect, levelZoom, levelZoom, levelRect);
    readResampledLevelBlockChannelsEx(level, levelRect, blockSize, componentIndices,
                                      zSliceIndex, tFrameIndex, output);
}

void CZIScene::readResampledLevelBlockChannelsEx(int level, const cv::Rect& levelRect,
                                                 const cv::Size& blockSize,
                                                 const std::vector<int>& componentIndices,
                                                 int zSliceIndex, int tFrameIndex, cv::OutputArray output)
{
    validateLevel(level);
    TilerData userData;
    userData.zoomLevelIndex = level;
    // relativeZoom was levelZoom/zoom; with the rect already in level coordinates that is
    // the ratio of the level rect to the output block.
    userData.relativeZoom = (blockSize.width > 0)
        ? static_cast<double>(levelRect.width) / static_cast<double>(blockSize.width)
        : 1.;
    userData.zSliceIndex = zSliceIndex + m_firstSliceIndex;
    userData.tFrameIndex = tFrameIndex + m_firstTFrameIndex;
    TileComposer::composeRect(this, componentIndices, levelRect, blockSize, output, &userData);
}
```

- [ ] **Step 4: Split the DICOM WSI read method**

Declare in `wsiscene.hpp`:

```cpp
	void readResampledLevelBlockChannelsEx(int level, const cv::Rect& levelRect,
		const cv::Size& blockSize, const std::vector<int>& componentIndices,
		int zSliceIndex, int tFrameIndex, cv::OutputArray output) override;
```

Replace `wsiscene.cpp:141-158` with:

```cpp
void WSIScene::readResampledBlockChannelsEx(const cv::Rect& blockRect, const cv::Size& blockSize,
    const std::vector<int>& componentIndices, int zSliceIndex, int tFrameIndex, cv::OutputArray output) {
	const double zoomX = static_cast<double>(blockSize.width) / static_cast<double>(blockRect.width);
	const double zoomY = static_cast<double>(blockSize.height) / static_cast<double>(blockRect.height);
	const double zoom = std::max(zoomX, zoomY);
	auto& files = m_files;
	const int level = Tools::findZoomLevel(zoom, static_cast<int>(m_files.size()), [&files](int index) {
		return files[index]->getScale();
		});
	const double levelZoom = files[level]->getScale();
	cv::Rect levelRect;
	Tools::scaleRect(blockRect, levelZoom, levelZoom, levelRect);
	readResampledLevelBlockChannelsEx(level, levelRect, blockSize, componentIndices,
	                                  zSliceIndex, tFrameIndex, output);
}

void WSIScene::readResampledLevelBlockChannelsEx(int level, const cv::Rect& levelRect,
	const cv::Size& blockSize, const std::vector<int>& componentIndices,
	int zSliceIndex, int tFrameIndex, cv::OutputArray output) {
	validateLevel(level);
	TilerData userData;
	userData.zoomLevelIndex = level;
	// relativeZoom was levelZoom/zoom; with the rect already in level coordinates that is
	// the ratio of the level rect to the output block.
	userData.relativeZoom = (blockSize.width > 0)
		? static_cast<double>(levelRect.width) / static_cast<double>(blockSize.width)
		: 1.;
	userData.zSliceIndex = zSliceIndex;
	userData.tFrameIndex = tFrameIndex;
	TileComposer::composeRect(this, componentIndices, levelRect, blockSize, output, &userData);
}
```

- [ ] **Step 5: Run the tests**

Run: `./build/release/bin/slideio_tests --gtest_filter="*CZI*:*DCM*:*Dcm*"`
Expected: PASS, all pre-existing tests unchanged.

- [ ] **Step 6: Commit**

```bash
git add src/slideio/drivers/czi/cziscene.hpp src/slideio/drivers/czi/cziscene.cpp \
        src/slideio/drivers/dcm/wsiscene.hpp src/slideio/drivers/dcm/wsiscene.cpp \
        src/tests/main/test_czi_driver.cpp src/tests/main/test_dcm_driver.cpp
git commit -m "read a named zoom level directly in the czi and dicom wsi scenes"
```

---

### Task 8: VSI ETS driver override

**Files:**
- Modify: `src/slideio/drivers/vsi/etsfilescene.hpp`, `src/slideio/drivers/vsi/etsfilescene.cpp:151-180`
- Modify: `src/tests/vsi/test_vsi_driver.cpp`

**Interfaces:**
- Consumes: Task 1's virtual and `validateLevel`.
- Produces: `EtsFileScene::readResampledLevelBlockChannelsEx`.

The ETS scene already validates its level index and already carries it in `TileComposerUserData`. Note the inversion: `PyramidLevel::getScaleLevel()` is a divisor, so the zoom is `1. / getScaleLevel()`.

- [ ] **Step 1: Write the failing test**

Add `readLevelMatchesTheResampledSceneRead` to `src/tests/vsi/test_vsi_driver.cpp`, using that file's existing fixture, image and skip guard, with the loop body from Task 6 Step 1.

- [ ] **Step 2: Run the test to verify it fails**

Run: `./build/release/bin/slideio_vsi_tests --gtest_filter="*readLevel*"`
Expected: FAIL.

- [ ] **Step 3: Declare the override**

In `etsfilescene.hpp`:

```cpp
    void readResampledLevelBlockChannelsEx(int level, const cv::Rect& levelRect,
        const cv::Size& blockSize, const std::vector<int>& channelIndices,
        int zSliceIndex, int tFrameIndex, cv::OutputArray output) override;
```

- [ ] **Step 4: Split the read method**

Replace `etsfilescene.cpp:151-180` with:

```cpp
void EtsFileScene::readResampledBlockChannelsEx(const cv::Rect& blockRect, const cv::Size& blockSize,
                                                const std::vector<int>& channelIndices, int zSliceIndex,
                                                int tFrameIndex, cv::OutputArray output) {
    const auto etsFile = getEtsFile();
    if (!etsFile) {
        RAISE_RUNTIME_ERROR << "VSIImageDriver: ETS file is not initialized";
    }
    const double zoomX = static_cast<double>(blockSize.width) / static_cast<double>(blockRect.width);
    const double zoomY = static_cast<double>(blockSize.height) / static_cast<double>(blockRect.height);
    const int levelIndex = findZoomLevelIndex(std::max(zoomX, zoomY));
    if (levelIndex < 0 || levelIndex >= etsFile->getNumPyramidLevels()) {
        RAISE_RUNTIME_ERROR << "VSIImageDriver: Unexpected zoom level index: "
            << levelIndex << " Expected: " << "0 - " << etsFile->getNumPyramidLevels();
    }
    const PyramidLevel& level = etsFile->getPyramidLevel(levelIndex);
    // getScaleLevel is a divisor, so the zoom of the level is its reciprocal.
    const double levelZoom = 1. / level.getScaleLevel();
    cv::Rect levelRect;
    Tools::scaleRect(blockRect, levelZoom, levelZoom, levelRect);
    readResampledLevelBlockChannelsEx(levelIndex, levelRect, blockSize, channelIndices,
                                      zSliceIndex, tFrameIndex, output);
}

void EtsFileScene::readResampledLevelBlockChannelsEx(int level, const cv::Rect& levelRect,
                                                     const cv::Size& blockSize,
                                                     const std::vector<int>& channelIndices,
                                                     int zSliceIndex, int tFrameIndex,
                                                     cv::OutputArray output) {
    const auto etsFile = getEtsFile();
    if (!etsFile) {
        RAISE_RUNTIME_ERROR << "VSIImageDriver: ETS file is not initialized";
    }
    const auto volume = etsFile->getVolume();
    if (!volume) {
        RAISE_RUNTIME_ERROR << "VSIImageDriver: ETS file does not contain volume";
    }
    validateLevel(level);
    TileComposerUserData userData;
    userData.levelIndex = level;
    userData.zSlice = zSliceIndex;
    userData.tFrame = tFrameIndex;
    TileComposer::composeRect(this, channelIndices, levelRect, blockSize, output, (void*)&userData);
}
```

- [ ] **Step 5: Run the tests**

Run: `./build/release/bin/slideio_vsi_tests`
Expected: PASS, all pre-existing tests unchanged.

- [ ] **Step 6: Commit**

```bash
git add src/slideio/drivers/vsi/etsfilescene.hpp src/slideio/drivers/vsi/etsfilescene.cpp \
        src/tests/vsi/test_vsi_driver.cpp
git commit -m "read a named zoom level directly in the vsi ets scene"
```

---

### Task 9: SCN driver override

**Files:**
- Modify: `src/slideio/drivers/scn/scnscene.hpp`
- Modify: `src/slideio/drivers/scn/scnscene.cpp:46-86`
- Modify: `src/tests/main/test_scn_driver.cpp`

**Interfaces:**
- Consumes: Task 1's virtual and `validateLevel`.
- Produces: `SCNScene::readResampledLevelBlockChannelsEx`.

SCN is the one driver whose level selection is per channel: `findZoomDirectory(channelIndex, zIndex, zoom)` searches each channel's own directory list, and `m_levels` is built from channel 0 at z 0 (`scnscene.cpp:252-267`). The override indexes each channel's list by the level directly instead of searching it. A channel whose list is shorter than the level resolves to `nullptr`, which `SCNTilingInfo` already tolerates — the same outcome the zoom search produces today for such a channel.

- [ ] **Step 1: Write the failing test**

Add to `src/tests/main/test_scn_driver.cpp`, using that file's existing fixture, driver class, test image and skip guard:

```cpp
// SCN resolves a directory per channel, so a level read has to address every channel's own
// directory list by the same level index. Reading the whole of a level by level and reading
// the whole scene resampled to that size have to agree on every channel.
TEST(SCNImageDriverTests, readLevelMatchesTheResampledSceneRead)
{
    // ... open the scene the way the other tests in this file do ...
    const int numLevels = scene->getNumZoomLevels();
    ASSERT_LE(2, numLevels);
    for (int level = 1; level < numLevels; ++level)
    {
        const slideio::LevelInfo* info = scene->getZoomLevelInfo(level);
        ASSERT_TRUE(info != nullptr) << "level " << level;
        const cv::Size levelSize(info->getSize().width, info->getSize().height);
        cv::Mat viaLevel, viaScene;
        scene->readResampledLevelBlockChannels(level, cv::Rect(cv::Point(0, 0), levelSize), levelSize, {}, viaLevel);
        scene->readResampledBlock(scene->getRect(), levelSize, viaScene);
        ASSERT_EQ(levelSize, viaLevel.size()) << "level " << level;
        ASSERT_EQ(viaScene.channels(), viaLevel.channels()) << "level " << level;
        EXPECT_LE(0.95, slideio::ImageTools::computeSimilarity2(viaLevel, viaScene)) << "level " << level;
    }
    // Every channel individually, because the per-channel directory lookup is what this
    // driver does differently from all the others.
    const slideio::LevelInfo* last = scene->getZoomLevelInfo(numLevels - 1);
    const cv::Size lastSize(last->getSize().width, last->getSize().height);
    cv::Mat all;
    scene->readResampledLevelBlockChannels(numLevels - 1, cv::Rect(cv::Point(0, 0), lastSize), lastSize, {}, all);
    std::vector<cv::Mat> planes;
    cv::split(all, planes);
    for (int channel = 0; channel < scene->getNumChannels(); ++channel)
    {
        cv::Mat single;
        scene->readResampledLevelBlockChannels(numLevels - 1, cv::Rect(cv::Point(0, 0), lastSize),
                                               lastSize, {channel}, single);
        ASSERT_EQ(lastSize, single.size()) << "channel " << channel;
        EXPECT_EQ(0, cv::countNonZero(cv::abs(single - planes[channel]))) << "channel " << channel;
    }
}
```

- [ ] **Step 2: Run the test to verify it fails**

Run: `./build/release/bin/slideio_tests --gtest_filter="SCNImageDriverTests.readLevel*"`
Expected: FAIL.

- [ ] **Step 3: Declare the override**

In `scnscene.hpp`, next to `readResampledBlockChannelsEx`:

```cpp
    void readResampledLevelBlockChannelsEx(int level, const cv::Rect& levelRect,
        const cv::Size& blockSize, const std::vector<int>& channelIndices,
        int zSliceIndex, int tFrameIndex, cv::OutputArray output) override;
```

- [ ] **Step 4: Split the read method**

Replace `scnscene.cpp:46-86` with:

```cpp
void SCNScene::readResampledBlockChannelsEx(const cv::Rect& blockRect, const cv::Size& blockSize,
    const std::vector<int>& channelIndicesIn, int zSliceIndex, int tFrameIndex, cv::OutputArray output)
{
	if (tFrameIndex != 0) {
		throw std::runtime_error("SCNImageDriver: Time frames are not supported");
	}
    const double zoomX = static_cast<double>(blockSize.width) / static_cast<double>(blockRect.width);
    const double zoomY = static_cast<double>(blockSize.height) / static_cast<double>(blockRect.height);
    const double zoom = std::max(zoomX, zoomY);

    // The level index comes from channel 0's directory list, which is the list m_levels was
    // built from, so the index and the reported level geometry cannot disagree.
    const auto& baseDirectories = getChannelDirectories(0, zSliceIndex);
    const cv::Rect sceneRect = getRect();
    const double sceneWidth = static_cast<double>(sceneRect.width);
    const int level = Tools::findZoomLevel(zoom, (int)baseDirectories.size(),
        [&baseDirectories, sceneWidth](int index) {
            return baseDirectories[index].width / sceneWidth;
        });
    if (level < 0 || level >= (int)baseDirectories.size()) {
        RAISE_RUNTIME_ERROR << "SCNImageDriver: no zoom level serves zoom " << zoom;
    }
    const TiffDirectory& baseDir = baseDirectories[level];
    const double zoomDirX = static_cast<double>(baseDir.width) / static_cast<double>(m_rect.width);
    const double zoomDirY = static_cast<double>(baseDir.height) / static_cast<double>(m_rect.height);
    cv::Rect levelRect;
    Tools::scaleRect(blockRect, zoomDirX, zoomDirY, levelRect);
    readResampledLevelBlockChannelsEx(level, levelRect, blockSize, channelIndicesIn,
                                      zSliceIndex, tFrameIndex, output);
}

void SCNScene::readResampledLevelBlockChannelsEx(int level, const cv::Rect& levelRect,
    const cv::Size& blockSize, const std::vector<int>& channelIndicesIn,
    int zSliceIndex, int tFrameIndex, cv::OutputArray output)
{
	if (tFrameIndex != 0) {
		throw std::runtime_error("SCNImageDriver: Time frames are not supported");
	}
    validateLevel(level);
    auto hFile = getFileHandle();
    if (hFile == nullptr)
        throw std::runtime_error("SCNImageDriver: Invalid file handle by raster reading operation");

    auto channelIndices(channelIndicesIn);
    if (channelIndices.empty()) {
        for (int channelIndex = 0; channelIndex < m_numChannels; ++channelIndex) {
            channelIndices.push_back(channelIndex);
        }
    }

    SCNTilingInfo info;
    for (auto channelIndex : channelIndices) {
        const auto& directories = getChannelDirectories(channelIndex, zSliceIndex);
        // Each channel keeps its own directory list. They are parallel in every file seen so
        // far, so the level index addresses all of them; a channel whose list is shorter
        // resolves to nullptr, which the composer treats as a channel this level does not
        // carry -- the same outcome the zoom search gives for such a channel.
        info.channel2ifd[channelIndex] =
            (level < (int)directories.size()) ? &directories[level] : nullptr;
    }

    TileComposer::composeRect(this, channelIndices, levelRect, blockSize, output, (void*)&info);
}
```

Note that the level entry point completes the channel list before building `info`, where the old code completed it before the per-channel search. Same list, same order.

- [ ] **Step 5: Run the tests**

Run: `./build/release/bin/slideio_tests --gtest_filter="*SCN*"`
Expected: PASS, all pre-existing tests unchanged.

- [ ] **Step 6: Run the whole C++ gate**

Run, in order:
```
./build/release/bin/slideio_tests
./build/release/bin/slideio_converter_tests
./build/release/bin/slideio_transformer_tests
./build/release/bin/slideio_ndpi_tests
./build/release/bin/slideio_vsi_tests
./build/release/bin/slideio_pke_tests
./build/release/bin/slideio_ometiff_tests
./build/release/bin/slideio_phtiff_tests
```
Expected: PASS in all eight, with no edited expectations anywhere.

- [ ] **Step 7: Commit**

```bash
git add src/slideio/drivers/scn/scnscene.hpp src/slideio/drivers/scn/scnscene.cpp \
        src/tests/main/test_scn_driver.cpp
git commit -m "read a named zoom level directly in the scn scene"
```

---

### Task 10: Python bindings

**Files:** (all in `D:/Projects/slideio/slideio-python`)
- Modify: `src/pyscene.hpp` (declare `readBlockFromLevel`, add the bounds parameter to `adjustSourceRect`, add a dtype-check helper)
- Modify: `src/pyscene.cpp` (implement)
- Modify: `src/pybind.cpp` (bind the method and the two `LevelInfo` members)
- Modify: `slideio/wrappers/py_slideio.py` (wrapper method)

**Interfaces:**
- Consumes: `slideio::Scene::readResampledLevelBlockChannels` and `readResampledLevel4DBlockChannels` (Task 3); `slideio::LevelInfo::getTileCount()` and `getTileRect(int)`, which already exist.
- Produces: `Scene.read_block_from_level(level, rect=(0,0,0,0), size=(0,0), channel_indices=None, slices=(0,1), frames=(0,1))`; `LevelInfo.tile_count`; `LevelInfo.get_tile_rect(index)`.

This repository has no test suite. Verification is by build plus the manual script in Step 6.

- [ ] **Step 1: Point the python build at the local slideio build**

Set `SLIDEIO_INSTALL_DIR` to the slideio install produced by `python3 install.py -a install -c release`, so the bindings compile against the 2.9.0 headers from Tasks 1-9 rather than the 2.8.x conan package.

```bash
export SLIDEIO_INSTALL_DIR=/path/to/slideio/install    # Windows: $env:SLIDEIO_INSTALL_DIR = "..."
cd /path/to/slideio-python
python -m build
```

Expected: a successful baseline build before any edit. If this fails, stop — the rest of the task cannot be verified.

- [ ] **Step 2: Declare the new members**

In `src/pyscene.hpp`, add to the public section after `readBlock`:

```cpp
    pybind11::array readBlockFromLevel(int level, std::tuple<int,int,int,int> rect,
        std::tuple<int,int> size, std::vector<int> channelIndices,
        std::tuple<int,int> sliceRange, std::tuple<int,int> tframeRange) const;
```

and change the private helpers to:

```cpp
private:
    // bounds is the rectangle a zero width or height extends to: the scene rect for
    // read_block, the level rect for read_block_from_level.
    static PyRect adjustSourceRect(const PyRect& rect, const PyRect& bounds);
    PySize adjustTargetSize(const PyRect& rect, const PySize& size) const;
    // Every channel of a numpy array carries one dtype, so a selection mixing types cannot
    // be returned. Shared by both read methods.
    void validateChannelDataTypes(const std::vector<int>& channelIndices) const;
```

- [ ] **Step 3: Implement**

In `src/pyscene.cpp`, replace `adjustSourceRect` (lines 187-200) with:

```cpp
PyRect PyScene::adjustSourceRect(const PyRect& rect, const PyRect& bounds)
{
    PyRect srcRect(rect);
    if(srcRect.width()==0)
    {
        srcRect.width() = bounds.width() - srcRect.x();
    }
    if(srcRect.height()==0)
    {
        srcRect.height() = bounds.height() - srcRect.y();
    }
    return srcRect;
}
```

Extract the dtype check currently inlined at lines 128-144 into:

```cpp
void PyScene::validateChannelDataTypes(const std::vector<int>& channelIndices) const
{
    const int imageChannels = getNumChannels();
    const int refChannel = channelIndices.empty() ? 0 : channelIndices[0];
    const slideio::DataType refDataType = m_scene->getChannelDataType(refChannel);
    if (!channelIndices.empty()) {
        for (int idx : channelIndices) {
            if (m_scene->getChannelDataType(idx) != refDataType) {
                RAISE_PYERROR << "Cannot read channels with different data types into a single numpy array. "
                              << "Channel " << idx << " has a different data type than the reference channel " << refChannel
                              << ". Use channel_indices to select channels of the same type.";
            }
        }
    } else {
        for (int ch = 1; ch < imageChannels; ++ch) {
            if (m_scene->getChannelDataType(ch) != refDataType) {
                RAISE_PYERROR << "Cannot read channels with different data types into a single numpy array. "
                              << "Channel " << ch << " has a different data type than channel 0. "
                              << "Use channel_indices to select channels of the same type.";
            }
        }
    }
}
```

In `readBlock`, replace lines 128-144 with `validateChannelDataTypes(channelIndices);` and line 146 with:

```cpp
    PyRect blockRect = adjustSourceRect(rect, m_scene->getRect());
```

Then add the new method after `readBlock`:

```cpp
pybind11::array PyScene::readBlockFromLevel(int level, std::tuple<int, int, int, int> rect,
    std::tuple<int, int> size, std::vector<int> channelIndices,
    std::tuple<int,int> sliceRange, std::tuple<int,int> tframeRange) const
{
    const slideio::LevelInfo* levelInfo = m_scene->getLevelInfo(level);
    if (levelInfo == nullptr) {
        RAISE_PYERROR << "Unexpected null pointer received for zoom level: " << level;
    }
    const int imageChannels = getNumChannels();

    const int startSlice = std::max(0,std::get<0>(sliceRange));
    const int stopSlice = std::get<1>(sliceRange);
    const int startFrame = std::max(0,std::get<0>(tframeRange));
    const int stopFrame = std::get<1>(tframeRange);
    const int numSlices = std::max(1,stopSlice - startSlice);
    const int numFrames = std::max(1,stopFrame - startFrame);

    const int refChannel = channelIndices.empty()?0:channelIndices[0];
    const int numChannels = channelIndices.empty()?imageChannels:static_cast<int>(channelIndices.size());

    const py::dtype dtype = getChannelDataType(refChannel);
    validateChannelDataTypes(channelIndices);

    // A zero width or height extends to the edge of the level, not of the scene: rect is in
    // level coordinates throughout this method.
    const PyRect levelBounds(std::tuple<int,int,int,int>(0, 0, levelInfo->getSize().width,
                                                        levelInfo->getSize().height));
    PyRect blockRect = adjustSourceRect(rect, levelBounds);
    PySize blockSize = adjustTargetSize(blockRect, size);

    const int memSize = m_scene->getBlockSize(blockSize, refChannel, numChannels, numSlices, numFrames);

    py::array::ShapeContainer shape;
    shape->push_back(blockSize.height());
    shape->push_back(blockSize.width());
    if(numChannels>1)
        shape->push_back(numChannels);
    if(numSlices>1)
        shape->insert(shape->begin(), numSlices);
    if(numFrames>1)
        shape->insert(shape->begin(), numFrames);

    py::array numpy_array(dtype, shape);

    if(startSlice==0 && stopSlice<=1 && startFrame==0 && stopFrame<=1)
    {
        py::gil_scoped_release release;
        m_scene->readResampledLevelBlockChannels(level, blockRect, blockSize, channelIndices,
                                                 numpy_array.mutable_data(), memSize);
    }
    else
    {
        if(stopSlice<=startSlice)
        {
            RAISE_PYERROR << "Invalid slice range (" << startSlice << "," << stopSlice << ")";
        }
        if(stopFrame<=startFrame)
        {
            RAISE_PYERROR << "Invalid time frame range (" << startFrame << "," << stopFrame << ")";
        }
        py::gil_scoped_release release;
        m_scene->readResampledLevel4DBlockChannels(level, blockRect, blockSize, channelIndices,
                                                   sliceRange, tframeRange,
                                                   numpy_array.mutable_data(), memSize);
    }

    return numpy_array;
}
```

`m_scene->getLevelInfo(level)` raises `slideio::RuntimeError` for an out-of-range level, which the binding's exception translator already maps to a Python exception, so the level check needs no separate branch.

- [ ] **Step 4: Bind the method and the two `LevelInfo` members**

In `src/pybind.cpp`, after the `read_block` binding (line 98, before `.def("__repr__", ...)`):

```cpp
        .def("read_block_from_level", &PyScene::readBlockFromLevel,
            py::arg("level"),
            py::arg("rect") = std::tuple<int, int, int, int>(0, 0, 0, 0),
            py::arg("size") = std::tuple<int, int>(0, 0),
            py::arg("channel_indices") = std::vector<int>(),
            py::arg("slices") = std::tuple<int, int>(0, 1),
            py::arg("frames") = std::tuple<int, int>(0, 1),
            R"del(
            Reads a rectangular block from an explicitly selected zoom level.

            Unlike read_block, this method reads from the level you name and no other, and
            the rectangle is given in that level's own pixel coordinates. Use it when you
            already know which level you want -- a tiled viewer, for instance -- so that no
            coordinate conversion and no implicit level selection happen on the way.

            Args:
                level: index of the zoom level, in range(scene.num_zoom_levels).
                rect: block rectangle in the coordinate system of the level, as a tuple (x, y, width, height). A width or height of 0 extends to the edge of the level. Parts of the rectangle outside the level are filled with the background value.
                size: size of the block after rescaling. (0,0) - no scaling, native level pixels. Rescaling is performed from the named level only.
                channel_indices: array of channel indices to be retrieved. [] - all channels.
                slices: range of z slices (first, last+1) to be retrieved.
                frames: range of time frames (first, last+1) to be retrieved.

            Returns:
                numpy array with pixel values
            )del"
        )
```

and in the `LevelInfo` class binding (lines 255-261), after `magnification`:

```cpp
        .def_property_readonly("tile_count", &slideio::LevelInfo::getTileCount, "Number of tiles of the level")
        .def("get_tile_rect", &slideio::LevelInfo::getTileRect, py::arg("index"),
             "Returns the rectangle of a tile in level coordinates. Edge tiles overhang the level.")
```

- [ ] **Step 5: Add the Python wrapper method**

In `slideio/wrappers/py_slideio.py`, after `read_block` (line 177):

```python
    def read_block_from_level(self, level, rect=(0,0,0,0), size=(0,0), channel_indices=None, slices=(0,1), frames=(0,1)):
        '''Reads a rectangular block from an explicitly selected zoom level.

        Unlike read_block, this reads from the level you name and no other, and rect is
        given in that level's own pixel coordinates. Use it when you already know the
        level you want, so that no coordinate conversion and no implicit level selection
        happen on the way.

        Args:
            level: index of the zoom level, in range(scene.num_zoom_levels)
            rect: block rectangle in the coordinate system of the level, as a tuple (x, y, width, height). A width or height of 0 extends to the edge of the level. Parts outside the level come back as background.
            size: size of the block after rescaling. (0,0) - no scaling. Rescaling is performed from the named level only.
            channel_indices: array of channel indices to be retrieved. None or [] - all channels.
            slices: range of z slices (first, last+1) to be retrieved.
            frames: range of time frames (first, last+1) to be retrieved.

        Returns:
            numpy array with pixel values
        '''
        if channel_indices is None:
            channel_indices = []
        return self.scene.read_block_from_level(level, rect, size, channel_indices, slices, frames)
```

- [ ] **Step 6: Build and verify manually**

```bash
python -m build
pip install --force-reinstall dist/slideio-*.whl
```

Then run this script against any multi-level slide available locally, substituting the path:

```python
import numpy as np
import slideio

slide = slideio.open_slide(r"<path to a multi level slide>", "AUTO")
scene = slide.get_scene(0)
print("levels:", scene.num_zoom_levels)

level = scene.num_zoom_levels - 1
info = scene.get_zoom_level_info(level)
print("level size:", info.size.width, info.size.height, "tiles:", info.tile_count)

# Whole level, native pixels, no size argument.
whole = scene.read_block_from_level(level)
assert whole.shape[0] == info.size.height, whole.shape
assert whole.shape[1] == info.size.width, whole.shape

# Tile by tile through get_tile_rect, stitched back into the level.
stitched = np.zeros_like(whole)
for i in range(info.tile_count):
    r = info.get_tile_rect(i)
    tile = scene.read_block_from_level(level, (r.x, r.y, r.width, r.height))
    assert tile.shape[0] == r.height and tile.shape[1] == r.width, tile.shape
    h = min(r.height, info.size.height - r.y)
    w = min(r.width, info.size.width - r.x)
    stitched[r.y:r.y + h, r.x:r.x + w] = tile[:h, :w]
assert np.array_equal(stitched, whole), "tiles do not reconstruct the level"

# An out of range level raises rather than returning something plausible.
try:
    scene.read_block_from_level(scene.num_zoom_levels)
    raise AssertionError("an out of range level must raise")
except Exception as error:
    print("out of range level raised:", error)

print("ok")
```

Expected: `ok`, with no assertion failure.

- [ ] **Step 7: Commit (in the slideio-python repository)**

```bash
git add src/pyscene.hpp src/pyscene.cpp src/pybind.cpp slideio/wrappers/py_slideio.py
git commit -m "add read_block_from_level and the level tile helpers"
```

---

### Task 11: Documentation, breaking changes and debt

**Files:**
- Modify: `software-docs/BREAKING_CHANGES.md`
- Modify: `software-docs/TECH_DEBT.md`
- Modify: `software-docs/cpp-interface.md`
- Modify: `docs/` — the public Jekyll site page covering the `Scene` API

**Interfaces:**
- Consumes: everything from Tasks 1-10.
- Produces: no code.

- [ ] **Step 1: Record the ABI break**

Append to `software-docs/BREAKING_CHANGES.md`, matching the format of the existing entries:

```markdown
## 2.9.0 — `CVScene` gains a virtual method

`CVScene::readResampledLevelBlockChannelsEx` was added as a virtual method, which
changes the vtable layout of `CVScene` and of every class deriving from it. This is a
binary incompatibility: an out-of-tree driver or an application linked against
slideio 2.8.x must be recompiled against 2.9.0. Source compatibility is unaffected —
no existing signature, default argument or documented behaviour changed, and the new
method carries a working default implementation, so a driver that does not override it
continues to build and to read correctly.

The python bindings must be rebuilt against 2.9.0. The version appears in
`conanfile.txt`, `build-dependencies.ps1` and `conan.sh` of the slideio-python
repository, and all three must move together.
```

- [ ] **Step 2: Record the locking asymmetry as debt**

Append to `software-docs/TECH_DEBT.md`, matching the format of the existing entries:

```markdown
## `CVScene` serialises every block read, and does so inconsistently

`CVScene::readResampledBlockChannels` and `readResampledLevelBlockChannels` each take
`m_readBlockMutex` for the whole read, so no two block reads of one scene ever overlap.
A tiled viewer fetching tiles from a thread pool — the workload issue #69 describes —
therefore gets no concurrency from the library, and this is likely to dominate whatever
the level-addressed read path saves it.

Inside `assemble4DBlock` the same mutex is taken in the single-plane branch and not in
the multi-plane one. That asymmetry predates the level api; it was carried over
unchanged when the plane loop was extracted for 2.9.0, deliberately, so that the
extraction stayed behaviour-preserving.

Fixing either needs a thread-safety audit of the driver state each `readTile`
implementation touches — the tiff handle above all, which several drivers share across
a whole slide. Out of scope for the level api and recorded here so it is not lost.
```

- [ ] **Step 3: Document the new methods for C++ consumers**

Add a section to `software-docs/cpp-interface.md` covering `Scene::readResampledLevelBlockChannels` and `readResampledLevel4DBlockChannels`: the level coordinate system, that no level selection happens, that `blockSize == levelRect.size()` means no resampling, and the background-fill rule for overhanging rectangles. Follow the structure the file already uses for the `readBlock` family.

- [ ] **Step 4: Document `read_block_from_level` on the public site**

Add the method to the `Scene` page under `docs/`, following the format of the existing `read_block` entry, and include the tile-loop example from the spec's §5.5. Do not put internal design notes on the public site.

- [ ] **Step 5: Commit**

```bash
git add software-docs/BREAKING_CHANGES.md software-docs/TECH_DEBT.md \
        software-docs/cpp-interface.md docs/
git commit -m "document level-addressed reading and record the read mutex as debt"
```

- [ ] **Step 6: Reply on the issue**

Post a comment on https://github.com/Booritas/slideio/issues/69 summarising what shipped: `read_block_from_level` with rect in level coordinates, `LevelInfo.tile_count` and `get_tile_rect`, the background-fill rule for edge tiles, and the honest caveat that all block reads of a scene still serialise on one mutex, tracked separately.

---

## Self-Review

**Spec coverage.** §5.1 core contract → Task 1. §5.2 driver overrides → Tasks 4-9, one per driver named in the spec (svs/phtiff/afi, ndpi, scn, czi, ome-tiff, pke, dcm-wsi, vsi-ets). §5.3 public C++ API → Task 3. §5.4 Python API → Task 10 Steps 2-5. §5.5 `LevelInfo` bindings → Task 10 Step 4. §5.6 GDAL and PKESmallScene level tables → Task 2. §6 worked example → Task 10 Step 6 and Task 11 Step 4. §7 testing → the test steps of every task, with the full gate in Task 9 Step 6. §8 compatibility → Task 11 Steps 1 and 3-4. §10 out-of-scope items → Task 11 Step 2 records the mutex as debt rather than fixing it, as the spec requires.

**Two places the plan is deliberately less prescriptive than elsewhere.** Task 6, Task 7, Task 8 and Task 9 give the shared test body in full but tell the implementer to adapt the fixture name, driver class and test image to the file being extended, because those differ per suite and are visible in the file. Task 2 Step 1 does the same for the PKE small-scene test. That is adaptation of named, inspectable details, not a gap to be filled with invention.

**Type and name consistency.** The virtual is `readResampledLevelBlockChannelsEx` in all nine places it appears; the `CVScene` and `Scene` wrappers are `readResampledLevelBlockChannels` and `readResampledLevel4DBlockChannels` throughout; the Python method is `read_block_from_level` in the binding, the docstring, the wrapper and the verification script. The level-index finders introduced by Tasks 4 and 5 are both named `findZoomLevelIndex` and both return `int`. `TestScene::Request` is defined in Task 1 Step 1 and used only there.
