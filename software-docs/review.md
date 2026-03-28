# SlideIO Code Review

Comprehensive review of the SlideIO C++ codebase (v2.8.0) covering bugs, performance, memory management, architecture, build system, testing, and maintainability.

---

## Table of Contents

1. [Critical Bugs](#1-critical-bugs)
2. [High-Severity Bugs](#2-high-severity-bugs)
3. [Medium-Severity Bugs](#3-medium-severity-bugs)
4. [Low-Severity Bugs](#4-low-severity-bugs)
5. [Architecture](#5-architecture)
6. [Performance](#6-performance)
7. [Memory Management](#7-memory-management)
8. [Thread Safety](#8-thread-safety)
9. [Security (Input Validation)](#9-security-input-validation)
10. [Build System](#10-build-system)
11. [Testing](#11-testing)
12. [CI/CD & Docker](#12-cicd--docker)
13. [Maintainability](#13-maintainability)

---

## 1. Critical Bugs

### 1.1 Uninitialized member read in RuntimeError copy constructor

**File:** `src/slideio/base/exceptions.hpp:23-29`

The copy constructor reads `m_shown` before it is initialized, which is undefined behavior:
```cpp
RuntimeError(RuntimeError& rhs) {
    std::string message = rhs.m_innerStream.str();
    if(!m_shown) {  // m_shown is UNINITIALIZED at this point
        log(message);
    }
    m_innerStream << message;
}
```
**Fix:** Initialize `m_shown = false` before the check, or use a member initializer list.

### 1.2 Wrong matrix copied in CVSmallScene::readResampledBlockChannelsEx

**File:** `src/slideio/core/cvsmallscene.cpp:37-42`

```cpp
if(output.empty()) {
    output.assign(resizedBlock);
} else {
    imageBlock.copyTo(output);  // BUG: copies original, not resized
}
```
When `output` is not empty, the function copies `imageBlock` instead of `resizedBlock`, silently ignoring the resizing step.

### 1.3 Duplicate TIFF resolution unit condition (dead code)

**File:** `src/slideio/imagetools/tifftools.cpp:511-518`

```cpp
if (units == RESUNIT_INCH && resx > 0 && resy > 0) {
    dir.res.x = 0.01 / resx;
    dir.res.y = 0.01 / resy;
}
else if (units == RESUNIT_INCH && resx > 0 && resy > 0) {  // identical condition
    dir.res.x = 0.0254 / resx;  // never reached
    dir.res.y = 0.0254 / resy;
}
```
The second branch (presumably for `RESUNIT_CENTIMETER`) is unreachable. Resolution calculation is wrong for centimeter-based TIFF files.

### 1.4 YCbCrSubsampling both values written to same index

**File:** `src/slideio/imagetools/tifftools.cpp:507`

```cpp
libtiff::TIFFGetField(tiff, TIFFTAG_YCBCRSUBSAMPLING, &YCbCrSubsampling[0], &YCbCrSubsampling[0]);
```
Both arguments write to `[0]`. Should be `&YCbCrSubsampling[0], &YCbCrSubsampling[1]`.

### 1.5 Logic error in SmallTiffWrapper bounds check

**File:** `src/slideio/imagetools/smalltiffwrapper.cpp:98`

```cpp
if (pageIndex < 0 && pageIndex >= getNumPages()) {  // always false
```
Condition uses `&&` instead of `||`. A negative index or index beyond page count will pass this check and cause out-of-bounds access.

### 1.6 Array index confusion in readRegularTile channel extraction

**File:** `src/slideio/imagetools/tifftools.cpp:779-786`

```cpp
for (int channelIndex : channelIndices) {
    cv::extractChannel(tileRaster, channelRasters[channelIndex], channelIndices[channelIndex]);
}
```
Uses `channelIndex` (the channel number from the vector) as an index back into `channelRasters` and `channelIndices`. If `channelIndices = {0, 2, 4}`, accessing `channelRasters[4]` when it has size 3 is out-of-bounds.

---

## 2. High-Severity Bugs

### 2.1 Null pointer dereference in RefCounterGuard

**File:** `src/slideio/core/refcounter.hpp:30-34`

```cpp
RefCounterGuard(RefCounter* counter) : m_counter(counter) {
    m_counter->increaseCounter();  // crashes if counter is nullptr
}
```

### 2.2 Unchecked buffer access in CZI driver

**File:** `src/slideio/drivers/czi/cziscene.cpp:462`

```cpp
const uint8_t* channelData = blockData.data() + channelOffset;
```
`channelOffset` is checked for negative values but not against `blockData.size()`. A malformed CZI file can cause out-of-bounds read.

### 2.3 Missing bounds check in CZI tile access

**File:** `src/slideio/drivers/czi/cziscene.cpp:343-345`

```cpp
const ZoomLevel& zoomLevel = m_zoomLevels[zoomLevelIndex];
const Tiles& tiles = zoomLevel.tiles;
tileRect = tiles[tileIndex].rect;
```
Neither `zoomLevelIndex` nor `tileIndex` are bounds-checked.

### 2.4 Unchecked memcpy in CZI driver

**File:** `src/slideio/drivers/czi/cziscene.cpp:472`

```cpp
std::memcpy(trg, channelData, channelSize);
```
No validation that the output buffer has space for `channelSize` bytes. `channelSize` originates from file data without upper bounds validation.

### 2.5 Missing bounds check in readNotRGBTile

**File:** `src/slideio/imagetools/tifftools.cpp:873-882`

```cpp
const int correctedIndex = channelMapping[channelIndices[channelIndex]];
```
No bounds checking on `channelIndices[channelIndex]` before accessing `channelMapping` (size 4). Out-of-bounds access for images with >4 channels.

### 2.6 Resource leak: raw new in ImageDriverManager

**File:** `src/slideio/slideio/imagedrivermanager.cpp:82-85`

```cpp
SVSImageDriver* driver = new SVSImageDriver;  // leak if next line throws
std::shared_ptr<ImageDriver> svs(driver);
```
Inconsistent pattern: some drivers use `make_shared`, others use raw `new`. If the `shared_ptr` constructor throws (unlikely but possible), memory leaks.

### 2.7 NDPI driver uses raw new without exception safety

**File:** `src/slideio/drivers/ndpi/ndpislide.cpp:85-86`

```cpp
m_pfile = new NDPIFile;
m_pfile->init(m_filePath);
```
If `init()` throws, `m_pfile` leaks.

### 2.8 Null pointer in FreeImage wrapper

**File:** `src/slideio/imagetools/fiwrapper.cpp:22`

```cpp
const char* key = FreeImage_GetTagKey(tag);
const char* desc = FreeImage_GetTagDescription(tag);
// key and desc used without null checks
```

---

## 3. Medium-Severity Bugs

### 3.1 Integer overflow in area calculations

**File:** `src/slideio/base/rect.hpp:49-50`

```cpp
int32_t area() const { return width * height; }  // overflows for large dimensions
```
Same issue in `src/slideio/base/size.hpp:54-56` and `src/slideio/core/tools/blocktiler.cpp:40-41`.

**Fix:** Use `int64_t` or add overflow checks.

### 3.2 Unsafe void* casting in endian conversion

**File:** `src/slideio/core/tools/endian.hpp:109-140`

```cpp
fromLittleEndianToNative(static_cast<int16_t*>(data), count / sizeof(int16_t));
```
If `count < sizeof(int16_t)`, division yields 0 and the call is silently skipped. No alignment validation.

### 3.3 Integer overflow in ETS file dimension calculations

**File:** `src/slideio/drivers/vsi/etsfile.cpp:127-132`

```cpp
const int minWidth = m_maxCoordinates[0] * m_tileSize.width;
```
Multiplication of two `int` values from untrusted file data can overflow.

### 3.4 CZI file position arithmetic overflow

**File:** `src/slideio/drivers/czi/czislide.cpp:372`

```cpp
m_fileStream.seekg(entryHeader.filePosition + originPos);
```
No overflow check when adding two potentially large 64-bit values from file data.

### 3.5 Endianness conversion on potentially failed read

**File:** `src/slideio/drivers/czi/czislide.cpp:298-309`

```cpp
m_fileStream.read((char*)&header, sizeof(header));
updateSegmentHeaderBE(header);  // proceeds even if read failed
```

### 3.6 TOCTOU in ETS dimension access

**File:** `src/slideio/drivers/vsi/etsfile.cpp:45-47`

```cpp
const int zIndex = m_volume->getDimensionOrder(Dimensions::Z);
if (zIndex > 1 && zIndex < m_maxCoordinates.size()) {
    m_numZSlices = m_maxCoordinates[m_volume->getDimensionOrder(Dimensions::Z)] + 1;
```
Calls `getDimensionOrder()` twice; second call not guaranteed to return same value.

### 3.7 Exception not properly joined in multithreaded converter

**File:** `src/slideio/converter/tiffconverter.cpp:817-835`

```cpp
for (auto& r : readers) { r.join(); }
for (auto& e : encoders) { e.join(); }
```
If `r.join()` throws, remaining readers and all encoders are never joined. Needs RAII or try-finally.

### 3.8 Missing const on Scene accessor methods

**File:** `src/slideio/slideio/scene.cpp:62, 68`

`getNumZSlices()` and `getNumTFrames()` modify no state but are not marked `const`.

### 3.9 Swallowed error in UTF-8 conversion

**File:** `src/slideio/core/tools/tools.cpp:141-160`

Returns empty `wstring` for most `MultiByteToWideChar` errors instead of throwing.

---

## 4. Low-Severity Bugs

### 4.1 Unused variable in CVScene::readBlockChannels

**File:** `src/slideio/core/cvscene.cpp:30, 61`

```cpp
const cv::Rect rectScene = blockRect;  // assigned but never used
```

### 4.2 Typo in error message

**File:** `src/slideio/core/tools/tools.cpp:151`

```
"Unrecognized UTF-8 charachters"  ->  "characters"
```

### 4.3 CVScene::getChannelName ignores channel parameter

**File:** `src/slideio/core/cvscene.cpp:15`

```cpp
std::string CVScene::getChannelName(int) const { return ""; }
```
Parameter unnamed and ignored. No bounds check.

---

## 5. Architecture

### 5.1 Hardcoded driver registration (Open/Closed Principle violation)

**File:** `src/slideio/slideio/imagedrivermanager.cpp:8-18, 58-68, 76-125`

ImageDriverManager directly `#include`s all 11 driver headers and instantiates them in `initialize()`. Adding a new driver requires modifying three places in this file.

```cpp
std::string driverOrder[] = { "OMETIFF", "SVS", "CZI", ... "GDAL" };
```

**Recommendation:** Use a factory registration pattern where each driver self-registers via a static initializer or a registration macro:
```cpp
REGISTER_DRIVER(SVSImageDriver);  // in svs driver compilation unit
```

### 5.2 Bloated CVScene interface (47 methods, 1 pure virtual)

**File:** `src/slideio/core/cvscene.hpp`

CVScene has ~47 methods but only `readResampledBlockChannelsEx()` is pure virtual. The other 46 have default implementations. Derived classes must understand which to override.

**Recommendation:** Split into a minimal abstract interface (4-5 methods) and a utility base class with default implementations (CRTP or mixin pattern).

### 5.3 Inconsistent public API types

**File:** `src/slideio/slideio/scene.hpp` vs `src/slideio/core/cvscene.hpp`

- Public Scene API uses `std::tuple<int,int,int,int>` for rectangles
- Internal CVScene uses `cv::Rect`
- Public Scene uses `void* buffer` + `size_t bufferSize` for reads
- Internal CVScene uses `cv::OutputArray`

Users must remember tuple index semantics with no type safety. The dual API (buffer vs Mat) adds confusion about which is canonical.

**Recommendation:** Use named structs (`Rect`, `Size`) in the public API instead of tuples.

### 5.4 All drivers loaded eagerly

**File:** `src/slideio/slideio/imagedrivermanager.cpp:71-125`

All 11 drivers are instantiated on first `initialize()` call, even if only one format is needed. For quick single-file reads, this is wasteful.

**Recommendation:** Lazy-load drivers: register driver factories and instantiate only when `canOpenFile()` is first called for each.

### 5.5 Sequential driver probing for file format detection

**File:** `src/slideio/slideio/imagedrivermanager.cpp:58-68`

```cpp
for (const auto& driverID : driverOrder) {
    if(driver->canOpenFile(filePath)) { return driver; }
}
```

Tries up to 11 drivers sequentially. For an unrecognized file, all 11 are probed. Some drivers (GDAL) open the file in `canOpenFile()`.

**Recommendation:** Use file extension as first-pass filter before calling `canOpenFile()`.

### 5.6 No typed exception hierarchy

**File:** `src/slideio/base/exceptions.hpp`

Only `RuntimeError` exists. Cannot distinguish file-not-found from format-error from I/O-error programmatically:
```cpp
catch (slideio::RuntimeError& e) {
    // Is this a format error? Permission denied? Corrupt file? No way to tell.
}
```

**Recommendation:** Add `FileNotFoundException`, `FormatException`, `IOException` subclasses.

### 5.7 Threading concerns mixed into converter logic

**File:** `src/slideio/converter/tiffconverter.hpp`

TiffConverter directly manages thread pools, atomics, bounded queues, and thread statistics. This mixes I/O logic with concurrency.

**Recommendation:** Extract threading into a `ThreadedPipeline` or `ParallelConverter` wrapper. Keep TiffConverter single-threaded.

### 5.8 Raw pointer from Scene to Slide in CZI driver

**File:** `src/slideio/drivers/czi/cziscene.hpp:139`

```cpp
CZISlide* m_slide;  // raw pointer, no ownership semantics
```
If `CZISlide` is destroyed before `CZIScene`, this becomes a dangling pointer. Other drivers use similar patterns.

**Recommendation:** Use `std::weak_ptr<CZISlide>` or ensure lifetime guarantee via `shared_ptr`.

### 5.9 Manual reference counting alongside shared_ptr

**File:** `src/slideio/core/refcounter.hpp`

Custom `RefCounter` with atomic operations and `RefCounterGuard` RAII wrapper exists alongside `std::shared_ptr` usage elsewhere. This is redundant and error-prone.

**Recommendation:** Replace with `std::shared_ptr` / `std::weak_ptr`.

### 5.10 BoundedQueue is not actually bounded

**File:** `src/slideio/core/tools/boundedqueue.hpp:49`

```cpp
std::queue<T> queue_;  // unbounded internal queue
```
The condition variable blocks producers when queue is "full", but there is no hard limit on queue memory. A slow consumer with fast producer can cause memory exhaustion.

---

## 6. Performance

### 6.1 Unnecessary vector copy in CVScene

**File:** `src/slideio/core/cvscene.cpp:79`

```cpp
std::vector<int> channelIndices(channelIndicesIn);  // full copy
if(channelIndices.empty()) { /* fill it */ }
```
Copies the entire vector unconditionally. Should use `const&` and copy only when modification needed.

### 6.2 Per-tile memory allocation in readRegularTile

**File:** `src/slideio/imagetools/tifftools.cpp:779-786`

Creates `std::vector<cv::Mat> channelRasters` and resizes it on every tile read call. For a tiled image with thousands of tiles, this causes thousands of allocations.

**Recommendation:** Pre-allocate outside the tile loop or use a fixed-size array for common cases (1-4 channels).

### 6.3 Repeated bounds checking in NDPI driver

**File:** `src/slideio/drivers/ndpi/ndpiscene.cpp:114-152`

Multiple methods (`getNumChannels`, `getChannelDataType`, `getResolution`, `getMagnification`) each repeat:
```cpp
const std::vector<NDPITiffDirectory>& directories = m_pfile->directories();
if (m_startDir < 0 || m_startDir >= directories.size()) { RAISE_RUNTIME_ERROR... }
const auto& dir = directories[m_startDir];
```

**Recommendation:** Cache the directory reference as a member.

### 6.4 Unnecessary matrix copies in TiffConverter

**File:** `src/slideio/converter/tiffconverter.cpp:590-592`

```cpp
block(tileRect).copyTo(tileInfo.raster);
```
Creates a copy of a sub-region for each tile. Consider using ROI references (cv::Mat headers are lightweight) where possible.

### 6.5 Excessive temporary matrices in transformer chain

**File:** `src/slideio/transformer/transformerscene.cpp:112-120`

```cpp
for (const auto& transformation : m_transformations) {
    cv::Mat targetBlock;  // allocated every iteration
    transformationEx->applyTransformation(sourceBlock, targetBlock);
    targetBlock.copyTo(sourceBlock);  // copy back
}
```
For a chain of N filters, this allocates N matrices and performs N copies. Use double-buffering (two pre-allocated mats, swap pointers).

### 6.6 Unnecessary vector copy in TransformationEx

**File:** `src/slideio/transformer/transformationex.cpp:13-17`

```cpp
std::vector<DataType> TransformationEx::computeChannelDataTypes(const std::vector<DataType>& channels) const {
    std::vector<DataType> copy(channels);  // pointless copy
    return copy;
}
```

### 6.7 O(n) channel attribute lookup

**File:** `src/slideio/core/cvscene.cpp:250-257`

```cpp
const auto it = std::find(m_channelAttributeNames.begin(), m_channelAttributeNames.end(), attributeName);
```
Linear search through vector. Use `std::unordered_map` for O(1) lookup if this is called frequently.

### 6.8 INFO-level logging on every getter call

**File:** `src/slideio/slideio/scene.cpp` (lines 38, 44, 50, etc.)

```cpp
SLIDEIO_LOG(INFO) << "Scene::getFilePath";
```
Every Scene getter logs at INFO. In hot paths this adds significant overhead.

**Recommendation:** Remove or change to VLOG/DEBUG level.

---

## 7. Memory Management

### 7.1 CZI driver preloads all zoom level tile data

**File:** `src/slideio/drivers/czi/cziscene.hpp:40-46`

```cpp
struct ZoomLevel {
    CZISubBlocks blocks;  // ALL blocks for this zoom level
    Tiles tiles;           // ALL tiles precomputed
};
std::vector<ZoomLevel> m_zoomLevels;
```
For a large multi-resolution image, all zoom levels' tile metadata is loaded into memory at open time. A typical whole-slide image can have millions of tiles across all zoom levels.

**Recommendation:** Lazy-load tile metadata per zoom level on demand.

### 7.2 Unbounded block queue in converter

**File:** `src/slideio/converter/tiffconverter.hpp:149-151`

```cpp
std::queue<Block> blockQueue;  // no size limit
```
For large image conversions, this queue can grow unbounded. Each Block contains a `cv::Mat` (potentially megabytes).

### 7.3 ETS file vector resize from untrusted data

**File:** `src/slideio/drivers/vsi/etsfile.cpp:109-110`

```cpp
tiles->resize(header.numUsedChunks);
m_maxCoordinates.resize(m_numDimensions);
```
Values come directly from file data. A malicious file with `numUsedChunks = 2^31` causes attempted allocation of gigabytes, leading to OOM.

**Recommendation:** Validate sizes against reasonable maximums before resizing.

### 7.4 TempFile partial construction risk

**File:** `src/slideio/core/tools/tempfile.hpp:23-34`

If `unique_path()` throws after temp directory is determined but before `m_path` is fully assigned, the file tracking state is inconsistent.

---

## 8. Thread Safety

### 8.1 Undocumented mutex in CVScene

**File:** `src/slideio/core/cvscene.hpp:227`

```cpp
mutable std::mutex m_readBlockMutex;
```
No documentation of what this mutex protects, which methods are thread-safe, or what the threading model is.

### 8.2 TiffConverter copy constructor copies atomics unsafely

**File:** `src/slideio/converter/tiffconverter.hpp:29-43`

The copy constructor reads `m_readersIdleTimeNs.load()` etc. If the object is copied while a conversion is in progress, this is a data race.

**Recommendation:** Make TiffConverter non-copyable (`= delete`).

### 8.3 Inconsistent memory ordering in TiffConverter

**File:** `src/slideio/converter/tiffconverter.cpp:626-628`

```cpp
m_readersIdleTimeNs.fetch_add(localIdleNs, std::memory_order_relaxed);
// ...
if (--activeReaders == 0)  // default: seq_cst
    inputQueue.setDone();
```
Mixed ordering can cause visibility issues on ARM/weak-ordering architectures.

### 8.4 Race condition in VSIStream::getSize

**File:** `src/slideio/drivers/vsi/vsistream.cpp:50-54`

```cpp
if(m_size < 0) {
    // seek to end, get size, seek back
    m_size = m_stream->tellg();
}
```
Not thread-safe. Multiple threads could compute size concurrently, corrupting stream position.

### 8.5 Lock scope in CVScene 4D reads

**File:** `src/slideio/core/cvscene.cpp:148-150`

```cpp
if (planeMatrix) {
    std::lock_guard<std::mutex> lock(m_readBlockMutex);
    readResampledBlockChannelsEx(..., dataRaster);
}
```
Lock is acquired inside a loop per z-slice/time-frame. Multiple threads reading different planes can interleave writes to the same `dataRaster`.

---

## 9. Security (Input Validation)

### 9.1 Unchecked vector access from file data in CZI driver

**Files:** `src/slideio/drivers/czi/cziscene.cpp:391, 446`

```cpp
const int channel = m_componentToChannelIndex[component].first;
```
`component` index from file data is not bounds-checked against map size.

### 9.2 Format string risk in NDPI driver

**File:** `src/slideio/drivers/ndpi/ndpitiffmessagehandler.cpp:12-33`

Custom `vasprintf` implementation. If format string comes from file metadata, this is a format string vulnerability.

### 9.3 Unchecked stoi in SVS metadata parsing

**File:** `src/slideio/drivers/svs/svstools.cpp:13-30`

```cpp
magn = std::stoi(magn_str);  // no try-catch
```
If regex captures a non-numeric string, `stoi` throws an unhandled `std::invalid_argument`.

### 9.4 Missing source buffer length validation in 12-to-16 bit conversion

**File:** `src/slideio/core/tools/tools.cpp:96-116`

```cpp
void Tools::convert12BitsTo16Bits(const uint8_t* source, uint16_t* target, int targetLen)
```
Validates `source != nullptr` and `targetLen > 0` but doesn't verify that source has enough bytes (needs `targetLen * 3 / 2` bytes).

---

## 10. Build System

### 10.1 CMAKE_CXX_STANDARD syntax error

**File:** `CMakeLists.txt:36-37`

```cmake
set(CMAKE_CXX_STANDARD_REQUIRED, ON)   # comma makes this a list, not a bool
set(CMAKE_CXX_STANDARD,17)             # same issue
```
These set variables named `CMAKE_CXX_STANDARD_REQUIRED,` (with comma) and `CMAKE_CXX_STANDARD,17` (concatenated). C++17 standard is **not actually enforced**.

**Fix:**
```cmake
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_STANDARD 17)
```

### 10.2 No compiler warnings enabled

**File:** `CMakeLists.txt:15-18`

Only `-Wno-switch` and `-Wno-switch-default` are set. No `-Wall`, `-Wextra`, or `-Werror`.

**Recommendation:** Add at minimum:
```cmake
add_compile_options(-Wall -Wextra -Wpedantic)
```

### 10.3 No sanitizer support

No ASAN/UBSAN/TSAN configuration anywhere in the build system. Add an option:
```cmake
option(SLIDEIO_ENABLE_SANITIZERS "Enable ASan/UBSan" OFF)
```

### 10.4 Unconditional symbol stripping in Release

**File:** `CMakeLists.txt:21-25`

```cmake
set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -s")
```
Makes crash dump debugging impossible. Should be a separate `RelWithDebInfo` concern or optional.

### 10.5 Missing SOVERSION on shared libraries

No `set_target_properties(... PROPERTIES SOVERSION ...)` on any library. ABI compatibility cannot be tracked.

### 10.6 find_package without REQUIRED

**Files:** Multiple CMakeLists.txt

```cmake
find_package(SQLite3)   # no REQUIRED
find_package(glog)      # no REQUIRED
find_package(OpenCV)    # no REQUIRED
```
Silently continues if package not found, causing cryptic link errors later.

### 10.7 Duplicate CMAKE_PREFIX_PATH in every subdirectory

Every subdirectory CMakeLists.txt repeats:
```cmake
set(CMAKE_PREFIX_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
set(CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
```
Should be set once in root.

### 10.8 No GNUInstallDirs support

Hard-coded `"lib"`, `"bin"`, `"include"` paths. Makes packaging (RPM, DEB) difficult.

### 10.9 Duplicate install() blocks

**File:** `CMakeLists.txt:154-204`

Same libraries installed twice: once to `lib`, again to `bin` for non-Windows. Confusing and error-prone.

### 10.10 Libraries and executables in same output directory

**File:** `CMakeLists.txt:121-127`

```cmake
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
```

### 10.11 -static-libstdc++ applied to all UNIX, not just Linux

**File:** `CMakeLists.txt:111`

```cmake
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -static-libstdc++")
```
Applies to macOS and BSD too, where it may not be appropriate.

### 10.12 Duplicate function in install.py

**File:** `install.py:237-252 and 255-270`

`install_slideio()` is defined twice with identical code.

### 10.13 Deprecated platform detection in install.py

**File:** `install.py:44-55`

Uses `sys.platform` with entries like `'linux2'` which is deprecated since Python 3.3.

---

## 11. Testing

### 11.1 No CI test execution

**Files:** `.github/workflows/`

Only two workflows exist: Docker image build and website deployment. **No workflow runs tests.** No build validation on pull requests.

### 11.2 No code coverage

No gcov/lcov integration. Cannot measure test effectiveness.

### 11.3 Memory/performance tests disabled

**File:** `src/CMakeLists.txt:4`

```cmake
#add_subdirectory(single_tests)
```
Memory leak and performance test suites are commented out.

### 11.4 No test timeouts

No `TIMEOUT` property set on any test. Hung tests block CI indefinitely.

### 11.5 Hardcoded test runner script

**File:** `run-tests-linux.sh`

Contains a static list of 8 test binaries. Adding new test suites requires manual update. No error handling if a binary is missing.

### 11.6 No parallel test execution

Tests run sequentially. No `ctest -j` support configured.

### 11.7 Missing CTest configuration

`enable_testing()` is called but no CTest setup, no XML output, no memory limits.

### 11.8 Test executables use hardcoded output path

```cmake
add_test(${TEST_NAME} ${TEST_DIR}/${TEST_NAME})
```
Should use `$<TARGET_FILE:${TEST_NAME}>` generator expression.

### 11.9 No mock/fixture infrastructure

Test library (`src/tests/testlib/`) has only 4 basic utilities. No mock objects for drivers, no test fixtures for common setup.

---

## 12. CI/CD & Docker

### 12.1 Version mismatch across artifacts

| Source | Version |
|--------|---------|
| CMakeLists.txt | 2.8.0 |
| CI workflow env | 2.8.0 |
| manylinux Dockerfile | clones v2.7.4 |
| debian-arm Dockerfile | clones v2.8.0 |

### 12.2 Secrets passed as Docker build args

**File:** `.github/workflows/docker-manylinux_2_28_x86_64.yml:23-25`

```yaml
--build-arg CONAN_LOGIN_USERNAME=${{ secrets.CONAN_USER }}
--build-arg CONAN_PASSWORD=${{ secrets.CONAN_PASSWORD }}
```
Build args are visible in `docker history`. Use BuildKit secrets instead.

### 12.3 Unpinned base images

**File:** `docker/debian/Dockerfile:1`

```dockerfile
FROM ubuntu:latest AS builder
```
Unpinned tag. Builds are not reproducible.

### 12.4 Manual-only workflow dispatch

Docker build workflow is `workflow_dispatch` only. No automatic builds on release/tag.

### 12.5 No image security scanning

No Trivy, Snyk, or similar scanner in CI.

### 12.6 Conan dependency management issues

- `force=True` used in multiple driver conanfiles without documentation
- No version upper bounds on dependencies
- Private Conan server URL hardcoded in Dockerfiles and install.py
- Root `conanfile.txt` has empty `[requires]` section -- no single source of truth

---

## 13. Maintainability

### 13.1 Pervasive boilerplate in transformer filters

Every filter class (7 total) repeats identical copy/move constructor and assignment operator patterns (~40 lines each). Each wrapper class duplicates getter/setter delegation.

**Files:** `src/slideio/transformer/gaussianblurfilter.hpp`, `medianblurfilter.hpp`, `bilateralfilter.hpp`, `cannyfilter.hpp`, `laplacianfilter.hpp`, `sobelfilter.hpp`, `scharrfilter.hpp` and their wrapper counterparts.

**Recommendation:** Use CRTP base template or macro to eliminate ~300 lines of duplicated code.

### 13.2 Dual switch statements for transformation dispatch

**File:** `src/slideio/transformer/transformations.cpp:21-73, 76-128`

Two nearly identical 8-case switch statements using unsafe C-style casts:
```cpp
GaussianBlurFilter& filter = (GaussianBlurFilter&)source;
```
Adding a new filter requires updating both switches.

**Recommendation:** Use virtual clone/copy methods instead of type-switch dispatch.

### 13.3 No parameter validation in filter setters

All filter setters accept any value:
```cpp
void setKernelSize(int kernelSize) { m_kernelSize = kernelSize; }  // negative? zero? even?
```
OpenCV will throw cryptic errors at apply time. Validate early.

### 13.4 Inconsistent smart pointer initialization in ImageDriverManager

**File:** `src/slideio/slideio/imagedrivermanager.cpp:78-123`

Three different patterns in 40 lines:
```cpp
std::shared_ptr<ImageDriver> driver { std::make_shared<OTImageDriver>() };   // braces
SVSImageDriver* driver = new SVSImageDriver;                                  // raw new
std::shared_ptr<ImageDriver> driver(new CZIImageDriver);                     // parens
```

### 13.5 Excessive logging in constructors/destructors

**Files:** `src/slideio/slideio/slide.cpp:12-17`, `scene.cpp` throughout

INFO-level logging on every object creation, destruction, and getter call creates noise in production.

### 13.6 Code duplication across CVScene read methods

**File:** `src/slideio/core/cvscene.cpp`

Four method families (readBlock, readResampledBlock, read4DBlock, readResampled4DBlock) each in two variants (with/without channelIndices), all following the same pattern. Could be consolidated with templates or a single parameterized method.

### 13.7 Exception safety gaps in channel attribute management

**File:** `src/slideio/core/cvscene.cpp:232-276`

`defineChannelAttribute()` modifies `m_channelAttributeNames` and `m_channelAttributes` non-atomically. If an exception occurs between modifications, the two vectors become misaligned.

---

## Priority Summary

| Priority | Category | Count | Key Actions |
|----------|----------|-------|-------------|
| **P0 - Fix Now** | Critical bugs | 6 | Fix UB in exceptions, wrong matrix copy, dead TIFF code, bounds check |
| **P0 - Fix Now** | Build system | 1 | Fix CMAKE_CXX_STANDARD syntax (C++17 not enforced!) |
| **P1 - Next Sprint** | High-severity bugs | 8 | Null checks, bounds checks, resource leaks |
| **P1 - Next Sprint** | Security | 4 | Input validation for file parsing |
| **P1 - Next Sprint** | CI/CD | 3 | Add test workflows, fix version mismatches, fix Docker secrets |
| **P2 - Plan** | Architecture | 10 | Driver registration, CVScene interface, typed exceptions |
| **P2 - Plan** | Thread safety | 5 | Document model, fix races, make TiffConverter non-copyable |
| **P2 - Plan** | Build system | 11 | Warnings, sanitizers, find_package REQUIRED |
| **P3 - Backlog** | Performance | 8 | Avoid copies, lazy loading, caching |
| **P3 - Backlog** | Testing | 9 | Coverage, CTest, parallel execution, mocks |
| **P3 - Backlog** | Maintainability | 7 | Reduce duplication, validate params, consistent patterns |
