# Converter S3 Input Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Let `slideio_converter` (CLI) and the converter library accept `s3://`, `http://`, `https://` URIs as input, using the HTTP streaming foundation shipped in S3 v1.

**Architecture:** Two existence gates in the CLI tool reject non-path URIs today — one in CLI11 (`->check(CLI::ExistingFile)` in `main.cpp`) and one in `convertFile` (`std::filesystem::exists` in `sceneconverter.cpp`). Replace both with URI-aware checks reusing `detectUriScheme` from `slideio/imagetools/uridispatcher.hpp`. Everything downstream (`openSlide`, `TiffConverter::cloneScene` reopen) already supports URI inputs.

**Tech Stack:** C++17, libtiff, libcurl (already linked), GoogleTest, CMake, Conan v2. Python 3 (already wired in for the existing `HttpFixture`).

**Reference spec:** `software-docs/specs/2026-05-28-converter-s3-input-design.md`

**Branch:** `s3`

---

## File Structure

### Modified files

| Path | Why |
|---|---|
| `src/tools/converter/sceneconverter.cpp` | URI-aware input-existence gate; optional info-line for remote inputs |
| `src/tools/converter/main.cpp` | Replace `CLI::ExistingFile` validator with one that accepts URIs; help-text update |
| `src/tests/main/CMakeLists.txt` | Add `test_converter_s3.cpp` to sources; add `src/tools/converter/sceneconverter.cpp` to sources; link `${CONVERTER_LIB_NAME}` |

### New files

| Path | Responsibility |
|---|---|
| `src/tests/main/test_converter_s3.cpp` | End-to-end converter tests over the local HTTP fixture (single- and multi-threaded reader paths) |

**No library/header changes. No driver changes. No public-API changes.**

### Rationale for placing the test in `slideio_tests`

The existing `HttpFixture` (Python child process + libcurl control channel) and its CMake plumbing (`Python3` find, `SLIDEIO_TEST_PYTHON`, `SLIDEIO_TEST_HTTP_SERVER` compile defines, `CURL::libcurl` link) live in `src/tests/main/` and the `slideio_tests` executable. Duplicating that plumbing into `slideio_converter_tests` would diverge two copies of an already-subtle fixture. Adding `${CONVERTER_LIB_NAME}` and `sceneconverter.cpp` to `slideio_tests` is the smaller, lower-risk change. The test verifies the converter end-to-end; module boundaries are not damaged because `slideio_tests` is already a cross-module integration suite (it links every driver).

---

## Coding conventions (apply to every task)

- Match existing file conventions (license header, `#pragma once` or `OPENCV_*_HPP` guard — copy from the file's neighbor).
- No emojis or decorative comments. Comments only for non-obvious *why*.
- Test names follow the existing `S3StreamingIntegration` pattern (`TEST(ConverterS3, <CaseName>)`).
- One commit per task; commit message subject under ~70 chars.

## Build & test commands (used throughout)

Windows PowerShell:

```powershell
# Incremental build only (after configure):
python install.py -a build-only -c release

# Run the new test subset:
.\build\release\bin\slideio_tests.exe --gtest_filter="ConverterS3*"

# Run all main tests:
.\build\release\bin\slideio_tests.exe
```

Linux/macOS:

```bash
python3 install.py -a build-only -c release
./build/release/bin/slideio_tests --gtest_filter="ConverterS3*"
```

---

## Task 1: Wire the failing single-threaded integration test

**Files:**
- Create: `src/tests/main/test_converter_s3.cpp`
- Modify: `src/tests/main/CMakeLists.txt`

This task adds the test scaffold and the CMake plumbing so the test compiles and runs. The test is *expected to fail* at this point — it will fail because the CLI gate in `convertFile` rejects the HTTP URI. The next two tasks make it pass.

- [ ] **Step 1: Create the new test file.**

```cpp
// src/tests/main/test_converter_s3.cpp
// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
//
// End-to-end converter tests with input read over HTTP. Exercises the v1 S3
// streaming foundation through slideio_converter's convertFile entry point
// (the same function the CLI calls), with the local HttpFixture serving a
// small SVS test slide. One single-threaded case here; the multi-threaded
// case (cloneScene reopen over HTTP) is added in a later task.

#include "tests/testlib/testtools.hpp"
#include "tools/converter/sceneconverter.hpp"
#include "http_fixture/http_fixture.hpp"
#include "slideio/slideio/imagedrivermanager.hpp"
#include "slideio/core/cvslide.hpp"
#include "slideio/core/cvscene.hpp"
#include "slideio/base/rect.hpp"
#include "slideio/base/range.hpp"

#include <gtest/gtest.h>
#include <filesystem>
#include <opencv2/imgproc.hpp>

namespace {

// Copies a test image into a clean temp directory that HttpFixture can serve.
// The destination filename can be sanitized to avoid percent-encoding issues
// in the served URL.
std::filesystem::path stageImage(const std::string& srcPath,
                                 const std::string& tag,
                                 const std::string& destName)
{
    namespace fs = std::filesystem;
    fs::path root = fs::temp_directory_path() / ("slideio_conv_s3_" + tag);
    fs::remove_all(root);
    fs::create_directories(root);
    fs::copy_file(srcPath, root / destName, fs::copy_options::overwrite_existing);
    return root;
}

// Build a clean output path under a fresh temp subdirectory and ensure no
// stale output remains. Returns the path; caller is responsible for cleanup
// if needed (test infrastructure tears down the parent on next run).
std::filesystem::path freshOutputPath(const std::string& tag, const std::string& name)
{
    namespace fs = std::filesystem;
    fs::path out = fs::temp_directory_path() / ("slideio_conv_s3_out_" + tag);
    fs::remove_all(out);
    fs::create_directories(out);
    return out / name;
}

// Convert with the converter tool's convertFile() helper. All optional flags
// defaulted to the values that match a typical slideio_converter invocation;
// only the input, output, and thread counts vary per test.
void runConvertFile(const std::string& inputPath,
                    const std::string& outputPath,
                    int numReadingThreads,
                    int numEncodingThreads)
{
    slideio::Rect emptyRect;
    slideio::Range emptyChannelRange;
    slideio::Range emptySliceRange;
    slideio::Range emptyFrameRange;
    convertFile(
        inputPath,
        outputPath,
        /*sceneIndex=*/0,
        /*compressionRate=*/5.0,
        /*tileSize=*/512,
        /*numZoomLevels=*/-1,
        /*inputDriver=*/"AUTO",
        /*targetFormat=*/"SVS",
        /*targetCompression=*/"Jpeg",
        /*compressionQuality=*/85,
        emptyRect,
        emptyChannelRange,
        emptySliceRange,
        emptyFrameRange,
        /*silent=*/true,
        /*infoOnly=*/false,
        /*deleteIfExists=*/true,
        /*tileBatchSize=*/10,
        numReadingThreads,
        numEncodingThreads);
}

// Compare two converted slides: dimensions, channels, and a sampled
// top-left tile read of equal bytes. We do *not* byte-compare the SVS files
// because the SVS image-description tag embeds a UUID and timestamps.
void expectConvertedSlidesMatch(const std::string& pathA,
                                const std::string& pathB)
{
    auto slideA = slideio::ImageDriverManager::openSlide(pathA, "SVS");
    auto slideB = slideio::ImageDriverManager::openSlide(pathB, "SVS");
    ASSERT_TRUE(slideA);
    ASSERT_TRUE(slideB);
    ASSERT_EQ(slideA->getNumScenes(), slideB->getNumScenes());
    ASSERT_GT(slideA->getNumScenes(), 0);
    auto sceneA = slideA->getScene(0);
    auto sceneB = slideB->getScene(0);
    ASSERT_TRUE(sceneA);
    ASSERT_TRUE(sceneB);
    EXPECT_EQ(sceneA->getRect(), sceneB->getRect());
    EXPECT_EQ(sceneA->getNumChannels(), sceneB->getNumChannels());

    const cv::Rect rect = sceneA->getRect();
    cv::Rect block(0, 0, std::min(256, rect.width), std::min(256, rect.height));
    cv::Mat rasterA, rasterB;
    sceneA->readBlock(block, rasterA);
    sceneB->readBlock(block, rasterB);
    ASSERT_EQ(rasterA.size(), rasterB.size());
    EXPECT_EQ(0.0, cv::norm(rasterA, rasterB, cv::NORM_INF));
}

} // namespace

// Test 1: single-threaded converter over HTTP input.
// Stages CMU-1-Small-Region.svs to a temp dir, serves it over the local
// HttpFixture, converts it to a local SVS via convertFile, then converts the
// same slide from the local path, and checks the two outputs match.
TEST(ConverterS3, SingleThreaded_HttpInput_MatchesLocal)
{
    namespace fs = std::filesystem;
    const std::string name = "CMU-1-Small-Region.svs";
    const std::string localSrc = TestTools::getTestImagePath("svs", name);
    fs::path root = stageImage(localSrc, "st", name);

    slideio::tests::HttpFixture fx(root);
    const std::string httpUrl = fx.url(name);

    fs::path outFromHttp = freshOutputPath("st_http", "out.svs");
    fs::path outFromLocal = freshOutputPath("st_local", "out.svs");

    ASSERT_NO_THROW(runConvertFile(httpUrl, outFromHttp.generic_string(), 1, 1));
    ASSERT_NO_THROW(runConvertFile((root / name).generic_string(), outFromLocal.generic_string(), 1, 1));

    expectConvertedSlidesMatch(outFromHttp.generic_string(), outFromLocal.generic_string());
}
```

- [ ] **Step 2: Add the test source and a few other knobs to `src/tests/main/CMakeLists.txt`.**

Edit the file. The `TEST_SOURCES` list and `target_link_libraries` need additions. Replace the existing `TEST_SOURCES` block with:

```cmake
set(TEST_SOURCES
  test_color_tools.cpp
  test_imagedrivermanager.cpp
  test_czi_driver.cpp
  test_czi_tools.cpp
  test_scn_driver.cpp
  test_gdal_driver.cpp
  test_imagetools.cpp
  test_afi_driver.cpp
  test_svs_driver.cpp
  test_zvi_driver.cpp
  test_svs_tools.cpp
  test_dcm_driver.cpp
  test_tifftools.cpp
  test_zviutils.cpp
  test_tilecomposer.cpp
  test_cvtools.cpp
  test_dcmfile.cpp
  test_exception.cpp
  test_generic.cpp
  test_vsi_driver.cpp
  test_blocktiler.cpp
  test_tools.cpp
  test_similaritytools.cpp
  test_endian.cpp
  test_dimensions.cpp
  test_metadata.cpp
  test_metadata_builder.cpp
  test_tifffiles.cpp
  test_fiwrapper.cpp
  test_channel_attributes.cpp
  test_boundedqueue.cpp
  test_random_access_stream_contract.cpp
  test_filestream.cpp
  test_blockcache.cpp
  test_httpstream.cpp
  test_tiff_client_adapter.cpp
  test_uri_dispatcher.cpp
  test_s3_streaming_integration.cpp
  test_converter_s3.cpp
  http_fixture/http_fixture.cpp
  ${CMAKE_SOURCE_DIR}/src/tools/converter/sceneconverter.cpp
)
```

Then, in the same file, edit the final `target_link_libraries(${TEST_NAME} ...)` line that currently reads:

```cmake
target_link_libraries(${TEST_NAME} ${SLIDEIO_LIB_NAME} ${BASE_LIB_NAME} ${TEST_LIB_NAME})
```

Replace with:

```cmake
target_link_libraries(${TEST_NAME} ${SLIDEIO_LIB_NAME} ${BASE_LIB_NAME} ${TEST_LIB_NAME} ${CONVERTER_LIB_NAME})
```

- [ ] **Step 3: Build the test executable.**

Windows:

```powershell
python install.py -a build-only -c release
```

Linux/macOS:

```bash
python3 install.py -a build-only -c release
```

Expected: build succeeds. If the build fails because `sceneconverter.cpp` references symbols (e.g., from `slideio::converter`) that aren't transitively visible, confirm `${CONVERTER_LIB_NAME}` is on the link line. If the build fails because `tools/converter/sceneconverter.hpp` isn't found, confirm `target_include_directories(${TEST_NAME} PRIVATE ${INCLUDE_ROOT})` is in the file (it already is — `INCLUDE_ROOT = ${CMAKE_SOURCE_DIR}/src`). No CMake changes beyond the two above should be necessary.

- [ ] **Step 4: Run the new test — expect FAIL.**

```powershell
.\build\release\bin\slideio_tests.exe --gtest_filter="ConverterS3.SingleThreaded_HttpInput_MatchesLocal"
```

Expected: the test FAILS in the first `runConvertFile` call (the HTTP one) with a `std::runtime_error` containing `"Input file does not exist: http://127.0.0.1:..."`. That's the existing CLI gate in `sceneconverter.cpp:213` rejecting the URI. The failure confirms the test exercises the exact code path we're about to fix.

- [ ] **Step 5: Commit.**

```powershell
git add src/tests/main/test_converter_s3.cpp src/tests/main/CMakeLists.txt
git commit -m "test: failing converter S3 integration test (HTTP single-threaded)"
```

---

## Task 2: URI-aware input-existence gate in `convertFile`

**Files:**
- Modify: `src/tools/converter/sceneconverter.cpp` (lines 1–25 for the new include; line 213 for the gate)

- [ ] **Step 1: Add the include.**

Open `src/tools/converter/sceneconverter.cpp`. The top of the file currently has these includes (lines 5–17):

```cpp
#include "slideio/converter/converterparameters.hpp"
#include "slideio/converter/tiffconverter.hpp"
#include "slideio/slideio/slide.hpp"
#include "slideio/base/rect.hpp"
#include "slideio/base/range.hpp"
#include "slideio/core/cvscene.hpp"
#include "slideio/slideio/slideio.hpp"
#include <iostream>
#include <filesystem>
#include <sstream>
#include <iomanip>
#include <chrono>
#include <csignal>
```

Add one new include directly after the existing `slideio/...` includes (preserving the group):

```cpp
#include "slideio/imagetools/uridispatcher.hpp"
```

- [ ] **Step 2: Wrap the existence check.**

Currently, `convertFile` starts (lines 213–215):

```cpp
	if (!std::filesystem::exists(inputPath)) {
		throw std::runtime_error("Input file does not exist: " + inputPath);
	}
```

Replace those three lines with:

```cpp
	if (slideio::detectUriScheme(inputPath) == slideio::UriScheme::LocalFile) {
		if (!std::filesystem::exists(inputPath)) {
			throw std::runtime_error("Input file does not exist: " + inputPath);
		}
	}
	// For s3:// and http(s):// URIs we skip the local-file existence check.
	// openSlide() validates the resource via a HEAD probe and surfaces a
	// clear HTTP-level error (404, 403, etc.) if the URI is invalid.
```

- [ ] **Step 3: Build.**

```powershell
python install.py -a build-only -c release
```

Expected: build succeeds.

- [ ] **Step 4: Run the failing test from Task 1 — expect PASS.**

```powershell
.\build\release\bin\slideio_tests.exe --gtest_filter="ConverterS3.SingleThreaded_HttpInput_MatchesLocal"
```

Expected: the test PASSES. Both `runConvertFile` calls complete, both outputs open as SVS, and the sampled tile reads match byte-for-byte.

If the test fails: read the failure message. If it failed on the *second* `runConvertFile` (local), there is an unrelated regression — investigate. If it failed on the *first* (http) with an HTTP-level error (`HEAD failed: ...`), the `HttpFixture` may not have started; re-check `SLIDEIO_TEST_PYTHON`.

- [ ] **Step 5: Run the existing S3 streaming integration tests to confirm no regression.**

```powershell
.\build\release\bin\slideio_tests.exe --gtest_filter="S3StreamingIntegration*"
```

Expected: all four tests still pass.

- [ ] **Step 6: Commit.**

```powershell
git add src/tools/converter/sceneconverter.cpp
git commit -m "converter: accept s3/http(s) URIs in convertFile input gate"
```

---

## Task 3: Drop the CLI11 `ExistingFile` validator in `main.cpp`

The CLI11 parser in `main.cpp:92` has a *second* gate: `->check(CLI::ExistingFile)` rejects URIs at argv-parse time, before `convertFile` is called. Without removing this, the CLI binary cannot accept URI inputs even though the library can. Drop the check; rely on `convertFile`'s URI-aware gate (Task 2) and `openSlide`'s HTTP probe for validation.

**Files:**
- Modify: `src/tools/converter/main.cpp:90–92`

- [ ] **Step 1: Remove the `->check(CLI::ExistingFile)`.**

The current block is:

```cpp
    app.add_option("input", inputPath, "Input file path")
       ->required()
       ->check(CLI::ExistingFile);
```

Replace with:

```cpp
    app.add_option("input", inputPath,
                   "Input file path or remote URI (s3://, http://, https://)")
       ->required();
```

The help string changes too — the previous "Input file path" no longer captures the full accepted-input domain.

- [ ] **Step 2: Build.**

```powershell
python install.py -a build-only -c release
```

Expected: build succeeds.

- [ ] **Step 3: Smoke-test the CLI binary with `--help`.**

```powershell
.\build\release\bin\converter.exe --help
```

Expected: help output shows the new description line (`Input file path or remote URI (s3://, http://, https://)`) and no longer says `REQUIRED FILE` next to the input arg (CLI11 adds `FILE` only when `CLI::ExistingFile` is attached).

- [ ] **Step 4: Smoke-test that a nonexistent local path now fails *inside* `convertFile`, not at parse time.**

```powershell
.\build\release\bin\converter.exe ./does-not-exist.svs ./out.svs
```

Expected output (stderr): `Input file does not exist: ./does-not-exist.svs` (thrown from `convertFile`). The behavior is byte-identical for the user — same error message — only the location of the check moves.

- [ ] **Step 5: Commit.**

```powershell
git add src/tools/converter/main.cpp
git commit -m "converter CLI: drop ExistingFile validator, accept URI inputs"
```

---

## Task 4: Multi-threaded reader path over HTTP

**Files:**
- Modify: `src/tests/main/test_converter_s3.cpp` (add one new test case)

The MT converter clones the input slide once per reader thread via `TiffConverter::cloneScene()`, which calls `openSlide(m_scene->getFilePath(), driverId)`. For HTTP inputs, `getFilePath()` returns the URI (verified for SVS via `svsslide.cpp:151 + stream->uri()`). Each reader opens its own `HttpStream`. This test pins the behavior.

- [ ] **Step 1: Add the MT test case at the bottom of `src/tests/main/test_converter_s3.cpp`.**

Append this block after the existing `SingleThreaded_HttpInput_MatchesLocal` test:

```cpp

// Test 2: multi-threaded converter over HTTP input.
// Exercises TiffConverter::cloneScene() reopening the stream-opened slide
// once per reader thread. Each reader opens its own HttpStream + cache.
TEST(ConverterS3, MultiThreaded_HttpInput_MatchesLocal)
{
    namespace fs = std::filesystem;
    const std::string name = "CMU-1-Small-Region.svs";
    const std::string localSrc = TestTools::getTestImagePath("svs", name);
    fs::path root = stageImage(localSrc, "mt", name);

    slideio::tests::HttpFixture fx(root);
    const std::string httpUrl = fx.url(name);

    fs::path outFromHttp = freshOutputPath("mt_http", "out.svs");
    fs::path outFromLocal = freshOutputPath("mt_local", "out.svs");

    // 4 reader + 4 encoder threads forces TiffConverter into its MT path
    // (cloneScene() + per-reader HttpStream).
    ASSERT_NO_THROW(runConvertFile(httpUrl, outFromHttp.generic_string(), 4, 4));
    ASSERT_NO_THROW(runConvertFile((root / name).generic_string(), outFromLocal.generic_string(), 4, 4));

    expectConvertedSlidesMatch(outFromHttp.generic_string(), outFromLocal.generic_string());
}
```

- [ ] **Step 2: Build.**

```powershell
python install.py -a build-only -c release
```

Expected: build succeeds.

- [ ] **Step 3: Run the new MT test.**

```powershell
.\build\release\bin\slideio_tests.exe --gtest_filter="ConverterS3.MultiThreaded_HttpInput_MatchesLocal"
```

Expected: PASS. If it fails with `Driver SVS does not yet support remote URIs` or a similar dispatcher message, the `cloneScene()` reopen is sending a non-URI string — investigate `SVSScene::getFilePath()` for the stream-open path (it should be the URI passed at `openSlide` time).

- [ ] **Step 4: Run both ConverterS3 tests together as a final check.**

```powershell
.\build\release\bin\slideio_tests.exe --gtest_filter="ConverterS3*"
```

Expected: both PASS.

- [ ] **Step 5: Commit.**

```powershell
git add src/tests/main/test_converter_s3.cpp
git commit -m "test: multi-threaded converter over HTTP input"
```

---

## Task 5: Document the per-reader cache footprint in CLI info output

When the input is a remote URI and the user is converting on the MT path, peak memory ≈ `numReadingThreads × 256 MB` of stream cache. The existing `printInfo()` in `sceneconverter.cpp` already prints `Reading threads: N` — augment it with one line that calls out the cache footprint when the input is remote, so a user surprised by memory has a visible reason and a knob to reach for.

**Files:**
- Modify: `src/tools/converter/sceneconverter.cpp` (function `printInfo`, around lines 155–189)

- [ ] **Step 1: Plumb the input path into `printInfo`.**

Currently `printInfo(const TiffConverter& converter)` doesn't know the input path. The call site is in `convertFile` at roughly line 285 (`printInfo(converter);`). Change the signature to accept the input path and pass it through.

In `sceneconverter.cpp`, find the declaration:

```cpp
void printInfo(const TiffConverter& converter) {
```

Change to:

```cpp
void printInfo(const TiffConverter& converter, const std::string& inputPath) {
```

Inside `printInfo`, at the end of the function body (right before the closing `}`), add:

```cpp
	if (slideio::detectUriScheme(inputPath) != slideio::UriScheme::LocalFile) {
		const int rt = (numReadingThreads <= 0)
			? std::max(1, static_cast<int>(std::thread::hardware_concurrency()) / 2)
			: numReadingThreads;
		std::cout << "Remote input: per-reader HTTP block cache is 256 MB; "
			<< "peak ~= " << rt << " x 256 MB during conversion. "
			<< "Lower --reading-threads or call "
			<< "ImageDriverManager::setHttpCacheEnabled(false) to bound memory."
			<< std::endl;
	}
```

If `<thread>` is not already included near the top of `sceneconverter.cpp`, add `#include <thread>` to the existing `<chrono>`/`<csignal>` block.

- [ ] **Step 2: Update the call site.**

Find the line (around line 285):

```cpp
	if (infoOnly || !silent) {
		printInfo(converter);
	}
```

Change the call to:

```cpp
	if (infoOnly || !silent) {
		printInfo(converter, inputPath);
	}
```

- [ ] **Step 3: Build.**

```powershell
python install.py -a build-only -c release
```

Expected: build succeeds.

- [ ] **Step 4: Smoke-test the new info line.**

Start the existing HTTP fixture manually (or by piggybacking on a test), then run:

```powershell
.\build\release\bin\converter.exe http://127.0.0.1:<port>/CMU-1-Small-Region.svs ./out.svs -i
```

The `-i` flag enables `--info-only`. Expected: the info output ends with the new "Remote input: per-reader HTTP block cache..." line.

Alternative smoke (no fixture needed): use a deliberately wrong URL to short-circuit *after* `printInfo`:

```powershell
.\build\release\bin\converter.exe https://example.invalid/x.svs ./out.svs -i
```

The CLI will hit a HEAD probe failure inside `openSlide` *before* `printInfo`, so this alternative doesn't actually exercise the line — fall back to using the test fixture or hand-running a Python `http.server` for the smoke.

For the local-input regression case:

```powershell
.\build\release\bin\converter.exe (TestTools::getTestImagePath('svs', 'CMU-1-Small-Region.svs') as a real path) ./out.svs -i
```

Expected: info output matches pre-change (no extra line).

- [ ] **Step 5: Commit.**

```powershell
git add src/tools/converter/sceneconverter.cpp
git commit -m "converter CLI: surface per-reader HTTP cache footprint for remote inputs"
```

---

## Task 6: Mark the converter S3 work in the S3 streaming spec status section

The S3 streaming spec (`software-docs/specs/2026-05-25-s3-streaming-design.md`) §14 records v1 implementation status. With converter S3 input shipped, append one bullet so the spec's status accurately reflects what's in tree.

**Files:**
- Modify: `software-docs/specs/2026-05-25-s3-streaming-design.md` (§14 "v1 implementation status")

- [ ] **Step 1: Append a follow-up entry to §14.**

Find the "v1 deviations / known limitations" list and the "Follow-up hardening" list under §14. Add a new bullet **at the end of the "Delivered" paragraph** (right after the AFI sentence and before "v1 deviations / known limitations"):

```markdown
The converter CLI (`slideio_converter`) accepts `s3://` and `http(s)://`
URIs as input as of 2026-05-28; output remains a local file path (per §1
non-goal). See
[`2026-05-28-converter-s3-input-design.md`](./2026-05-28-converter-s3-input-design.md).
```

- [ ] **Step 2: Commit.**

```powershell
git add software-docs/specs/2026-05-25-s3-streaming-design.md
git commit -m "docs: note converter S3 input in v1 streaming status"
```

---

## Final verification

After all tasks, run the full test suite to confirm no regression:

```powershell
.\build\release\bin\slideio_tests.exe
.\build\release\bin\slideio_converter_tests.exe
```

Expected: both green. The `slideio_tests` count should be exactly two higher than before (the two new `ConverterS3` tests).
