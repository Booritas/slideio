# Parallel `read_block` Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Let two `read_block` calls on one `Scene` run concurrently, for the
eight slide formats whose drivers can be made thread-safe without pulling in a
third-party library's threading model.

**Architecture:** `CVScene` stops taking `m_readBlockMutex` unconditionally and
instead consults a new virtual, `supportsConcurrentReads()`, which defaults to
`false`. Drivers opt in after their read path is converted to one of two
mechanisms: `FileReader` (positional, cursor-free I/O — CZI, VSI) or
`ContextPool` (a bounded free-list of per-thread `ReadContext` objects holding
a file handle or scratch buffer — the TIFF family, NDPI, and the scratch halves
of CZI and VSI).

**Tech Stack:** C++17, CMake 3.10+, Conan v2, GoogleTest, OpenCV, libtiff (plus
a separate `ndpi-tiff` fork inside the NDPI module), spdlog via the
`slideio-core` logging seam. No new dependencies: `FileReader` uses `pread` and
`ReadFile`/`OVERLAPPED` directly.

**Spec:** `software-docs/specs/2026-09-07-parallel-read-block-design.md`

## Global Constraints

- **Default-deny.** `CVScene::supportsConcurrentReads()` returns `false` in the
  base class. A driver returns `true` only in the task that converts it.
  Spec §4.1.
- **No new public API** beyond `supportsConcurrentReads()`. No
  `setMaxReaders`, no environment variable, no pool knob. Spec §3.3.
- **Pool bound:** `min(8, std::thread::hardware_concurrency())`, minimum 1,
  from `ContextPool::defaultMax()`. `ContextPool::kUnbounded == 0` for pools
  whose context holds no scarce resource. Spec §4.2.
- **Out of scope, do not touch:** ZVI, DCM, GDAL, OME-TIFF read paths; the
  fused YCbCr repack; `Tiler::getTileIndices`; the redundant clears and
  identity resize; `read_batch`; `TiffConverter::cloneScene()`. Spec §8.
- **No `thread_local` for anything with a lifetime** — handles and scratch
  buffers live in a `ReadContext`, never in thread-local storage. The one
  permitted `thread_local` is the Windows `OVERLAPPED` event in `FileReader`,
  which owns no file state. Spec §4.2, §4.3.
- **Two libtiff instances.** The regular libtiff (via
  `slideio/imagetools/libtiff.hpp`, namespace `libtiff`) and the NDPI fork
  (via `slideio/drivers/ndpi/ndpilibtiff.hpp`, plain `<tiffio.h>` inside that
  module) have **separate globals**. Handler installation must happen once per
  instance, in two different places. Tasks 1 and 2.
- **Test images:** `TestTools::getTestImagePath(subfolder, name)`; guard every
  image-reading test with `SLIDEIO_SKIP_IF_IMAGE_MISSING(filePath)`, which must
  appear in the test body or a fixture's `SetUp`, never in a helper.
- **Build:** `python install.py -a build-only -c release`. Test binaries are at
  `build/release/bin/<name>` on Linux/macOS and `build/<name>.exe` on Windows;
  the commands below use the Linux path.

---

## Task 1: Install the regular libtiff message handlers once

Spec §4.4. This is a standalone improvement and a precondition for every
driver task: `TIFFMessageHandler`'s constructor/destructor swap libtiff's
**process-global** error and warning handlers, which two concurrent readers
would write outside any lock.

**Files:**
- Modify: `src/slideio/imagetools/tiffmessagehandler.hpp`
- Modify: `src/slideio/imagetools/tiffmessagehandler.cpp`
- Modify: `src/slideio/imagetools/tiffkeeper.hpp`, `tiffkeeper.cpp` (drop `m_messageHandler`)
- Modify: `src/slideio/imagetools/tifffiles.hpp`, `tifffiles.cpp` (drop `m_messageHandler` and `initMessageHandler`)
- Modify: `src/slideio/converter/tiffconverter.cpp` (drop the local `TIFFMessageHandler mh;`)
- Modify: `src/slideio/slideio/imagedrivermanager.cpp` (call the installer)
- Test: `src/tests/main/test_tiffmessagehandler.cpp` (create)
- Modify: `src/tests/main/CMakeLists.txt` (add the new source)

**Interfaces:**
- Consumes: nothing.
- Produces: `void slideio::installTiffMessageHandlers();` — idempotent,
  thread-safe, installs the error and warning handlers into the regular
  libtiff. Declared in `tiffmessagehandler.hpp`.

- [ ] **Step 1: Write the failing test**

Create `src/tests/main/test_tiffmessagehandler.cpp`:

```cpp
// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#include <gtest/gtest.h>
#include "slideio/imagetools/tiffmessagehandler.hpp"
#include "slideio/imagetools/libtiff.hpp"
#include "slideio/imagetools/tiffkeeper.hpp"

// Reads the currently installed handler without disturbing it: TIFFSetErrorHandler
// returns the previous one, so setting it back restores the status quo.
static libtiff::TIFFErrorHandler currentErrorHandler() {
    libtiff::TIFFErrorHandler previous = libtiff::TIFFSetErrorHandler(nullptr);
    libtiff::TIFFSetErrorHandler(previous);
    return previous;
}

static libtiff::TIFFErrorHandler currentWarningHandler() {
    libtiff::TIFFErrorHandler previous = libtiff::TIFFSetWarningHandler(nullptr);
    libtiff::TIFFSetWarningHandler(previous);
    return previous;
}

TEST(TiffMessageHandler, installsHandlers) {
    slideio::installTiffMessageHandlers();
    EXPECT_NE(currentErrorHandler(), nullptr);
    EXPECT_NE(currentWarningHandler(), nullptr);
}

TEST(TiffMessageHandler, installIsIdempotent) {
    slideio::installTiffMessageHandlers();
    const libtiff::TIFFErrorHandler firstError = currentErrorHandler();
    const libtiff::TIFFErrorHandler firstWarning = currentWarningHandler();
    slideio::installTiffMessageHandlers();
    EXPECT_EQ(currentErrorHandler(), firstError);
    EXPECT_EQ(currentWarningHandler(), firstWarning);
}

// The regression this task exists to prevent: a TIFFKeeper's lifetime must no
// longer change the process-global handlers.
TEST(TiffMessageHandler, keeperLifetimeDoesNotChangeHandlers) {
    slideio::installTiffMessageHandlers();
    const libtiff::TIFFErrorHandler beforeError = currentErrorHandler();
    const libtiff::TIFFErrorHandler beforeWarning = currentWarningHandler();
    {
        slideio::TIFFKeeper keeper;
        EXPECT_EQ(currentErrorHandler(), beforeError);
        EXPECT_EQ(currentWarningHandler(), beforeWarning);
    }
    EXPECT_EQ(currentErrorHandler(), beforeError);
    EXPECT_EQ(currentWarningHandler(), beforeWarning);
}

// Spec §6's "diagnostics still routed" gate. Installing once instead of
// swapping per object must not lose log routing -- which was the whole
// justification for the old swap, so it is the regression to guard.
TEST(TiffMessageHandler, warningsReachTheLog) {
    slideio::installTiffMessageHandlers();
    slideio::setLogLevel("WARNING");
    testing::internal::CaptureStderr();
    libtiff::TIFFWarning("slideio-test", "canary %d", 1234);
    const std::string captured = testing::internal::GetCapturedStderr();
    slideio::setLogLevel("FATAL");
    EXPECT_NE(captured.find("canary 1234"), std::string::npos)
        << "a libtiff warning did not reach the log; captured:\n" << captured;
}
```

Add `#include "slideio/core/log.hpp"` for `setLogLevel`, following the pattern
in `src/tests/main/test_logging.cpp`, which is where `CaptureStderr` is
already used this way. Add `test_tiffmessagehandler.cpp` to the `TEST_SOURCES`
list in `src/tests/main/CMakeLists.txt`.

- [ ] **Step 2: Run the test to verify it fails**

```bash
python install.py -a build-only -c release
./build/release/bin/slideio_tests --gtest_filter="TiffMessageHandler.*" -v
```

Expected: compile error — `installTiffMessageHandlers` is not declared.

- [ ] **Step 3: Add the installer**

In `src/slideio/imagetools/tiffmessagehandler.hpp`, alongside the existing
class declaration:

```cpp
namespace slideio
{
    /// Installs the slideio error and warning handlers into libtiff. Idempotent
    /// and thread-safe; call from library initialisation. The handlers are
    /// process-global, so they are installed once and never swapped again --
    /// swapping them per object was a data race as soon as two threads could
    /// read at the same time.
    SLIDEIO_IMAGETOOLS_EXPORTS void installTiffMessageHandlers();
}
```

In `tiffmessagehandler.cpp`, after the two handler functions:

```cpp
void slideio::installTiffMessageHandlers() {
    static std::once_flag flag;
    std::call_once(flag, []() {
        libtiff::TIFFSetErrorHandler(TIFFErrorHandlerFunc);
        libtiff::TIFFSetWarningHandler(TIFFMessageHandlerFunc);
    });
}
```

Add `#include <mutex>` to that file.

- [ ] **Step 4: Delete the per-object swaps**

Remove the `TIFFMessageHandler` class definition (constructor and destructor)
and its declaration; keep only the two handler functions and the new
installer. Then remove every remaining reference:

- `tiffkeeper.hpp`: delete the `std::shared_ptr<TIFFMessageHandler> m_messageHandler;` member, the forward declaration, and the comment block describing the swap and the non-LIFO hazard. Replace that comment with one sentence saying the handlers are installed once at library initialisation.
- `tiffkeeper.cpp`: delete the `m_messageHandler = std::make_shared<TIFFMessageHandler>();` assignment from both constructors.
- `tifffiles.hpp` / `tifffiles.cpp`: delete the member, the forward declaration, the `initMessageHandler()` declaration and definition, and its call from the constructor.
- `tiffconverter.cpp`: delete the local `TIFFMessageHandler mh;`.

- [ ] **Step 5: Call the installer from library initialisation**

In `src/slideio/slideio/imagedrivermanager.cpp`, in the same one-time
initialisation path that sets up logging, add:

```cpp
    installTiffMessageHandlers();
```

with `#include "slideio/imagetools/tiffmessagehandler.hpp"`. Because the
installer is itself `call_once`, placing it here is safe regardless of how
many times initialisation is entered.

- [ ] **Step 6: Run the tests to verify they pass**

```bash
python install.py -a build-only -c release
./build/release/bin/slideio_tests --gtest_filter="TiffMessageHandler.*" -v
./build/release/bin/slideio_tests --gtest_filter="*TiffKeeper*:*TiffFiles*" -v
./build/release/bin/slideio_tests
./build/release/bin/slideio_converter_tests
```

Expected: PASS. The full `slideio_tests` and converter runs matter here because
this task deletes a member from two widely used classes.

- [ ] **Step 7: Commit**

```bash
git add src/slideio/imagetools/tiffmessagehandler.hpp src/slideio/imagetools/tiffmessagehandler.cpp \
        src/slideio/imagetools/tiffkeeper.hpp src/slideio/imagetools/tiffkeeper.cpp \
        src/slideio/imagetools/tifffiles.hpp src/slideio/imagetools/tifffiles.cpp \
        src/slideio/converter/tiffconverter.cpp src/slideio/slideio/imagedrivermanager.cpp \
        src/tests/main/test_tiffmessagehandler.cpp src/tests/main/CMakeLists.txt
git commit -m "install libtiff message handlers once instead of per object"
```

---

## Task 2: Install the NDPI libtiff handlers once, and stop swapping per tile

Spec §4.4. Separate task because the NDPI module links its **own** libtiff
fork with its own globals, so Task 1's installer cannot reach it. This task
also removes the per-tile swap: `NDPIScene::getTileRect` runs once per tile of
the whole level under today's `composeRect`, so a level-0 read of a large
slide performs on the order of 10⁵ global handler writes.

**Files:**
- Modify: `src/slideio/drivers/ndpi/ndpitiffmessagehandler.hpp`, `.cpp`
- Modify: `src/slideio/drivers/ndpi/ndpitiffkeeper.hpp`, `.cpp` (drop `m_messageHandler`)
- Modify: `src/slideio/drivers/ndpi/ndpiimagedriver.cpp` (install once; drop the local handler)
- Modify: `src/slideio/drivers/ndpi/ndpiscene.cpp` (delete the two per-tile constructions)
- Modify: `src/tests/ndpi/test_ndpi_driver.cpp`, `test_ndpitiffkeeper.cpp`, `test_ndpitiff_tools.cpp` (fixtures declare `m_messageHandler` members that no longer exist)
- Test: `src/tests/ndpi/test_ndpitiffmessagehandler.cpp` (create)
- Modify: `src/tests/ndpi/CMakeLists.txt`

**Interfaces:**
- Consumes: nothing from Task 1 — a separate libtiff instance.
- Produces: `void slideio::installNDPITiffMessageHandlers();` declared in
  `ndpitiffmessagehandler.hpp`.

- [ ] **Step 1: Write the failing test**

Create `src/tests/ndpi/test_ndpitiffmessagehandler.cpp`:

```cpp
// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#include <gtest/gtest.h>
#include "slideio/drivers/ndpi/ndpitiffmessagehandler.hpp"
#include "slideio/drivers/ndpi/ndpitiffkeeper.hpp"
#include <tiffio.h>

static TIFFErrorHandler currentNDPIErrorHandler() {
    TIFFErrorHandler previous = TIFFSetErrorHandler(nullptr);
    TIFFSetErrorHandler(previous);
    return previous;
}

TEST(NDPITiffMessageHandler, installsHandler) {
    slideio::installNDPITiffMessageHandlers();
    EXPECT_NE(currentNDPIErrorHandler(), nullptr);
}

TEST(NDPITiffMessageHandler, keeperLifetimeDoesNotChangeHandler) {
    slideio::installNDPITiffMessageHandlers();
    const TIFFErrorHandler before = currentNDPIErrorHandler();
    {
        slideio::NDPITIFFKeeper keeper;
        EXPECT_EQ(currentNDPIErrorHandler(), before);
    }
    EXPECT_EQ(currentNDPIErrorHandler(), before);
}
```

Add it to `TEST_SOURCES` in `src/tests/ndpi/CMakeLists.txt`.

- [ ] **Step 2: Run the test to verify it fails**

```bash
python install.py -a build-only -c release
./build/release/bin/slideio_ndpi_tests --gtest_filter="NDPITiffMessageHandler.*" -v
```

Expected: compile error — `installNDPITiffMessageHandlers` is not declared.

- [ ] **Step 3: Add the installer and delete the class**

In `ndpitiffmessagehandler.hpp`, replace the class declaration with:

```cpp
namespace slideio
{
    /// Installs the slideio handlers into the NDPI libtiff fork. Idempotent and
    /// thread-safe. The fork has its own process-global handlers, separate from
    /// the regular libtiff's, so this is a second installation point rather
    /// than a duplicate of installTiffMessageHandlers().
    SLIDEIO_NDPI_EXPORTS void installNDPITiffMessageHandlers();
}
```

In `ndpitiffmessagehandler.cpp`, delete the constructor and destructor and add:

```cpp
void slideio::installNDPITiffMessageHandlers() {
    static std::once_flag flag;
    std::call_once(flag, []() {
        TIFFSetErrorHandler(NDPITIFFErrorHandlerFunc);
        TIFFSetWarningHandler(NDPITIFFMessageHandlerFunc);
    });
}
```

using whatever the two existing handler function names are in that file, and
adding `#include <mutex>`.

- [ ] **Step 4: Remove every per-object and per-tile construction**

- `ndpitiffkeeper.hpp`: delete the `std::unique_ptr<NDPITIFFMessageHandler> m_messageHandler;` member, the forward declaration, and the comment describing the swap.
- `ndpitiffkeeper.cpp`: delete the `m_messageHandler = std::make_unique<NDPITIFFMessageHandler>();` assignment.
- `ndpiscene.cpp`: delete the `NDPITIFFMessageHandler mh;` line from **all three** sites — `NDPIScene::getTileRect`, `NDPIScene::readTile`, and the one near the top of the file (around the scene's initialisation path). Grep `NDPITIFFMessageHandler` in that file and remove every declaration.
- `ndpiimagedriver.cpp`: delete the local `NDPITIFFMessageHandler mh;` and call `installNDPITiffMessageHandlers();` from the `NDPIImageDriver` constructor instead.
- The three NDPI test files declare an `m_messageHandler` fixture member; delete those members and any `#include` that becomes unused.

- [ ] **Step 5: Run the tests to verify they pass**

```bash
python install.py -a build-only -c release
./build/release/bin/slideio_ndpi_tests --gtest_filter="NDPITiffMessageHandler.*" -v
./build/release/bin/slideio_ndpi_tests
```

Expected: PASS.

- [ ] **Step 6: Commit**

```bash
git add src/slideio/drivers/ndpi src/tests/ndpi
git commit -m "install NDPI libtiff handlers once; drop the per-tile swap"
```

---

## Task 3: The concurrency contract

Spec §4.1. Adds the virtual and the conditional lock, and fixes the
`assemble4DBlock` asymmetry recorded as the second half of TECH_DEBT #4. No
driver opts in, so observable behaviour is unchanged — which is exactly what
the test asserts.

**Files:**
- Modify: `src/slideio/core/cvscene.hpp`
- Modify: `src/slideio/core/cvscene.cpp`
- Modify: `src/slideio/slideio/scene.hpp` (documentation only)
- Test: `src/tests/main/test_concurrency_contract.cpp` (create)
- Modify: `src/tests/main/CMakeLists.txt`

**Interfaces:**
- Consumes: nothing.
- Produces:
  - `virtual bool CVScene::supportsConcurrentReads() const` — public, default `false`.
  - `std::unique_lock<std::mutex> CVScene::lockIfSerialised() const` — protected.

- [ ] **Step 1: Write the failing test**

Create `src/tests/main/test_concurrency_contract.cpp`:

```cpp
// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#include <gtest/gtest.h>
#include "testlib/testscene.hpp"

// TestScene is the in-tree fake CVScene used by the tile-composer tests. It does
// not override supportsConcurrentReads, so it must report the safe default.
TEST(ConcurrencyContract, defaultsToSerialised) {
    slideio::TestScene scene;
    EXPECT_FALSE(scene.supportsConcurrentReads());
}

namespace
{
    class ConcurrentTestScene : public slideio::TestScene
    {
    public:
        bool supportsConcurrentReads() const override { return true; }
    };
}

TEST(ConcurrencyContract, driverCanOptIn) {
    ConcurrentTestScene scene;
    EXPECT_TRUE(scene.supportsConcurrentReads());
}
```

Check `src/tests/testlib/testscene.hpp` for `TestScene`'s actual constructor
signature and default-construct it accordingly; if it requires arguments, pass
the same ones `test_tilecomposer.cpp` passes. Add the new file to
`TEST_SOURCES` in `src/tests/main/CMakeLists.txt`.

- [ ] **Step 2: Run the test to verify it fails**

```bash
python install.py -a build-only -c release
./build/release/bin/slideio_tests --gtest_filter="ConcurrencyContract.*" -v
```

Expected: compile error — `supportsConcurrentReads` is not a member.

- [ ] **Step 3: Add the virtual and the helper**

In `src/slideio/core/cvscene.hpp`, in the public section near the other read
declarations:

```cpp
        /**
         * True if two block reads of this scene may run concurrently on
         * different threads.
         *
         * False -- the default -- means the base class serialises block reads
         * of one scene, as it has always done. A driver returns true only once
         * every mutable object its read path touches is either per-thread (see
         * ContextPool) or cursor-free (see FileReader).
         */
        virtual bool supportsConcurrentReads() const { return false; }
```

and in the protected section, next to `m_readBlockMutex`:

```cpp
        /**
         * A lock that is engaged only for scenes that do not support
         * concurrent reads.
         *
         * The mutex is not recursive, so nothing called while holding this may
         * re-enter a locking entry point. In particular the plane callbacks
         * passed to assemble4DBlock must keep calling the *Ex read variants,
         * which do not lock.
         */
        std::unique_lock<std::mutex> lockIfSerialised() const {
            return supportsConcurrentReads()
                       ? std::unique_lock<std::mutex>()
                       : std::unique_lock<std::mutex>(m_readBlockMutex);
        }
```

- [ ] **Step 4: Convert the three lock sites**

In `src/slideio/core/cvscene.cpp`, `grep -n m_readBlockMutex` finds three
`std::lock_guard` constructions plus the header declaration. Replace each
construction:

In `readResampledBlockChannels` and in `readResampledLevelBlockChannels`:

```cpp
    auto lock = lockIfSerialised();
```

In `assemble4DBlock`, the mutex is currently taken only inside the
`if (planeMatrix)` branch. Give both branches the same guard:

```cpp
            if (planeMatrix) {
                auto lock = lockIfSerialised();
                readPlane(zSlieceIndex, tfIndex, dataRaster);
            }
            else {
                cv::Mat sliceRaster;
                {
                    auto lock = lockIfSerialised();
                    readPlane(zSlieceIndex, tfIndex, sliceRaster);
                }
                CVTools::insertSliceInMultidimMatrix(dataRaster, sliceRaster, indices);
            }
```

The inner scope in the second branch matters: `insertSliceInMultidimMatrix`
touches only locals and the destination, so it must not be held under the lock.

- [ ] **Step 5: Document the contract change in the public header**

In `src/slideio/slideio/scene.hpp`, update the class-level documentation so it
no longer promises serialisation. Replace any wording that says reads are
serialised with:

```
 * Thread safety: block reads of one Scene are always safe to call from several
 * threads. Whether they *overlap* depends on the driver: a scene whose driver
 * supports concurrent reads runs them in parallel, otherwise the library
 * serialises them internally. Callers that relied on reads of one scene being
 * mutually exclusive in order to protect their own state must take their own
 * lock.
```

- [ ] **Step 6: Run the tests to verify they pass**

```bash
python install.py -a build-only -c release
./build/release/bin/slideio_tests --gtest_filter="ConcurrencyContract.*" -v
./build/release/bin/slideio_tests
./build/release/bin/slideio_ndpi_tests
./build/release/bin/slideio_vsi_tests
./build/release/bin/slideio_pke_tests
./build/release/bin/slideio_phtiff_tests
./build/release/bin/slideio_ometiff_tests
./build/release/bin/slideio_converter_tests
```

Expected: PASS, everywhere. No driver opts in yet, so every existing
`multiThreadSceneAccess` test still exercises the serialised path.

- [ ] **Step 7: Commit**

```bash
git add src/slideio/core/cvscene.hpp src/slideio/core/cvscene.cpp \
        src/slideio/slideio/scene.hpp \
        src/tests/main/test_concurrency_contract.cpp src/tests/main/CMakeLists.txt
git commit -m "add supportsConcurrentReads contract; make the 4D lock symmetric"
```

---

## Task 4: `FileReader` and `SequentialReader`

Spec §4.3. Positional, cursor-free reads so one descriptor serves every
thread. Two platform behaviours are the whole point of the class and both fail
quietly if got wrong: a Windows handle opened without `FILE_FLAG_OVERLAPPED`
moves its file pointer even when given an offset, and `pread` may return
short.

**Files:**
- Create: `src/slideio/core/tools/filereader.hpp`
- Create: `src/slideio/core/tools/filereader.cpp`
- Create: `src/slideio/core/tools/sequentialreader.hpp`
- Modify: `src/slideio/core/CMakeLists.txt`
- Test: `src/tests/main/test_filereader.cpp` (create)
- Modify: `src/tests/main/CMakeLists.txt`

**Interfaces:**
- Consumes: nothing.
- Produces:
  - `slideio::FileReader(const std::string& path)`
  - `void FileReader::readAt(uint64_t offset, void* dst, size_t size) const` — fills exactly `size` bytes or throws `RuntimeError`
  - `using FileReader::ReadPrimitive = std::function<int64_t(void*, size_t, uint64_t)>`
  - `static void FileReader::fillFrom(void* dst, size_t size, uint64_t offset, const ReadPrimitive& primitive, const std::string& path)` — the retry loop, extracted so a short read can be tested without a filesystem that produces one
  - `uint64_t FileReader::size() const`
  - `slideio::SequentialReader(const FileReader&)` with `template <class T> void read(T&)`, `void readBytes(void*, size_t)`, `void skip(int64_t)`, `void setPos(uint64_t)`, `uint64_t pos() const`

- [ ] **Step 1: Write the failing test**

Create `src/tests/main/test_filereader.cpp`:

```cpp
// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#include <gtest/gtest.h>
#include "slideio/core/tools/filereader.hpp"
#include "slideio/core/tools/sequentialreader.hpp"
#include "slideio/core/exceptions.hpp"
#include <filesystem>
#include <fstream>
#include <numeric>
#include <thread>
#include <vector>

namespace
{
    // Writes a temp file of `size` bytes where byte i == i % 251, so any offset
    // has a locally checkable expected value.
    std::string writePattern(const std::string& name, size_t size) {
        const std::filesystem::path path =
            std::filesystem::temp_directory_path() / name;
        std::vector<uint8_t> data(size);
        for (size_t i = 0; i < size; ++i) {
            data[i] = static_cast<uint8_t>(i % 251);
        }
        std::ofstream out(path, std::ios::binary | std::ios::trunc);
        out.write(reinterpret_cast<const char*>(data.data()), data.size());
        out.close();
        return path.string();
    }
}

TEST(FileReader, reportsSize) {
    const std::string path = writePattern("slideio_fr_size.bin", 4096);
    slideio::FileReader reader(path);
    EXPECT_EQ(reader.size(), 4096u);
    std::filesystem::remove(path);
}

TEST(FileReader, readsAtAnOffset) {
    const std::string path = writePattern("slideio_fr_offset.bin", 4096);
    slideio::FileReader reader(path);
    std::vector<uint8_t> buffer(16);
    reader.readAt(1000, buffer.data(), buffer.size());
    for (size_t i = 0; i < buffer.size(); ++i) {
        EXPECT_EQ(buffer[i], static_cast<uint8_t>((1000 + i) % 251)) << "byte " << i;
    }
    std::filesystem::remove(path);
}

// The property the whole class exists for: no shared cursor. Two sequential
// reads at the same offset must return the same bytes, and a read must not
// depend on what was read before it.
TEST(FileReader, readsAreIndependentOfOrder) {
    const std::string path = writePattern("slideio_fr_order.bin", 8192);
    slideio::FileReader reader(path);
    std::vector<uint8_t> first(32), second(32);
    reader.readAt(4096, first.data(), first.size());
    reader.readAt(0, second.data(), second.size());
    std::vector<uint8_t> again(32);
    reader.readAt(4096, again.data(), again.size());
    EXPECT_EQ(first, again);
    std::filesystem::remove(path);
}

TEST(FileReader, throwsPastEndOfFile) {
    const std::string path = writePattern("slideio_fr_eof.bin", 512);
    slideio::FileReader reader(path);
    std::vector<uint8_t> buffer(256);
    EXPECT_THROW(reader.readAt(400, buffer.data(), buffer.size()),
                 slideio::RuntimeError);
    std::filesystem::remove(path);
}

TEST(FileReader, throwsOnMissingFile) {
    EXPECT_THROW(slideio::FileReader("no-such-file-4b8c1e.bin"),
                 slideio::RuntimeError);
}

// 16 threads reading overlapping offsets through ONE FileReader. Every read is
// checked against the byte pattern, so a shared-cursor bug shows up as wrong
// data rather than as a crash. This is also the test the ThreadSanitizer job in
// Task 7 runs.
TEST(FileReader, concurrentReadsReturnCorrectBytes) {
    const std::string path = writePattern("slideio_fr_mt.bin", 1u << 20);
    slideio::FileReader reader(path);
    constexpr int threadCount = 16;
    constexpr int readsPerThread = 200;
    std::vector<std::thread> threads;
    std::atomic<int> failures{0};
    for (int t = 0; t < threadCount; ++t) {
        threads.emplace_back([&reader, &failures, t]() {
            std::vector<uint8_t> buffer(64);
            for (int i = 0; i < readsPerThread; ++i) {
                const uint64_t offset =
                    static_cast<uint64_t>((t * 7919 + i * 4093) % ((1 << 20) - 64));
                reader.readAt(offset, buffer.data(), buffer.size());
                for (size_t b = 0; b < buffer.size(); ++b) {
                    if (buffer[b] != static_cast<uint8_t>((offset + b) % 251)) {
                        ++failures;
                        return;
                    }
                }
            }
        });
    }
    for (auto& thread : threads) {
        thread.join();
    }
    EXPECT_EQ(failures.load(), 0);
    std::filesystem::remove(path);
}

// Spec §6's short-read gate. pread is allowed to return fewer bytes than asked
// for; ifstream::read hid that, so the retry loop is the one guarantee actively
// lost in this migration. A filesystem that produces short reads on demand is
// not portable to arrange, so the loop is tested directly through its seam.
TEST(FileReader, retriesShortReadsUntilSatisfied) {
    std::vector<uint8_t> destination(1000, 0);
    int calls = 0;
    // Returns one byte per call, so a loop that trusts the first return value
    // fills 1 byte out of 1000 and this test catches it.
    slideio::FileReader::ReadPrimitive dribble =
        [&calls](void* dst, size_t size, uint64_t offset) -> int64_t {
            ++calls;
            if (size == 0) {
                return 0;
            }
            *static_cast<uint8_t*>(dst) = static_cast<uint8_t>(offset % 251);
            return 1;
        };
    slideio::FileReader::fillFrom(destination.data(), destination.size(), 7,
                                  dribble, "fake");
    EXPECT_EQ(calls, 1000);
    for (size_t i = 0; i < destination.size(); ++i) {
        EXPECT_EQ(destination[i], static_cast<uint8_t>((7 + i) % 251)) << "byte " << i;
    }
}

TEST(FileReader, retriesInterruptedReads) {
    std::vector<uint8_t> destination(4, 0);
    int calls = 0;
    // -1 means "retryable interruption" (EINTR). The loop must not treat it as
    // an error and must not advance the offset.
    slideio::FileReader::ReadPrimitive flaky =
        [&calls](void* dst, size_t size, uint64_t offset) -> int64_t {
            ++calls;
            if (calls % 2 == 1) {
                return -1;
            }
            *static_cast<uint8_t*>(dst) = 0xAB;
            return 1;
        };
    slideio::FileReader::fillFrom(destination.data(), destination.size(), 0,
                                  flaky, "fake");
    EXPECT_EQ(calls, 8);
    EXPECT_EQ(destination, std::vector<uint8_t>(4, 0xAB));
}

TEST(FileReader, throwsWhenThePrimitiveHitsEndOfFile) {
    std::vector<uint8_t> destination(8, 0);
    slideio::FileReader::ReadPrimitive empty =
        [](void*, size_t, uint64_t) -> int64_t { return 0; };
    EXPECT_THROW(slideio::FileReader::fillFrom(destination.data(), destination.size(),
                                               0, empty, "fake"),
                 slideio::RuntimeError);
}

TEST(SequentialReader, readsStructsAndSkips) {
    const std::string path = writePattern("slideio_sr.bin", 1024);
    slideio::FileReader reader(path);
    slideio::SequentialReader sequential(reader);
    uint32_t first = 0;
    sequential.read(first);
    EXPECT_EQ(sequential.pos(), 4u);
    sequential.skip(4);
    EXPECT_EQ(sequential.pos(), 8u);
    uint8_t byte = 0;
    sequential.read(byte);
    EXPECT_EQ(byte, static_cast<uint8_t>(8));
    std::filesystem::remove(path);
}
```

Add `test_filereader.cpp` to `TEST_SOURCES` in `src/tests/main/CMakeLists.txt`.

- [ ] **Step 2: Run the test to verify it fails**

```bash
python install.py -a build-only -c release
./build/release/bin/slideio_tests --gtest_filter="FileReader.*:SequentialReader.*" -v
```

Expected: compile error — `filereader.hpp` does not exist.

- [ ] **Step 3: Write `FileReader`**

Create `src/slideio/core/tools/filereader.hpp`:

```cpp
// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#pragma once
#include "slideio/core/slideio_core_def.hpp"
#include <cstdint>
#include <functional>
#include <string>

namespace slideio
{
    /**
     * A read-only file opened for positional access.
     *
     * readAt() carries its own offset and never touches a shared cursor, so a
     * single FileReader serves any number of threads with one descriptor -- no
     * pool, no per-thread handle. This is what lets the CZI and VSI read paths
     * be concurrent without replicating anything.
     *
     * Not memory-mapped, deliberately: a truncated or network-backed file would
     * raise SIGBUS or an SEH exception inside a memcpy, which is not survivable
     * in a library that reads arbitrary user files.
     */
    class SLIDEIO_CORE_EXPORTS FileReader
    {
    public:
        explicit FileReader(const std::string& path);
        ~FileReader();

        FileReader(const FileReader&) = delete;
        FileReader& operator=(const FileReader&) = delete;

        /// Fills exactly `size` bytes from `offset`, or throws. Thread-safe.
        void readAt(uint64_t offset, void* dst, size_t size) const;
        uint64_t size() const { return m_size; }
        const std::string& path() const { return m_path; }

        /// Reads `size` bytes into `dst` by calling `primitive` until satisfied.
        /// `primitive(dst, size, offset)` returns the number of bytes read, 0 at
        /// end of file (which throws), or -1 for a retryable interruption.
        ///
        /// Extracted from readAt so the retry loop can be unit-tested with a
        /// primitive that deliberately returns short. That matters because pread
        /// is permitted to return fewer bytes than requested and ifstream::read
        /// used to hide it -- this loop is the guarantee being restored, and a
        /// filesystem that produces short reads on demand is not portable to
        /// arrange in a test.
        using ReadPrimitive = std::function<int64_t(void* dst, size_t size, uint64_t offset)>;
        static void fillFrom(void* dst, size_t size, uint64_t offset,
                             const ReadPrimitive& primitive, const std::string& path);

    private:
        std::string m_path;
#if defined(WIN32)
        void* m_handle;
#else
        int m_fd;
#endif
        uint64_t m_size;
    };
}
```

Create `src/slideio/core/tools/filereader.cpp`:

```cpp
// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#include "slideio/core/tools/filereader.hpp"
#include "slideio/core/exceptions.hpp"
#include "slideio/core/tools/tools.hpp"

#if defined(WIN32)
#include <windows.h>
#else
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#endif

using namespace slideio;

// Platform-independent, and the only place the "fill or throw" contract lives.
void FileReader::fillFrom(void* dst, size_t size, uint64_t offset,
                          const ReadPrimitive& primitive, const std::string& path) {
    uint8_t* cursor = static_cast<uint8_t*>(dst);
    size_t remaining = size;
    uint64_t position = offset;
    while (remaining > 0) {
        const int64_t read = primitive(cursor, remaining, position);
        if (read < 0) {
            continue;                      // retryable interruption
        }
        if (read == 0) {
            RAISE_RUNTIME_ERROR << "FileReader: unexpected end of file " << path
                                << " at " << position;
        }
        cursor += static_cast<size_t>(read);
        remaining -= static_cast<size_t>(read);
        position += static_cast<uint64_t>(read);
    }
}

#if defined(WIN32)

FileReader::FileReader(const std::string& path) : m_path(path), m_handle(nullptr), m_size(0) {
    const std::wstring wsPath = Tools::toWstring(path);
    // FILE_FLAG_OVERLAPPED is not optional. Passing an OVERLAPPED offset to a
    // handle opened without it does read from the offset, but it also moves the
    // shared file pointer, and concurrent operations on such a handle are not
    // supported -- the build would be silently racy.
    HANDLE handle = ::CreateFileW(wsPath.c_str(), GENERIC_READ,
                                  FILE_SHARE_READ, nullptr, OPEN_EXISTING,
                                  FILE_FLAG_OVERLAPPED | FILE_FLAG_RANDOM_ACCESS,
                                  nullptr);
    if (handle == INVALID_HANDLE_VALUE) {
        RAISE_RUNTIME_ERROR << "FileReader: cannot open file " << path
                            << ". Error code: " << ::GetLastError();
    }
    LARGE_INTEGER fileSize = {};
    if (!::GetFileSizeEx(handle, &fileSize)) {
        const DWORD error = ::GetLastError();
        ::CloseHandle(handle);
        RAISE_RUNTIME_ERROR << "FileReader: cannot query size of " << path
                            << ". Error code: " << error;
    }
    m_handle = handle;
    m_size = static_cast<uint64_t>(fileSize.QuadPart);
}

FileReader::~FileReader() {
    if (m_handle) {
        ::CloseHandle(static_cast<HANDLE>(m_handle));
    }
}

void FileReader::readAt(uint64_t offset, void* dst, size_t size) const {
    if (size == 0) {
        return;
    }
    if (offset + size > m_size) {
        RAISE_RUNTIME_ERROR << "FileReader: read of " << size << " bytes at "
                            << offset << " is past the end of " << m_path
                            << " (" << m_size << " bytes)";
    }
    // One manual-reset event per thread, reused across calls. It holds no file
    // state, so it is the one thread_local this design permits.
    thread_local HANDLE event = ::CreateEventW(nullptr, TRUE, FALSE, nullptr);
    if (!event) {
        RAISE_RUNTIME_ERROR << "FileReader: cannot create an event object";
    }
    HANDLE handle = static_cast<HANDLE>(m_handle);
    const std::string& path = m_path;
    fillFrom(dst, size, offset,
             [handle, event, &path](void* chunkDst, size_t chunkSize,
                                    uint64_t position) -> int64_t {
                 const DWORD chunk = static_cast<DWORD>(
                     std::min<size_t>(chunkSize, 0x40000000u));
                 OVERLAPPED overlapped = {};
                 overlapped.Offset = static_cast<DWORD>(position & 0xFFFFFFFFu);
                 overlapped.OffsetHigh = static_cast<DWORD>(position >> 32);
                 overlapped.hEvent = event;
                 ::ResetEvent(event);
                 DWORD read = 0;
                 if (!::ReadFile(handle, chunkDst, chunk, &read, &overlapped)) {
                     const DWORD error = ::GetLastError();
                     if (error != ERROR_IO_PENDING) {
                         RAISE_RUNTIME_ERROR << "FileReader: read failed on " << path
                                             << " at " << position
                                             << ". Error code: " << error;
                     }
                     if (!::GetOverlappedResult(handle, &overlapped, &read, TRUE)) {
                         RAISE_RUNTIME_ERROR << "FileReader: read failed on " << path
                                             << " at " << position
                                             << ". Error code: " << ::GetLastError();
                     }
                 }
                 return static_cast<int64_t>(read);
             },
             m_path);
}

#else

FileReader::FileReader(const std::string& path) : m_path(path), m_fd(-1), m_size(0) {
    const int fd = ::open(path.c_str(), O_RDONLY | O_CLOEXEC);
    if (fd < 0) {
        RAISE_RUNTIME_ERROR << "FileReader: cannot open file " << path;
    }
    struct stat info = {};
    if (::fstat(fd, &info) != 0) {
        ::close(fd);
        RAISE_RUNTIME_ERROR << "FileReader: cannot query size of " << path;
    }
#if defined(POSIX_FADV_RANDOM)
    ::posix_fadvise(fd, 0, 0, POSIX_FADV_RANDOM);
#endif
    m_fd = fd;
    m_size = static_cast<uint64_t>(info.st_size);
}

FileReader::~FileReader() {
    if (m_fd >= 0) {
        ::close(m_fd);
    }
}

void FileReader::readAt(uint64_t offset, void* dst, size_t size) const {
    if (size == 0) {
        return;
    }
    if (offset + size > m_size) {
        RAISE_RUNTIME_ERROR << "FileReader: read of " << size << " bytes at "
                            << offset << " is past the end of " << m_path
                            << " (" << m_size << " bytes)";
    }
    // pread does not touch the file offset, so one fd is safe across threads --
    // but it may return short, which fillFrom is what handles.
    const int fd = m_fd;
    const std::string& path = m_path;
    fillFrom(dst, size, offset,
             [fd, &path](void* chunkDst, size_t chunkSize,
                         uint64_t position) -> int64_t {
                 const ssize_t read = ::pread(fd, chunkDst, chunkSize,
                                              static_cast<off_t>(position));
                 if (read < 0) {
                     if (errno == EINTR) {
                         return -1;        // fillFrom retries without advancing
                     }
                     RAISE_RUNTIME_ERROR << "FileReader: read failed on " << path
                                         << " at " << position << ". errno " << errno;
                 }
                 return static_cast<int64_t>(read);
             },
             m_path);
}

#endif
```

Add `#include <algorithm>` and `#include <cerrno>` as the compiler requires.

- [ ] **Step 4: Write `SequentialReader`**

Create `src/slideio/core/tools/sequentialreader.hpp`:

```cpp
// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#pragma once
#include "slideio/core/tools/filereader.hpp"

namespace slideio
{
    /**
     * A cursor over a FileReader, for the single-threaded parsing that runs
     * during init(). Header parsing reads one field after another and should
     * not be contorted into positional calls; the read path uses FileReader
     * directly.
     *
     * Non-owning: the FileReader must outlive it.
     */
    class SequentialReader
    {
    public:
        explicit SequentialReader(const FileReader& reader, uint64_t pos = 0)
            : m_reader(reader), m_pos(pos) {}

        template <class T>
        void read(T& value) {
            m_reader.readAt(m_pos, &value, sizeof(T));
            m_pos += sizeof(T);
        }

        template <class T>
        T readValue() {
            T value{};
            read(value);
            return value;
        }

        void readBytes(void* dst, size_t size) {
            m_reader.readAt(m_pos, dst, size);
            m_pos += size;
        }

        void skip(int64_t bytes) { m_pos += bytes; }
        void setPos(uint64_t pos) { m_pos = pos; }
        uint64_t pos() const { return m_pos; }
        uint64_t size() const { return m_reader.size(); }

    private:
        const FileReader& m_reader;
        uint64_t m_pos;
    };
}
```

- [ ] **Step 5: Add the sources to the core library**

In `src/slideio/core/CMakeLists.txt`, add `tools/filereader.cpp` to the source
list and `tools/filereader.hpp`, `tools/sequentialreader.hpp` to the header
list, following whatever pattern the neighbouring `tools/` entries use.

- [ ] **Step 6: Run the tests to verify they pass**

```bash
python install.py -a build-only -c release
./build/release/bin/slideio_tests --gtest_filter="FileReader.*:SequentialReader.*" -v
```

Expected: PASS, all eight tests.

- [ ] **Step 7: Commit**

```bash
git add src/slideio/core/tools/filereader.hpp src/slideio/core/tools/filereader.cpp \
        src/slideio/core/tools/sequentialreader.hpp src/slideio/core/CMakeLists.txt \
        src/tests/main/test_filereader.cpp src/tests/main/CMakeLists.txt
git commit -m "add FileReader for positional, cursor-free file reads"
```

---

## Task 5: `ReadContext` and `ContextPool`

Spec §4.2. The one mechanism every converted driver uses for per-thread
mutable read state. Free-list rather than thread-affine, so it is bounded,
releases deterministically, and survives a churning thread pool.

**Files:**
- Create: `src/slideio/core/tools/readcontext.hpp`
- Create: `src/slideio/core/tools/contextpool.hpp`
- Create: `src/slideio/core/tools/contextpool.cpp`
- Modify: `src/slideio/core/CMakeLists.txt`
- Test: `src/tests/main/test_contextpool.cpp` (create)
- Modify: `src/tests/main/CMakeLists.txt`

**Interfaces:**
- Consumes: nothing.
- Produces:
  - `class slideio::ReadContext` — abstract, virtual destructor, non-copyable.
  - `slideio::ContextPool(ContextPool::Factory factory, int maxContexts = ContextPool::defaultMax())`
  - `static int ContextPool::defaultMax()` — `min(8, hardware_concurrency())`, at least 1
  - `static constexpr int ContextPool::kUnbounded = 0`
  - `ContextPool::Borrow ContextPool::acquire()` — move-only RAII
  - `ReadContext& ContextPool::Borrow::get() const`, `template <class T> T& ContextPool::Borrow::as() const`
  - `int ContextPool::contextCount() const` — for tests only

- [ ] **Step 1: Write the failing test**

Create `src/tests/main/test_contextpool.cpp`:

```cpp
// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#include <gtest/gtest.h>
#include "slideio/core/tools/contextpool.hpp"
#include "slideio/core/tools/readcontext.hpp"
#include <atomic>
#include <thread>
#include <vector>

namespace
{
    class CountingContext : public slideio::ReadContext
    {
    public:
        explicit CountingContext(std::atomic<int>& liveCount) : m_liveCount(liveCount) {
            ++m_liveCount;
        }
        ~CountingContext() override { --m_liveCount; }
        int payload = 0;
    private:
        std::atomic<int>& m_liveCount;
    };
}

TEST(ContextPool, defaultMaxIsBoundedAndPositive) {
    const int max = slideio::ContextPool::defaultMax();
    EXPECT_GE(max, 1);
    EXPECT_LE(max, 8);
}

TEST(ContextPool, constructsLazily) {
    std::atomic<int> live{0};
    slideio::ContextPool pool([&live]() {
        return std::make_unique<CountingContext>(live);
    });
    EXPECT_EQ(live.load(), 0) << "no context should exist before the first acquire";
    {
        auto borrow = pool.acquire();
        EXPECT_EQ(live.load(), 1);
    }
    EXPECT_EQ(live.load(), 1) << "a returned context is kept for reuse, not destroyed";
    EXPECT_EQ(pool.contextCount(), 1);
}

TEST(ContextPool, reusesAReturnedContext) {
    std::atomic<int> live{0};
    slideio::ContextPool pool([&live]() {
        return std::make_unique<CountingContext>(live);
    });
    {
        auto borrow = pool.acquire();
        borrow.as<CountingContext>().payload = 42;
    }
    {
        auto borrow = pool.acquire();
        EXPECT_EQ(borrow.as<CountingContext>().payload, 42);
    }
    EXPECT_EQ(pool.contextCount(), 1);
}

TEST(ContextPool, handsOutDistinctContextsToSimultaneousBorrowers) {
    std::atomic<int> live{0};
    slideio::ContextPool pool([&live]() {
        return std::make_unique<CountingContext>(live);
    }, 4);
    auto first = pool.acquire();
    auto second = pool.acquire();
    EXPECT_NE(&first.get(), &second.get());
    EXPECT_EQ(live.load(), 2);
}

TEST(ContextPool, destroysEveryContextOnPoolDestruction) {
    std::atomic<int> live{0};
    {
        slideio::ContextPool pool([&live]() {
            return std::make_unique<CountingContext>(live);
        }, 4);
        auto first = pool.acquire();
        auto second = pool.acquire();
        EXPECT_EQ(live.load(), 2);
    }
    EXPECT_EQ(live.load(), 0);
}

// The bound is the point: a pool of 2 must never construct a third context, no
// matter how many threads ask.
TEST(ContextPool, neverExceedsItsBound) {
    std::atomic<int> live{0};
    std::atomic<int> peak{0};
    slideio::ContextPool pool([&live, &peak]() {
        auto context = std::make_unique<CountingContext>(live);
        int current = live.load();
        int seen = peak.load();
        while (current > seen && !peak.compare_exchange_weak(seen, current)) {
        }
        return context;
    }, 2);

    std::vector<std::thread> threads;
    for (int t = 0; t < 16; ++t) {
        threads.emplace_back([&pool]() {
            for (int i = 0; i < 100; ++i) {
                auto borrow = pool.acquire();
                borrow.as<CountingContext>().payload += 1;
            }
        });
    }
    for (auto& thread : threads) {
        thread.join();
    }
    EXPECT_LE(peak.load(), 2);
    EXPECT_LE(pool.contextCount(), 2);
}

// No two borrowers may hold the same context at the same time -- the property
// that makes a borrowed TIFF handle safe to use without further locking.
TEST(ContextPool, neverHandsOneContextToTwoBorrowersAtOnce) {
    std::atomic<int> live{0};
    slideio::ContextPool pool([&live]() {
        return std::make_unique<CountingContext>(live);
    }, 4);

    std::atomic<int> collisions{0};
    std::vector<std::thread> threads;
    for (int t = 0; t < 16; ++t) {
        threads.emplace_back([&pool, &collisions]() {
            for (int i = 0; i < 200; ++i) {
                auto borrow = pool.acquire();
                CountingContext& context = borrow.as<CountingContext>();
                // Exclusive access means this increment-then-check cannot observe
                // another borrower's value.
                context.payload = 1;
                if (context.payload != 1) {
                    ++collisions;
                }
                context.payload = 0;
            }
        });
    }
    for (auto& thread : threads) {
        thread.join();
    }
    EXPECT_EQ(collisions.load(), 0);
}

TEST(ContextPool, unboundedGrowsWithBorrowers) {
    std::atomic<int> live{0};
    slideio::ContextPool pool([&live]() {
        return std::make_unique<CountingContext>(live);
    }, slideio::ContextPool::kUnbounded);
    auto a = pool.acquire();
    auto b = pool.acquire();
    auto c = pool.acquire();
    EXPECT_EQ(live.load(), 3);
}

TEST(ContextPool, borrowIsMovable) {
    std::atomic<int> live{0};
    slideio::ContextPool pool([&live]() {
        return std::make_unique<CountingContext>(live);
    });
    auto borrow = pool.acquire();
    {
        slideio::ReadContext* const address = &borrow.get();
        auto moved = std::move(borrow);
        EXPECT_EQ(&moved.get(), address);
    }
    // The moved-from borrow must not have returned the context a second time.
    EXPECT_EQ(pool.contextCount(), 1);
    auto again = pool.acquire();
    EXPECT_EQ(pool.contextCount(), 1) << "double release would have grown the pool";
}
```

Add `test_contextpool.cpp` to `TEST_SOURCES` in `src/tests/main/CMakeLists.txt`.

- [ ] **Step 2: Run the test to verify it fails**

```bash
python install.py -a build-only -c release
./build/release/bin/slideio_tests --gtest_filter="ContextPool.*" -v
```

Expected: compile error — `contextpool.hpp` does not exist.

- [ ] **Step 3: Write `ReadContext`**

Create `src/slideio/core/tools/readcontext.hpp`:

```cpp
// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#pragma once
#include "slideio/core/slideio_core_def.hpp"

namespace slideio
{
    /**
     * Per-thread mutable state for one block read.
     *
     * A driver derives from this and adds whatever its read path cannot share
     * between threads: a file handle, a scratch buffer, a parsed container
     * object. Instances are handed out one borrower at a time by ContextPool,
     * so a driver may treat the contents as exclusively its own for the
     * duration of a read and needs no further locking.
     *
     * This is where such state belongs -- not in thread-local storage, which
     * would tie a file handle's lifetime to a thread rather than to the Scene
     * that owns it.
     */
    class SLIDEIO_CORE_EXPORTS ReadContext
    {
    public:
        virtual ~ReadContext() = default;
        ReadContext(const ReadContext&) = delete;
        ReadContext& operator=(const ReadContext&) = delete;
    protected:
        ReadContext() = default;
    };
}
```

- [ ] **Step 4: Write `ContextPool`**

Create `src/slideio/core/tools/contextpool.hpp`:

```cpp
// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#pragma once
#include "slideio/core/slideio_core_def.hpp"
#include "slideio/core/tools/readcontext.hpp"
#include <condition_variable>
#include <functional>
#include <memory>
#include <mutex>
#include <vector>

namespace slideio
{
    /**
     * A bounded free-list of ReadContext objects.
     *
     * acquire() hands out any free context rather than one keyed to the calling
     * thread. That is deliberate: a free list can be bounded, releases a
     * context when the borrow dies rather than when the thread does, and does
     * not leak one context per thread that has read and exited. It requires
     * that contexts be interchangeable between threads, which every context in
     * the tree is -- a TIFF handle repositioned per read, or a scratch buffer.
     *
     * Contexts are constructed lazily, so a scene nobody reads concurrently
     * holds exactly one.
     */
    class SLIDEIO_CORE_EXPORTS ContextPool
    {
    public:
        /// Pass as maxContexts when the context holds no scarce resource, so
        /// that capping it would serialise a path with no contention.
        static constexpr int kUnbounded = 0;
        /// min(8, hardware_concurrency()), at least 1.
        static int defaultMax();

        using Factory = std::function<std::unique_ptr<ReadContext>()>;

        explicit ContextPool(Factory factory, int maxContexts = defaultMax());
        /// Blocks until every outstanding Borrow has been returned.
        ~ContextPool();

        ContextPool(const ContextPool&) = delete;
        ContextPool& operator=(const ContextPool&) = delete;

        class SLIDEIO_CORE_EXPORTS Borrow
        {
        public:
            Borrow() = default;
            Borrow(Borrow&& other) noexcept;
            Borrow& operator=(Borrow&& other) noexcept;
            ~Borrow();

            Borrow(const Borrow&) = delete;
            Borrow& operator=(const Borrow&) = delete;

            ReadContext& get() const { return *m_context; }

            template <class T>
            T& as() const {
                return static_cast<T&>(*m_context);
            }

        private:
            friend class ContextPool;
            Borrow(ContextPool* pool, ReadContext* context)
                : m_pool(pool), m_context(context) {}
            void release();

            ContextPool* m_pool = nullptr;
            ReadContext* m_context = nullptr;
        };

        /// Reuses a free context, else constructs one, else blocks until another
        /// thread returns one. Throws if the pool is being destroyed.
        Borrow acquire();

        /// Contexts constructed so far. For tests.
        int contextCount() const;

    private:
        void give(ReadContext* context);

        Factory m_factory;
        int m_maxContexts;
        mutable std::mutex m_mutex;
        std::condition_variable m_available;
        std::vector<std::unique_ptr<ReadContext>> m_contexts;  // owns everything
        std::vector<ReadContext*> m_free;
        int m_borrowed = 0;
        bool m_closing = false;
    };
}
```

Create `src/slideio/core/tools/contextpool.cpp`:

```cpp
// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#include "slideio/core/tools/contextpool.hpp"
#include "slideio/core/exceptions.hpp"
#include <algorithm>
#include <thread>

using namespace slideio;

int ContextPool::defaultMax() {
    const unsigned int cores = std::thread::hardware_concurrency();
    if (cores == 0) {
        return 1;
    }
    return static_cast<int>(std::min(8u, cores));
}

ContextPool::ContextPool(Factory factory, int maxContexts)
    : m_factory(std::move(factory)), m_maxContexts(maxContexts) {
    if (!m_factory) {
        RAISE_RUNTIME_ERROR << "ContextPool: a context factory is required";
    }
    if (m_maxContexts < 0) {
        RAISE_RUNTIME_ERROR << "ContextPool: negative bound " << m_maxContexts;
    }
}

ContextPool::~ContextPool() {
    std::unique_lock<std::mutex> lock(m_mutex);
    m_closing = true;
    // A read racing a Slide close must not have its context freed underneath it.
    m_available.wait(lock, [this]() { return m_borrowed == 0; });
    m_free.clear();
    m_contexts.clear();
}

int ContextPool::contextCount() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return static_cast<int>(m_contexts.size());
}

ContextPool::Borrow ContextPool::acquire() {
    std::unique_lock<std::mutex> lock(m_mutex);
    for (;;) {
        if (m_closing) {
            RAISE_RUNTIME_ERROR << "ContextPool: the pool is being destroyed";
        }
        if (!m_free.empty()) {
            ReadContext* context = m_free.back();
            m_free.pop_back();
            ++m_borrowed;
            return Borrow(this, context);
        }
        const bool mayGrow = m_maxContexts == kUnbounded
                             || static_cast<int>(m_contexts.size()) < m_maxContexts;
        if (mayGrow) {
            ++m_borrowed;
            lock.unlock();
            // The factory does I/O (TIFFOpen), so it runs outside the lock.
            std::unique_ptr<ReadContext> created;
            try {
                created = m_factory();
            }
            catch (...) {
                lock.lock();
                --m_borrowed;
                m_available.notify_all();
                throw;
            }
            if (!created) {
                lock.lock();
                --m_borrowed;
                m_available.notify_all();
                RAISE_RUNTIME_ERROR << "ContextPool: the factory returned null";
            }
            ReadContext* context = created.get();
            lock.lock();
            m_contexts.push_back(std::move(created));
            return Borrow(this, context);
        }
        m_available.wait(lock);
    }
}

void ContextPool::give(ReadContext* context) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_free.push_back(context);
    --m_borrowed;
    m_available.notify_all();
}

ContextPool::Borrow::Borrow(Borrow&& other) noexcept
    : m_pool(other.m_pool), m_context(other.m_context) {
    other.m_pool = nullptr;
    other.m_context = nullptr;
}

ContextPool::Borrow& ContextPool::Borrow::operator=(Borrow&& other) noexcept {
    if (this != &other) {
        release();
        m_pool = other.m_pool;
        m_context = other.m_context;
        other.m_pool = nullptr;
        other.m_context = nullptr;
    }
    return *this;
}

ContextPool::Borrow::~Borrow() {
    release();
}

void ContextPool::Borrow::release() {
    if (m_pool && m_context) {
        m_pool->give(m_context);
    }
    m_pool = nullptr;
    m_context = nullptr;
}
```

Note on the growth path: `m_borrowed` is incremented *before* the factory runs
so that a concurrent destructor waits for the context under construction. The
`m_contexts.size() < m_maxContexts` check can therefore let two threads grow at
once only up to the bound, because both increments happen under the lock before
either releases it.

- [ ] **Step 5: Add the sources to the core library**

In `src/slideio/core/CMakeLists.txt`, add `tools/contextpool.cpp` to the
sources and `tools/contextpool.hpp`, `tools/readcontext.hpp` to the headers.

- [ ] **Step 6: Run the tests to verify they pass**

```bash
python install.py -a build-only -c release
./build/release/bin/slideio_tests --gtest_filter="ContextPool.*" -v
```

Expected: PASS, all nine tests. If `neverExceedsItsBound` is flaky, that is a
real defect in the growth path, not test noise — fix the pool.

- [ ] **Step 7: Commit**

```bash
git add src/slideio/core/tools/readcontext.hpp src/slideio/core/tools/contextpool.hpp \
        src/slideio/core/tools/contextpool.cpp src/slideio/core/CMakeLists.txt \
        src/tests/main/test_contextpool.cpp src/tests/main/CMakeLists.txt
git commit -m "add ContextPool, a bounded free-list of per-thread read contexts"
```

---

## Task 6: The byte-exactness harness

Spec §6. Every driver already has a `multiThreadSceneAccess` test calling
`TestTools::multiThreadedTest`, but that helper has each thread read **one**
random ROI and compare against whichever raster happened to be stored first.
It is a smoke test. The gate this work needs is different: compute a
single-threaded baseline for every ROI first, then have many threads read every
ROI many times and compare **every** read against that baseline.

The helper is validated in this task against a driver that is still
serialised, which proves the harness itself works before any driver's
behaviour changes.

**Files:**
- Modify: `src/tests/testlib/testtools.hpp`
- Modify: `src/tests/testlib/testtools.cpp`
- Test: `src/tests/main/test_svs_driver.cpp` (add one test; leave `multiThreadSceneAccess` alone)

**Interfaces:**
- Consumes: nothing.
- Produces:
  - `static void TestTools::concurrentReadIdentityTest(const std::string& filePath, slideio::ImageDriver& driver, int sceneIndex = 0, int numRois = 8, int numThreads = 16, int readsPerThread = 8);`
  - `static void TestTools::concurrentReadIdentityTestAllScenes(const std::string& filePath, slideio::ImageDriver& driver, int numRois = 4, int numThreads = 16, int readsPerThread = 4);`

- [ ] **Step 1: Write the failing test**

Add to `src/tests/main/test_svs_driver.cpp`:

```cpp
// The concurrency gate. SVS is still serialised at this point, and that is the
// point: a correct harness must pass against a serialised driver too, so
// running it here proves the harness before any driver's behaviour changes.
TEST(SVSImageDriver, concurrentReadsAreByteIdentical) {
    const std::string filePath = TestTools::getTestImagePath("svs", "JP2K-33003-1.svs");
    SLIDEIO_SKIP_IF_IMAGE_MISSING(filePath);
    slideio::SVSImageDriver driver;
    TestTools::concurrentReadIdentityTest(filePath, driver);
}
```

- [ ] **Step 2: Run the test to verify it fails**

```bash
python install.py -a build-only -c release
./build/release/bin/slideio_tests --gtest_filter="SVSImageDriver.concurrentReadsAreByteIdentical" -v
```

Expected: compile error — `concurrentReadIdentityTest` is not a member of
`TestTools`.

- [ ] **Step 3: Declare the helpers**

In `src/tests/testlib/testtools.hpp`, next to `multiThreadedTest`:

```cpp
    // Reads a set of ROIs single-threaded to build a baseline, then reads every
    // ROI from numThreads threads and requires every result to be byte-identical
    // to its baseline.
    //
    // This is the gate for concurrent reads, and it is shaped by the failure it
    // has to catch: a race on a shared decode buffer or file cursor produces
    // WRONG PIXELS, not an exception. A test that only checks for absent
    // exceptions passes while the data is corrupt.
    static void concurrentReadIdentityTest(const std::string& filePath,
                                           slideio::ImageDriver& driver,
                                           int sceneIndex = 0,
                                           int numRois = 8,
                                           int numThreads = 16,
                                           int readsPerThread = 8);

    // The same, applied to every scene of the slide in turn. Formats whose
    // slides carry more than one kind of scene -- VSI has ETS scenes and TIFF
    // scenes -- need every kind covered, not just scene 0.
    static void concurrentReadIdentityTestAllScenes(const std::string& filePath,
                                                    slideio::ImageDriver& driver,
                                                    int numRois = 4,
                                                    int numThreads = 16,
                                                    int readsPerThread = 4);
```

- [ ] **Step 4: Implement the helpers**

In `src/tests/testlib/testtools.cpp`:

```cpp
void TestTools::concurrentReadIdentityTest(const std::string& filePath,
                                           slideio::ImageDriver& driver,
                                           int sceneIndex,
                                           int numRois,
                                           int numThreads,
                                           int readsPerThread) {
    std::shared_ptr<slideio::CVSlide> slide = driver.openFile(filePath);
    ASSERT_TRUE(slide);
    ASSERT_GT(slide->getNumScenes(), sceneIndex);
    std::shared_ptr<slideio::CVScene> scene = slide->getScene(sceneIndex);
    ASSERT_TRUE(scene);

    const cv::Rect sceneRect = scene->getRect();
    // Small enough that numRois of them fit, big enough to span several codec
    // tiles so that tile assembly is exercised rather than a single tile.
    const cv::Size blockSize(std::min(512, std::max(16, sceneRect.width / 4)),
                             std::min(512, std::max(16, sceneRect.height / 4)));
    ASSERT_GT(blockSize.width, 0);
    ASSERT_GT(blockSize.height, 0);

    const int maxX = std::max(0, sceneRect.width - blockSize.width);
    const int maxY = std::max(0, sceneRect.height - blockSize.height);

    std::vector<cv::Rect> rois;
    rois.reserve(numRois);
    for (int i = 0; i < numRois; ++i) {
        const int x = (numRois == 1) ? 0 : (maxX * i) / (numRois - 1);
        const int y = (numRois == 1) ? 0 : (maxY * i) / (numRois - 1);
        rois.emplace_back(cv::Point(sceneRect.x + x, sceneRect.y + y), blockSize);
    }

    // Baseline, single-threaded. Native size: no resampling, so a mismatch is a
    // read bug and not an interpolation difference.
    std::vector<cv::Mat> baseline(rois.size());
    for (size_t i = 0; i < rois.size(); ++i) {
        scene->readResampledBlockChannels(rois[i], blockSize, {}, baseline[i]);
        ASSERT_FALSE(baseline[i].empty()) << "baseline roi " << i << " came back empty";
    }

    std::atomic<int> mismatches{0};
    std::atomic<int> exceptions{0};
    std::vector<std::thread> threads;
    threads.reserve(numThreads);
    for (int t = 0; t < numThreads; ++t) {
        threads.emplace_back([&, t]() {
            for (int r = 0; r < readsPerThread; ++r) {
                for (size_t i = 0; i < rois.size(); ++i) {
                    // Stagger the starting ROI per thread so threads are reading
                    // different regions at the same moment.
                    const size_t index = (i + static_cast<size_t>(t)) % rois.size();
                    try {
                        cv::Mat raster;
                        scene->readResampledBlockChannels(rois[index], blockSize, {}, raster);
                        if (raster.size() != baseline[index].size()
                            || raster.type() != baseline[index].type()) {
                            ++mismatches;
                            continue;
                        }
                        if (cv::norm(raster, baseline[index], cv::NORM_INF) != 0.0) {
                            ++mismatches;
                        }
                    }
                    catch (const std::exception&) {
                        ++exceptions;
                    }
                }
            }
        });
    }
    for (auto& thread : threads) {
        thread.join();
    }

    EXPECT_EQ(exceptions.load(), 0) << "concurrent reads threw";
    EXPECT_EQ(mismatches.load(), 0)
        << "concurrent reads returned different pixels than the single-threaded "
           "baseline -- this is data corruption, not a performance problem";
}

void TestTools::concurrentReadIdentityTestAllScenes(const std::string& filePath,
                                                    slideio::ImageDriver& driver,
                                                    int numRois,
                                                    int numThreads,
                                                    int readsPerThread) {
    std::shared_ptr<slideio::CVSlide> slide = driver.openFile(filePath);
    ASSERT_TRUE(slide);
    const int numScenes = slide->getNumScenes();
    ASSERT_GE(numScenes, 1);
    for (int sceneIndex = 0; sceneIndex < numScenes; ++sceneIndex) {
        SCOPED_TRACE("scene index " + std::to_string(sceneIndex));
        concurrentReadIdentityTest(filePath, driver, sceneIndex, numRois,
                                   numThreads, readsPerThread);
    }
}
```

Add `#include <atomic>`, `#include <thread>` and `#include <numeric>` to that
file if they are not already present.

- [ ] **Step 5: Run the test to verify it passes**

```bash
python install.py -a build-only -c release
./build/release/bin/slideio_tests --gtest_filter="SVSImageDriver.concurrentReadsAreByteIdentical" -v
```

Expected: PASS. SVS is still serialised, so this is measuring the harness, not
the driver. If it fails here, the harness is wrong — most likely the ROI
arithmetic produces a rect outside the scene, or `getRect()` returns a
non-zero-based rect (which is true for CZI and SCN; see TECH_DEBT #6 and #7 —
hence the `sceneRect.x +` and `sceneRect.y +` offsets above).

- [ ] **Step 6: Commit**

```bash
git add src/tests/testlib/testtools.hpp src/tests/testlib/testtools.cpp \
        src/tests/main/test_svs_driver.cpp
git commit -m "add a byte-exactness harness for concurrent scene reads"
```

---

## Task 7: The ThreadSanitizer CI job

Spec §6. TSan finds a shared-buffer race on the first run; review may not.
MSVC has no TSan, so this is a Linux/clang job and the coverage gap is
explicit.

**Scope note, and a deviation from the spec worth reading before starting.**
The spec's §6 asks for the per-driver byte-exactness tests to run under TSan.
Those tests need the slide corpus, which CI does not have — and `CLAUDE.md`
requires CI to leave `SLIDEIO_SKIP_MISSING_IMAGES` unset, so a job without
images fails rather than skips. This task therefore splits the gate:

- **Automatic, every push:** the mechanism-level tests under TSan —
  `FileReader.*` and `ContextPool.*`. These need no images and cover the code
  where a generic concurrency bug would live.
- **Manual and local:** the per-driver tests under TSan, via
  `workflow_dispatch` on a runner with the corpus, and via the documented local
  command below. This is what must be green before each driver task's commit
  merges.

**Files:**
- Modify: `.github/workflows/build-validation.yml`
- Modify: `software-docs/specs/2026-09-07-parallel-read-block-design.md` (§6, to record the split)

**Interfaces:**
- Consumes: `FileReader.*` and `ContextPool.*` tests from Tasks 4 and 5.
- Produces: a CI job named `tsan-linux`.

- [ ] **Step 1: Verify the sanitizer build works locally before automating it**

```bash
CXXFLAGS="-fsanitize=thread -g -O1" CFLAGS="-fsanitize=thread -g -O1" \
LDFLAGS="-fsanitize=thread" \
python install.py -a install -c release -bd build-tsan
./build-tsan/release/bin/slideio_tests --gtest_filter="FileReader.*:ContextPool.*" -v
```

Expected: PASS with no `WARNING: ThreadSanitizer` lines. If Conan needs a
clang profile, use the matching profile under `conan/Linux/`; record whatever
worked in the workflow step in Step 2 rather than inventing flags.

Note that OpenCV, libtiff and the other Conan dependencies are not
instrumented. TSan will therefore miss races *inside* them but still catches
races in slideio's own code, which is what these two test suites exercise.

- [ ] **Step 2: Add the automatic job**

Append to the `jobs:` map in `.github/workflows/build-validation.yml`, using
the same checkout, Python, Conan-cache and `sync-toolchain.py` steps the
existing `build-linux` job uses:

```yaml
  tsan-linux:
    name: ThreadSanitizer (Linux)
    runs-on: ubuntu-latest

    env:
      SLIDEIO_HOME: ${{ github.workspace }}
      CXXFLAGS: -fsanitize=thread -g -O1
      CFLAGS: -fsanitize=thread -g -O1
      LDFLAGS: -fsanitize=thread

    steps:
      - uses: actions/checkout@v4
        with:
          submodules: true

      - uses: actions/setup-python@v5
        with:
          python-version: '3.x'

      - name: Install Python dependencies
        run: pip install conan distro

      - name: Sync toolchain
        run: python sync-toolchain.py

      - name: Conan install
        run: python install.py -a conan -c release

      - name: Build
        run: python install.py -a install -c release

      # The mechanism-level suites only: they need no slide corpus, and CI must
      # leave SLIDEIO_SKIP_MISSING_IMAGES unset, so an image-reading suite would
      # fail here rather than skip. The per-driver byte-exactness tests run under
      # TSan on a runner with the corpus, and locally before each driver merges.
      - name: FileReader and ContextPool under ThreadSanitizer
        run: ./build/release/bin/slideio_tests --gtest_filter="FileReader.*:ContextPool.*"
```

- [ ] **Step 3: Record the per-driver command every driver task must run**

Add to the spec's §6, replacing the paragraph that states the sanitizer job
covers the per-driver tests:

```markdown
The gate is split by what CI can reach. The mechanism-level suites --
`FileReader.*` and `ContextPool.*` -- run under ThreadSanitizer on every push
(`tsan-linux` in `build-validation.yml`); they need no slide corpus. The
per-driver byte-exactness tests need the corpus, which CI does not carry, and
`CLAUDE.md` requires CI to leave `SLIDEIO_SKIP_MISSING_IMAGES` unset, so they
cannot run there. They must instead be run under ThreadSanitizer on a machine
with the corpus before each driver's opt-in commit merges:

    CXXFLAGS="-fsanitize=thread -g -O1" CFLAGS="-fsanitize=thread -g -O1" \
    LDFLAGS="-fsanitize=thread" python install.py -a install -c release -bd build-tsan
    ./build-tsan/release/bin/slideio_tests --gtest_filter="*concurrentReads*"

MSVC has no ThreadSanitizer, so Windows -- the primary development platform --
gets the byte-exactness tests only. Anything TSan finds is found on Linux.
```

- [ ] **Step 4: Commit**

```bash
git add .github/workflows/build-validation.yml \
        software-docs/specs/2026-09-07-parallel-read-block-design.md
git commit -m "run FileReader and ContextPool under ThreadSanitizer in CI"
```

---

## Task 8: Convert CZI

Spec §4.5.3. First driver, and the one whose read path needs no change beyond
the reader swap — so it validates Tasks 4 and 5 against the smallest diff.

**Files:**
- Modify: `src/slideio/drivers/czi/czislide.hpp`, `czislide.cpp`
- Modify: `src/slideio/drivers/czi/cziscene.hpp`, `cziscene.cpp`
- Test: `src/tests/main/test_czi_driver.cpp`

**Interfaces:**
- Consumes: `slideio::FileReader`, `slideio::SequentialReader` (Task 4);
  `TestTools::concurrentReadIdentityTest` (Task 6).
- Produces: `CZIScene::supportsConcurrentReads() == true`.

- [ ] **Step 1: Write the failing test**

Replace the body of `TEST(CZIImageDriver, multiThreadSceneAccess)` in
`src/tests/main/test_czi_driver.cpp` — keep the test, strengthen it — and add
the contract assertion:

```cpp
TEST(CZIImageDriver, multiThreadSceneAccess) {
    std::string filePath = TestTools::getTestImagePath("czi", "03_14_2019_DSGN0545_A_wb_1353_fov_1_633.czi");
    SLIDEIO_SKIP_IF_IMAGE_MISSING(filePath);
    slideio::CZIImageDriver driver;
    TestTools::multiThreadedTest(filePath, driver);
}

TEST(CZIImageDriver, concurrentReadsAreByteIdentical) {
    std::string filePath = TestTools::getTestImagePath("czi", "03_14_2019_DSGN0545_A_wb_1353_fov_1_633.czi");
    SLIDEIO_SKIP_IF_IMAGE_MISSING(filePath);
    slideio::CZIImageDriver driver;
    TestTools::concurrentReadIdentityTest(filePath, driver);
}

TEST(CZIImageDriver, reportsConcurrentReadSupport) {
    std::string filePath = TestTools::getTestImagePath("czi", "03_14_2019_DSGN0545_A_wb_1353_fov_1_633.czi");
    SLIDEIO_SKIP_IF_IMAGE_MISSING(filePath);
    slideio::CZIImageDriver driver;
    auto slide = driver.openFile(filePath);
    ASSERT_TRUE(slide);
    auto scene = slide->getScene(0);
    ASSERT_TRUE(scene);
    EXPECT_TRUE(scene->supportsConcurrentReads());
}
```

- [ ] **Step 2: Run the tests to verify the contract one fails**

```bash
python install.py -a build-only -c release
./build/release/bin/slideio_tests --gtest_filter="CZIImageDriver.reportsConcurrentReadSupport" -v
```

Expected: FAIL — `supportsConcurrentReads()` still returns the base class's
`false`.

- [ ] **Step 3: Replace the stream with a `FileReader`**

In `czislide.hpp`: replace the `std::ifstream m_fileStream;` member with

```cpp
        std::shared_ptr<const FileReader> m_reader;
```

adding `#include "slideio/core/tools/filereader.hpp"` and `<memory>`. Add an
accessor for the scenes:

```cpp
        const std::shared_ptr<const FileReader>& getReader() const { return m_reader; }
```

In `czislide.cpp`:

- In the open path, replace the two-branch `m_fileStream.open(...)` plus
  `m_fileStream.exceptions(...)` with
  `m_reader = std::make_shared<const FileReader>(m_filePath);`. `FileReader`
  handles the wide-path conversion internally, so the `#if defined(WIN32)`
  branch around the open goes away.
- Replace `readBlock` entirely:

```cpp
void CZISlide::readBlock(uint64_t pos, uint64_t size,
                         std::vector<unsigned char>& data) const {
    data.resize(size);
    m_reader->readAt(pos, data.data(), size);
}
```

  Mark it `const` in the header too. Note what disappears with it: the old
  `catch` block did `m_fileStream.clear(); m_fileStream.seekg(0); throw ex;` —
  error recovery by mutating shared state, which is precisely what is being
  removed, and `throw ex;` sliced the exception to `std::exception`. There is
  no catch block in the replacement; `FileReader::readAt` throws
  `RuntimeError` and it propagates.
- Convert the three `init()`-time readers — `readFileHeader`, `readDirectory`
  / `readMetadata`, and `readAttachments` — to a local
  `SequentialReader reader(*m_reader);`, replacing `m_fileStream.seekg(p)`
  with `reader.setPos(p)`, `m_fileStream.read((char*)&x, sizeof(x))` with
  `reader.read(x)`, `m_fileStream.tellg()` with `reader.pos()`, and dropping
  the `checkStream(...)` calls (a short read now throws from inside
  `readAt`). Include `sequentialreader.hpp`.

- [ ] **Step 4: Give `CZIScene` the reader directly and opt in**

In `cziscene.hpp`, add

```cpp
        bool supportsConcurrentReads() const override { return true; }
```

and a member

```cpp
        std::shared_ptr<const FileReader> m_reader;
```

In `cziscene.cpp`, set `m_reader = slide->getReader();` wherever the scene is
initialised from its slide, and change the tile read to call
`m_reader->readAt(...)` — or a small private `readBlock` on the scene wrapping
it — instead of `m_slide->readBlock(...)`.

Why this and not just leaving the call on `m_slide`: `CZIScene::m_slide` is a
raw `CZISlide*`, so a scene outliving its slide is already a dangling read on
every tile. Holding the `shared_ptr<const FileReader>` removes the
back-pointer from the read path and fixes that lifetime at the same time.
Leave `m_slide` in place for the metadata it still serves.

- [ ] **Step 5: Run the tests to verify they pass**

```bash
python install.py -a build-only -c release
./build/release/bin/slideio_tests --gtest_filter="CZIImageDriver.*" -v
./build/release/bin/slideio_tests --gtest_filter="*CZI*:*czi*" -v
./build/release/bin/slideio_tests
./build/release/bin/slideio_converter_tests
```

Expected: PASS. The whole `slideio_tests` run matters: CZI has a large test
surface and the `init()` conversion touches every header-parsing path.

- [ ] **Step 6: Run the driver test under ThreadSanitizer**

```bash
CXXFLAGS="-fsanitize=thread -g -O1" CFLAGS="-fsanitize=thread -g -O1" \
LDFLAGS="-fsanitize=thread" python install.py -a install -c release -bd build-tsan
./build-tsan/release/bin/slideio_tests --gtest_filter="CZIImageDriver.concurrentReadsAreByteIdentical"
```

Expected: PASS with no `WARNING: ThreadSanitizer` output. This is the gate from
Task 7 Step 3 and it must be green before committing.

- [ ] **Step 7: Commit**

```bash
git add src/slideio/drivers/czi src/tests/main/test_czi_driver.cpp
git commit -m "make CZI reads concurrent via positional I/O"
```

---

## Task 9: Convert SVS and PHTIFF

Spec §4.5.1. One task because `PHTIFFTiledScene` derives from `SVSTiledScene`
and both reach the handle through `SVSScene::getFileHandle()`; they cannot be
separated.

**Files:**
- Modify: `src/slideio/drivers/svs/svsscene.hpp`, `svsscene.cpp`
- Modify: `src/slideio/drivers/svs/svstiledscene.cpp`, `svssmallscene.cpp`
- Modify: `src/slideio/drivers/svs/phtifftiledscene.cpp` (and its header if it declares a userData struct)
- Test: `src/tests/main/test_svs_driver.cpp`, `src/tests/phtiff/test_phtiff_driver.cpp`

**Interfaces:**
- Consumes: `slideio::ContextPool`, `slideio::ReadContext` (Task 5);
  `TestTools::concurrentReadIdentityTest` (Task 6).
- Produces:
  - `class slideio::SVSReadContext : public slideio::ReadContext` with a public `TIFFKeeper keeper;`
  - `ContextPool::Borrow SVSScene::acquireContext()` replacing `libtiff::TIFF* SVSScene::getFileHandle()`
  - `SVSScene::supportsConcurrentReads() == true`

- [ ] **Step 1: Write the failing test**

In `src/tests/main/test_svs_driver.cpp`, add:

```cpp
TEST(SVSImageDriver, reportsConcurrentReadSupport) {
    const std::string filePath = TestTools::getTestImagePath("svs", "JP2K-33003-1.svs");
    SLIDEIO_SKIP_IF_IMAGE_MISSING(filePath);
    slideio::SVSImageDriver driver;
    auto slide = driver.openFile(filePath);
    ASSERT_TRUE(slide);
    auto scene = slide->getScene(0);
    ASSERT_TRUE(scene);
    EXPECT_TRUE(scene->supportsConcurrentReads());
}

// Closing the slide while readers were active must release every descriptor.
// On Windows a retained handle shows up as a file that cannot be deleted, which
// is what isFileHeldOpen checks.
TEST(SVSImageDriver, closingReleasesEveryDescriptor) {
    const std::string source = TestTools::getTestImagePath("svs", "JP2K-33003-1.svs");
    SLIDEIO_SKIP_IF_IMAGE_MISSING(source);
    const std::string copy =
        (std::filesystem::temp_directory_path() / "slideio_svs_close.svs").string();
    std::filesystem::copy_file(source, copy,
                               std::filesystem::copy_options::overwrite_existing);
    {
        slideio::SVSImageDriver driver;
        TestTools::concurrentReadIdentityTest(copy, driver, 0, 4, 8, 2);
    }
    EXPECT_FALSE(TestTools::isFileHeldOpen(copy))
        << "a context's TIFF handle outlived the scene that owned it";
    std::error_code ignored;
    std::filesystem::remove(copy, ignored);
}
```

and in `src/tests/phtiff/test_phtiff_driver.cpp`, next to the existing
`multiThreadedTest` call:

```cpp
TEST(PHTIFFImageDriver, concurrentReadsAreByteIdentical) {
    const std::string filePath = TestTools::getTestImagePath("philips", ph2::FILE_NAME);
    SLIDEIO_SKIP_IF_IMAGE_MISSING(filePath);
    slideio::PHTIFFImageDriver driver;
    TestTools::concurrentReadIdentityTest(filePath, driver);
}
```

Match the driver class name and test-suite naming already used in that file.
Add `#include <filesystem>` to the SVS test file if absent.

- [ ] **Step 2: Run the tests to verify they fail**

```bash
python install.py -a build-only -c release
./build/release/bin/slideio_tests --gtest_filter="SVSImageDriver.reportsConcurrentReadSupport" -v
```

Expected: FAIL — still `false`.

- [ ] **Step 3: Add the context type and the pool**

In `src/slideio/drivers/svs/svsscene.hpp`:

```cpp
#include "slideio/core/tools/contextpool.hpp"
#include "slideio/core/tools/readcontext.hpp"

namespace slideio
{
    /// One libtiff handle. A fresh handle is cheap here because
    /// TiffTools::setCurrentDirectory positions with TIFFSetSubDirectory(offset),
    /// so it jumps straight to the right IFD with no directory walk and no
    /// re-parse of the pyramid -- a context duplicates the descriptor, not the
    /// parsed model.
    class SVSReadContext : public ReadContext
    {
    public:
        explicit SVSReadContext(const std::string& filePath)
            : keeper(TiffTools::openTiffFile(filePath)) {
            if (!keeper.isValid()) {
                RAISE_RUNTIME_ERROR << "SVSImageDriver: cannot open file " << filePath;
            }
        }
        TIFFKeeper keeper;
    };
}
```

Replace the `TIFFKeeper m_tiffKeeper;` member and the
`libtiff::TIFF* getFileHandle();` declaration with:

```cpp
    public:
        bool supportsConcurrentReads() const override { return true; }
    protected:
        /// Borrows a handle for the duration of one block read. Acquire once per
        /// read and pass the context down through userData -- never re-acquire
        /// mid-read.
        ContextPool::Borrow acquireContext() { return m_contextPool.acquire(); }
    private:
        ContextPool m_contextPool;
```

initialising the pool in the `SVSScene` constructor:

```cpp
    m_contextPool([filePath = m_filePath]() {
        return std::make_unique<SVSReadContext>(filePath);
    })
```

In `svsscene.cpp`, delete `getFileHandle()` and `makeSureFileIsOpened()`
entirely. The latter lazily `reset` the keeper, which was itself a race — two
threads could both find it invalid and both open — and pool acquisition
subsumes it.

- [ ] **Step 4: Thread the borrow through the read paths**

In `svstiledscene.cpp`: acquire once in `readResampledBlockChannelsEx`, put the
context pointer in the driver's userData struct, and have `readTile` and
`getTileRect` read the handle from it:

```cpp
void SVSTiledScene::readResampledBlockChannelsEx(...) {
    auto borrow = acquireContext();
    SVSUserData userData;                       // whatever the existing struct is
    userData.context = &borrow.as<SVSReadContext>();
    ...
    TileComposer::composeRect(this, channelIndices, blockRect, blockSize, output, &userData);
}

bool SVSTiledScene::readTile(int tileIndex, const std::vector<int>& channelIndices,
                             cv::OutputArray tileRaster, void* userData) {
    auto* data = static_cast<SVSUserData*>(userData);
    libtiff::TIFF* hFile = data->context->keeper.getHandle();
    ...
}
```

Add the `SVSReadContext* context = nullptr;` field to that struct. If the
driver currently has no userData struct of its own, add one carrying the
existing `TilerData` fields plus the context pointer.

In `svssmallscene.cpp`: the single read path acquires a borrow locally and uses
`borrow.as<SVSReadContext>().keeper.getHandle()` in place of `getFileHandle()`.

In `phtifftiledscene.cpp`: same treatment. `PHTIFFTiledScene` inherits
`acquireContext()`, so only the `getFileHandle()` call sites change.

- [ ] **Step 5: Run the tests to verify they pass**

```bash
python install.py -a build-only -c release
./build/release/bin/slideio_tests --gtest_filter="SVSImageDriver.*" -v
./build/release/bin/slideio_phtiff_tests
./build/release/bin/slideio_tests --gtest_filter="*AFI*" -v
./build/release/bin/slideio_tests
./build/release/bin/slideio_converter_tests
```

Expected: PASS. The AFI filter matters — AFI scenes *are* SVS scenes, so AFI
inherits this conversion and its tests are the check that nothing about scene
construction broke.

- [ ] **Step 6: Run the driver tests under ThreadSanitizer**

```bash
CXXFLAGS="-fsanitize=thread -g -O1" CFLAGS="-fsanitize=thread -g -O1" \
LDFLAGS="-fsanitize=thread" python install.py -a install -c release -bd build-tsan
./build-tsan/release/bin/slideio_tests --gtest_filter="SVSImageDriver.concurrentReadsAreByteIdentical"
./build-tsan/release/bin/slideio_phtiff_tests --gtest_filter="*concurrentReads*"
```

Expected: PASS with no ThreadSanitizer warnings.

- [ ] **Step 7: Commit**

```bash
git add src/slideio/drivers/svs src/tests/main/test_svs_driver.cpp \
        src/tests/phtiff/test_phtiff_driver.cpp
git commit -m "make SVS and PHTIFF reads concurrent via a TIFF handle pool"
```

---

## Task 10: Convert PKE

Spec §4.5.1. Same pattern as Task 9. The specific hazard here:
`PKETiledScene::readTile` calls `getFileHandle()` up to six times in one
operation, so the borrow must be hoisted or a thread could use different
handles inside one tile.

**Files:**
- Modify: `src/slideio/drivers/pke/pkescene.hpp`, `pkescene.cpp`
- Modify: `src/slideio/drivers/pke/pketiledscene.cpp`, `pkesmallscene.cpp`
- Test: `src/tests/pke/test_pke_driver.cpp`

**Interfaces:**
- Consumes: `ContextPool`, `ReadContext` (Task 5); the harness (Task 6).
- Produces: `class slideio::PKEReadContext : public ReadContext` with `TIFFKeeper keeper;`;
  `ContextPool::Borrow PKEScene::acquireContext()`; `PKEScene::supportsConcurrentReads() == true`.

- [ ] **Step 1: Write the failing test**

In `src/tests/pke/test_pke_driver.cpp`:

```cpp
TEST_F(PKEImageDriverTests, concurrentReadsAreByteIdentical) {
    std::string filePath = TestTools::getTestImagePath("pke", "openmicroscopy/PKI_scans/LuCa-7color_Scan1.qptiff");
    SLIDEIO_SKIP_IF_IMAGE_MISSING(filePath);
    slideio::PKEImageDriver driver;
    TestTools::concurrentReadIdentityTest(filePath, driver);
}

TEST_F(PKEImageDriverTests, reportsConcurrentReadSupport) {
    std::string filePath = TestTools::getTestImagePath("pke", "openmicroscopy/PKI_scans/LuCa-7color_Scan1.qptiff");
    SLIDEIO_SKIP_IF_IMAGE_MISSING(filePath);
    slideio::PKEImageDriver driver;
    auto slide = driver.openFile(filePath);
    ASSERT_TRUE(slide);
    auto scene = slide->getScene(0);
    ASSERT_TRUE(scene);
    EXPECT_TRUE(scene->supportsConcurrentReads());
}
```

- [ ] **Step 2: Run the test to verify it fails**

```bash
python install.py -a build-only -c release
./build/release/bin/slideio_pke_tests --gtest_filter="*reportsConcurrentReadSupport*" -v
```

Expected: FAIL — still `false`.

- [ ] **Step 3: Add the context type and pool**

In `src/slideio/drivers/pke/pkescene.hpp`, mirroring Task 9 Step 3:

```cpp
namespace slideio
{
    class PKEReadContext : public ReadContext
    {
    public:
        explicit PKEReadContext(const std::string& filePath)
            : keeper(TiffTools::openTiffFile(filePath)) {
            if (!keeper.isValid()) {
                RAISE_RUNTIME_ERROR << "PKEImageDriver: cannot open file " << filePath;
            }
        }
        TIFFKeeper keeper;
    };
}
```

Replace the `TIFFKeeper m_tiffKeeper;` member and `getFileHandle()` declaration
with `supportsConcurrentReads()` returning `true`, a protected
`acquireContext()`, and a private `ContextPool m_contextPool;` initialised in
the constructor with a factory capturing the file path. Delete
`PKEScene::getFileHandle()` from `pkescene.cpp`.

- [ ] **Step 4: Hoist the borrow in the read paths**

In `pketiledscene.cpp`, `readResampledBlockChannelsEx` acquires one borrow and
stores `&borrow.as<PKEReadContext>()` in the userData struct (adding the field).
Then in `readTile`, replace **every** `getFileHandle()` — there are several in
one function, across the interleaved, single-channel and per-channel branches
— with one local obtained once at the top:

```cpp
bool PKETiledScene::readTile(int tileIndex, const std::vector<int>& channelIndices,
                             cv::OutputArray tileRaster, void* userData) {
    auto* data = static_cast<PKEUserData*>(userData);
    libtiff::TIFF* hFile = data->context->keeper.getHandle();
    // ... every former getFileHandle() call site now uses hFile
}
```

Do the same in the striped-directory helpers in that file and in
`pkesmallscene.cpp`. Grep `getFileHandle` in the pke directory and confirm it
returns nothing before moving on.

- [ ] **Step 5: Run the tests to verify they pass**

```bash
python install.py -a build-only -c release
./build/release/bin/slideio_pke_tests
./build/release/bin/slideio_tests
```

Expected: PASS.

- [ ] **Step 6: Run the driver test under ThreadSanitizer**

```bash
CXXFLAGS="-fsanitize=thread -g -O1" CFLAGS="-fsanitize=thread -g -O1" \
LDFLAGS="-fsanitize=thread" python install.py -a install -c release -bd build-tsan
./build-tsan/release/bin/slideio_pke_tests --gtest_filter="*concurrentReads*"
```

Expected: PASS with no ThreadSanitizer warnings.

- [ ] **Step 7: Commit**

```bash
git add src/slideio/drivers/pke src/tests/pke/test_pke_driver.cpp
git commit -m "make PKE reads concurrent via a TIFF handle pool"
```

---

## Task 11: Convert SCN

Spec §4.5.1. Same pattern. Two SCN-specific points: `SCNScene` opens its own
handle in `init()` rather than lazily, and the driver's auxiliary scenes carry
the TECH_DEBT #3 landmine described below.

**Files:**
- Modify: `src/slideio/drivers/scn/scnscene.hpp`, `scnscene.cpp`
- Test: `src/tests/main/test_scn_driver.cpp`

**Interfaces:**
- Consumes: `ContextPool`, `ReadContext` (Task 5); the harness (Task 6).
- Produces: `class slideio::SCNReadContext : public ReadContext` with `TIFFKeeper keeper;`;
  `SCNScene::supportsConcurrentReads() == true`.

- [ ] **Step 1: Write the failing test**

In `src/tests/main/test_scn_driver.cpp`:

```cpp
TEST(SCNImageDriver, concurrentReadsAreByteIdentical) {
    std::string filePath = TestTools::getTestImagePath("scn", "ultivue/Leica Aperio Versa 5 channel fluorescent image.scn");
    SLIDEIO_SKIP_IF_IMAGE_MISSING(filePath);
    slideio::SCNImageDriver driver;
    TestTools::concurrentReadIdentityTest(filePath, driver);
}

TEST(SCNImageDriver, reportsConcurrentReadSupport) {
    std::string filePath = TestTools::getTestImagePath("scn", "ultivue/Leica Aperio Versa 5 channel fluorescent image.scn");
    SLIDEIO_SKIP_IF_IMAGE_MISSING(filePath);
    slideio::SCNImageDriver driver;
    auto slide = driver.openFile(filePath);
    ASSERT_TRUE(slide);
    auto scene = slide->getScene(0);
    ASSERT_TRUE(scene);
    EXPECT_TRUE(scene->supportsConcurrentReads());
}
```

- [ ] **Step 2: Run the test to verify it fails**

```bash
python install.py -a build-only -c release
./build/release/bin/slideio_tests --gtest_filter="SCNImageDriver.reportsConcurrentReadSupport" -v
```

Expected: FAIL — still `false`.

- [ ] **Step 3: Add the context type and pool**

In `scnscene.hpp`, add `SCNReadContext` exactly as in Task 10 Step 3 but with
an `SCNImageDriver:` message prefix, replace the `TIFFKeeper m_tiff;` member
and the inline `getFileHandle()` with `supportsConcurrentReads()`,
`acquireContext()` and `ContextPool m_contextPool;`.

In `scnscene.cpp`, delete the `m_tiff.reset(TiffTools::openTiffFile(...))` from
`init()`. Note that `init()` also uses `m_tiff.getHandle()` to
`scanTiffDir` the channel directories; that is single-threaded initialisation,
so give it a local borrow:

```cpp
    auto borrow = m_contextPool.acquire();
    libtiff::TIFF* hFile = borrow.as<SCNReadContext>().keeper.getHandle();
    // ... existing scanTiffDir calls use hFile
```

Then thread the borrow through `readResampledBlockChannelsEx` → userData →
`readTile` and the striped helpers, replacing every `getFileHandle()`.

- [ ] **Step 4: Leave the auxiliary-scene bug alone, deliberately**

Do **not** change how `SCNSlide` constructs `SVSSmallScene`. It passes
`m_tiff.getHandle()` into a parameter declared `bool auxiliary`, so the handle
is converted to `true` and discarded and the small scene opens its own file —
the documented TECH_DEBT #3 bug. That accident is what makes SCN's auxiliary
scenes safe for concurrency.

If a later change fixes #3, it must **drop** the argument, not plumb the
slide's handle into the scene: doing the latter would introduce a shared handle
into a driver this task has just declared concurrent. Add that sentence as a
comment at the construction site in `scnslide.cpp` so the next person to touch
it sees it.

- [ ] **Step 5: Run the tests to verify they pass**

```bash
python install.py -a build-only -c release
./build/release/bin/slideio_tests --gtest_filter="SCNImageDriver.*" -v
./build/release/bin/slideio_tests
./build/release/bin/slideio_converter_tests
```

Expected: PASS.

- [ ] **Step 6: Run the driver test under ThreadSanitizer**

```bash
CXXFLAGS="-fsanitize=thread -g -O1" CFLAGS="-fsanitize=thread -g -O1" \
LDFLAGS="-fsanitize=thread" python install.py -a install -c release -bd build-tsan
./build-tsan/release/bin/slideio_tests --gtest_filter="SCNImageDriver.concurrentReadsAreByteIdentical"
```

Expected: PASS with no ThreadSanitizer warnings.

- [ ] **Step 7: Commit**

```bash
git add src/slideio/drivers/scn src/tests/main/test_scn_driver.cpp
git commit -m "make SCN reads concurrent via a TIFF handle pool"
```

---

## Task 12: Convert `VsiFileScene`'s mechanism, without opting in

Spec §4.5.4. VSI slides carry two kinds of scene: ETS scenes and
`VsiFileScene` (a plain TIFF directory). Converting only one would leave one
VSI slide reporting different guarantees for different scenes — a worse
contract than either answer. So this task converts the TIFF half's *mechanism*
and leaves `supportsConcurrentReads()` returning `false`; Task 13 converts the
ETS half and flips both.

**Files:**
- Modify: `src/slideio/drivers/vsi/vsifilescene.hpp`, `vsifilescene.cpp`
- Test: `src/tests/vsi/test_vsi_driver.cpp`

**Interfaces:**
- Consumes: `ContextPool`, `ReadContext` (Task 5).
- Produces: `class slideio::vsi::VsiTiffReadContext : public ReadContext` with `TIFFKeeper keeper;`;
  `ContextPool::Borrow VsiFileScene::acquireContext()`. **Not**
  `supportsConcurrentReads()` — that comes in Task 13.

- [ ] **Step 1: Write the failing test**

In `src/tests/vsi/test_vsi_driver.cpp`:

```cpp
// Both VSI scene kinds must agree on the contract: a slide that reports
// concurrency for its ETS scenes and not for its TIFF scenes is a worse
// contract than either answer. This test holds through Task 12 (both false)
// and Task 13 (both true).
TEST_F(VSIImageDriverTests, allScenesAgreeOnTheConcurrencyContract) {
    std::string filePath = TestTools::getTestImagePath("vsi", "private/d/STS_G6889_11_1_pHH3.vsi");
    SLIDEIO_SKIP_IF_IMAGE_MISSING(filePath);
    slideio::VSIImageDriver driver;
    auto slide = driver.openFile(filePath);
    ASSERT_TRUE(slide);
    const int numScenes = slide->getNumScenes();
    ASSERT_GE(numScenes, 1);
    const bool first = slide->getScene(0)->supportsConcurrentReads();
    for (int i = 1; i < numScenes; ++i) {
        EXPECT_EQ(slide->getScene(i)->supportsConcurrentReads(), first)
            << "scene " << i << " disagrees with scene 0";
    }
}
```

- [ ] **Step 2: Run the test to verify it passes trivially, then keep it honest**

```bash
python install.py -a build-only -c release
./build/release/bin/slideio_vsi_tests --gtest_filter="*allScenesAgree*" -v
```

Expected: PASS — every scene currently reports `false`. This test is the
guardrail for Task 13, so it passing now is correct; the failing test for this
task's mechanism work is the existing VSI suite continuing to pass after the
handle moves into a pool.

- [ ] **Step 3: Add the context type and pool**

In `vsifilescene.hpp`:

```cpp
namespace slideio
{
    namespace vsi
    {
        class VsiTiffReadContext : public ReadContext
        {
        public:
            explicit VsiTiffReadContext(const std::string& filePath)
                : keeper(TiffTools::openTiffFile(filePath)) {
                if (!keeper.isValid()) {
                    RAISE_RUNTIME_ERROR << "VSIImageDriver: cannot open file " << filePath;
                }
            }
            TIFFKeeper keeper;
        };
    }
}
```

Replace the `TIFFKeeper m_tiff;` member with a `ContextPool m_contextPool;`
initialised in the constructor, and a protected
`ContextPool::Borrow acquireContext() { return m_contextPool.acquire(); }`.
Do **not** add `supportsConcurrentReads()`.

- [ ] **Step 4: Thread the borrow through the read path**

In `vsifilescene.cpp`, acquire one borrow in `readResampledBlockChannelsEx`,
put the context pointer in the userData struct that `getTileCount`,
`getTileRect` and `readTile` receive, and read the handle from it. Replace
every use of the old `m_tiff` member.

- [ ] **Step 5: Run the tests to verify they pass**

```bash
python install.py -a build-only -c release
./build/release/bin/slideio_vsi_tests
./build/release/bin/slideio_tests
```

Expected: PASS. Behaviour is unchanged — the scene still serialises, it just
gets its handle from a pool of one.

- [ ] **Step 6: Commit**

```bash
git add src/slideio/drivers/vsi/vsifilescene.hpp src/slideio/drivers/vsi/vsifilescene.cpp \
        src/tests/vsi/test_vsi_driver.cpp
git commit -m "move VsiFileScene's TIFF handle into a context pool"
```

---

## Task 13: Convert VSI/ETS and opt both VSI scene kinds in

Spec §4.5.4. `EtsFile::readTilePart` has **two** races, and the second is the
one that matters: `m_buffer` is a member scratch vector, it is not a file
handle so looking for file state will not find it, it survives fixing the
stream, and it produces **corrupted tiles rather than a crash**.

**Files:**
- Modify: `src/slideio/drivers/vsi/etsfile.hpp`, `etsfile.cpp`
- Modify: `src/slideio/drivers/vsi/etsfilescene.cpp`
- Modify: `src/slideio/drivers/vsi/vsistream.hpp`, `vsistream.cpp`
- Modify: `src/slideio/drivers/vsi/vsifile.cpp`
- Modify: `src/slideio/drivers/vsi/vsifilescene.hpp` (add the opt-in)
- Test: `src/tests/vsi/test_vsi_driver.cpp`

**Interfaces:**
- Consumes: `FileReader`, `SequentialReader` (Task 4); `ContextPool`,
  `ReadContext` (Task 5); `VsiTiffReadContext` (Task 12); the harness (Task 6).
- Produces:
  - `class slideio::vsi::EtsReadContext : public ReadContext` with `std::vector<uint8_t> buffer;`
  - `void EtsFile::readTilePart(const TileInfo&, EtsReadContext&, cv::OutputArray) const`
  - `EtsFileScene::supportsConcurrentReads() == true` and
    `VsiFileScene::supportsConcurrentReads() == true`

- [ ] **Step 1: Write the failing test**

In `src/tests/vsi/test_vsi_driver.cpp`:

```cpp
TEST_F(VSIImageDriverTests, concurrentReadsAreByteIdenticalOnEveryScene) {
    std::string filePath = TestTools::getTestImagePath("vsi", "private/d/STS_G6889_11_1_pHH3.vsi");
    SLIDEIO_SKIP_IF_IMAGE_MISSING(filePath);
    slideio::VSIImageDriver driver;
    // Every scene, because a VSI slide mixes ETS scenes and TIFF scenes and they
    // take different code paths.
    TestTools::concurrentReadIdentityTestAllScenes(filePath, driver);
}

TEST_F(VSIImageDriverTests, reportsConcurrentReadSupport) {
    std::string filePath = TestTools::getTestImagePath("vsi", "private/d/STS_G6889_11_1_pHH3.vsi");
    SLIDEIO_SKIP_IF_IMAGE_MISSING(filePath);
    slideio::VSIImageDriver driver;
    auto slide = driver.openFile(filePath);
    ASSERT_TRUE(slide);
    EXPECT_TRUE(slide->getScene(0)->supportsConcurrentReads());
}
```

- [ ] **Step 2: Run the tests to verify the contract one fails**

```bash
python install.py -a build-only -c release
./build/release/bin/slideio_vsi_tests --gtest_filter="*reportsConcurrentReadSupport*" -v
```

Expected: FAIL — still `false`.

- [ ] **Step 3: Reimplement `VSIStream` over `FileReader`**

`VSIStream` stays the parsing API — `vsifile.cpp` and `EtsFile::init` use it
heavily at open time — but its cursor becomes local rather than shared. Give it
a `std::shared_ptr<const FileReader>` plus a `uint64_t m_pos`, implementing
`setPos`, `getPos`, `readBytes`, `read<T>`, `readValue<T>` and `skipBytes` over
`readAt`. Keep the existing method names so the call sites in `vsifile.cpp` and
`etsfile.cpp` do not change.

- [ ] **Step 4: Fix both races in `EtsFile`**

In `etsfile.hpp`, add the context and remove the scratch member:

```cpp
namespace slideio
{
    namespace vsi
    {
        /// Scratch memory for one tile decode. Context-owned rather than
        /// thread_local: the buffer then dies with the EtsFile's pool instead of
        /// living for the thread's lifetime, and it is per-scene rather than
        /// per-process.
        class EtsReadContext : public ReadContext
        {
        public:
            std::vector<uint8_t> buffer;
        };
    }
}
```

Delete `std::vector<uint8_t> m_buffer;` and replace
`std::unique_ptr<VSIStream> m_etsStream;` with
`std::shared_ptr<const FileReader> m_reader;` plus a
`ContextPool m_contextPool;` (constructed with
`ContextPool::kUnbounded`, since the context holds no scarce resource — only
scratch memory).

In `etsfile.cpp`:

```cpp
void EtsFile::readTilePart(const TileInfo& tileInfo, EtsReadContext& context,
                           cv::OutputArray tileRaster) const {
    const int ds = CVTools::cvGetDataTypeSize(m_dataType);
    context.buffer.resize(tileInfo.size);
    m_reader->readAt(static_cast<uint64_t>(tileInfo.offset),
                     context.buffer.data(), tileInfo.size);
    tileRaster.create(m_tileSize, CV_MAKETYPE(CVTools::cvTypeFromDataType(m_dataType), 1));
    if (m_compression == slideio::Compression::Uncompressed) {
        const int tileSize = m_tileSize.width * m_tileSize.height * ds;
        std::memcpy(tileRaster.getMat().data, context.buffer.data(), tileSize);
    }
    else if (m_compression == slideio::Compression::Jpeg) {
        ImageTools::decodeJpegStream(context.buffer.data(), context.buffer.size(), tileRaster);
    }
    else if (m_compression == slideio::Compression::Jpeg2000) {
        ImageTools::decodeJp2KStream(context.buffer.data(), context.buffer.size(), tileRaster);
    }
    else {
        RAISE_RUNTIME_ERROR << "VSIImageDriver: readTile: Compression "
            << static_cast<int>(m_compression) << " is not supported";
    }
}
```

Mark both `readTilePart` and `readTile` `const` in the header and the
definition. That is not cosmetic: it makes the compiler find any remaining
member mutation on the read path, which is the cheapest available check that
nothing else was missed.

`EtsFile::init` keeps a cursor — convert it to a local
`SequentialReader reader(*m_reader);` or a local `VSIStream` over the shared
reader, and drop the `m_etsStream` member.

Add `ContextPool::Borrow EtsFile::acquireContext()`, and update `readTile` to
take the context and pass it to both `readTilePart` call sites (the
multi-channel loop and the single-raster branch).

- [ ] **Step 5: Thread the borrow through `EtsFileScene` and opt in**

In `etsfilescene.cpp`, acquire one borrow in `readResampledBlockChannelsEx`,
store the context pointer in `TileComposerUserData`, and pass it into
`EtsFile::readTile`. Add to the scene's header:

```cpp
        bool supportsConcurrentReads() const override { return true; }
```

Add the same override to `VsiFileScene` — its mechanism landed in Task 12, and
this is the point at which both halves agree.

- [ ] **Step 6: Run the tests to verify they pass**

```bash
python install.py -a build-only -c release
./build/release/bin/slideio_vsi_tests
./build/release/bin/slideio_tests
```

Expected: PASS, including `allScenesAgreeOnTheConcurrencyContract` from Task
12, which now holds with both sides `true`.

- [ ] **Step 7: Run the driver tests under ThreadSanitizer**

```bash
CXXFLAGS="-fsanitize=thread -g -O1" CFLAGS="-fsanitize=thread -g -O1" \
LDFLAGS="-fsanitize=thread" python install.py -a install -c release -bd build-tsan
./build-tsan/release/bin/slideio_vsi_tests --gtest_filter="*concurrentReads*"
```

Expected: PASS with no ThreadSanitizer warnings. This is the run that would
have caught `m_buffer`, so if it reports a race on a `std::vector` inside
`readTilePart`, a scratch buffer was missed.

- [ ] **Step 8: Commit**

```bash
git add src/slideio/drivers/vsi src/tests/vsi/test_vsi_driver.cpp
git commit -m "make VSI reads concurrent; move the ETS scratch buffer into a context"
```

---

## Task 14: Convert NDPI

Spec §4.5.2. Same mechanism as the TIFF family, different owner:
`NDPIScene::m_pfile` is a raw pointer to a **shared** `NDPIFile`, so the pool
belongs to `NDPIFile` and descriptor use is bounded per file rather than per
scene.

**Files:**
- Modify: `src/slideio/drivers/ndpi/ndpifile.hpp`, `ndpifile.cpp`
- Modify: `src/slideio/drivers/ndpi/ndpiscene.hpp`, `ndpiscene.cpp`
- Test: `src/tests/ndpi/test_ndpi_driver.cpp`

**Interfaces:**
- Consumes: `ContextPool`, `ReadContext` (Task 5); the harness (Task 6);
  `installNDPITiffMessageHandlers()` (Task 2).
- Produces:
  - `class slideio::NDPIReadContext : public ReadContext` with `NDPITIFFKeeper keeper;`
  - `ContextPool::Borrow NDPIFile::acquireContext()` replacing `libtiff::TIFF* NDPIFile::getTiffHandle()`
  - `NDPIScene::supportsConcurrentReads() == true`

- [ ] **Step 1: Write the failing test**

In `src/tests/ndpi/test_ndpi_driver.cpp`:

```cpp
TEST_F(NDPIImageDriverTests, concurrentReadsAreByteIdentical) {
    std::string filePath = TestTools::getTestImagePath("hamamatsu", "DM0014 - 2020-04-02 11.10.47.ndpi");
    SLIDEIO_SKIP_IF_IMAGE_MISSING(filePath);
    slideio::NDPIImageDriver driver;
    TestTools::concurrentReadIdentityTest(filePath, driver);
}

TEST_F(NDPIImageDriverTests, reportsConcurrentReadSupport) {
    std::string filePath = TestTools::getTestImagePath("hamamatsu", "DM0014 - 2020-04-02 11.10.47.ndpi");
    SLIDEIO_SKIP_IF_IMAGE_MISSING(filePath);
    slideio::NDPIImageDriver driver;
    auto slide = driver.openFile(filePath);
    ASSERT_TRUE(slide);
    auto scene = slide->getScene(0);
    ASSERT_TRUE(scene);
    EXPECT_TRUE(scene->supportsConcurrentReads());
}

// Scenes of one NDPI file share the pool, so opening a second scene must not
// double the descriptor count.
TEST_F(NDPIImageDriverTests, scenesOfOneFileShareTheHandlePool) {
    std::string filePath = TestTools::getTestImagePath("hamamatsu", "DM0014 - 2020-04-02 11.10.47.ndpi");
    SLIDEIO_SKIP_IF_IMAGE_MISSING(filePath);
    slideio::NDPIImageDriver driver;
    auto slide = driver.openFile(filePath);
    ASSERT_TRUE(slide);
    const int numScenes = slide->getNumScenes();
    for (int i = 0; i < numScenes; ++i) {
        cv::Mat raster;
        auto scene = slide->getScene(i);
        const cv::Rect rect = scene->getRect();
        const cv::Size size(std::min(64, rect.width), std::min(64, rect.height));
        scene->readResampledBlockChannels(cv::Rect(rect.x, rect.y, size.width, size.height),
                                          size, {}, raster);
        EXPECT_FALSE(raster.empty());
    }
}
```

- [ ] **Step 2: Run the test to verify it fails**

```bash
python install.py -a build-only -c release
./build/release/bin/slideio_ndpi_tests --gtest_filter="*reportsConcurrentReadSupport*" -v
```

Expected: FAIL — still `false`.

- [ ] **Step 3: Move the handle into a pool on `NDPIFile`**

In `ndpifile.hpp`:

```cpp
namespace slideio
{
    class NDPIReadContext : public ReadContext
    {
    public:
        explicit NDPIReadContext(const std::string& filePath) : keeper(filePath) {
            if (!keeper.isValid()) {
                RAISE_RUNTIME_ERROR << "NDPIImageDriver: cannot open file " << filePath;
            }
        }
        NDPITIFFKeeper keeper;
    };
}
```

Replace `NDPITIFFKeeper m_tiff;` and `libtiff::TIFF* getTiffHandle()` with:

```cpp
        /// Borrows a handle for one block read. The pool lives here rather than
        /// on the scene because the handle does: NDPIScene::m_pfile is a raw
        /// pointer to a shared NDPIFile, so a per-scene pool would multiply
        /// descriptors by scene count.
        ContextPool::Borrow acquireContext() { return m_contextPool.acquire(); }
    private:
        ContextPool m_contextPool;
```

initialised in `NDPIFile::init` (or the constructor, once the path is known)
with a factory capturing `m_filePath`. `NDPIFile::scanFile` runs at open time
and needs a handle — give it a local borrow.

- [ ] **Step 4: Thread the borrow through the scene**

In `ndpiscene.hpp`, add `bool supportsConcurrentReads() const override { return true; }`
and add an `NDPIReadContext* context = nullptr;` field to `NDPIUserData`.

In `ndpiscene.cpp`, acquire one borrow in `readResampledBlockChannelsEx`, set
`userData.context`, and replace every `m_pfile->getTiffHandle()` in
`getTileCount`, `getTileRect` and `readTile` with
`data->context->keeper.getHandle()`. Grep `getTiffHandle` in the ndpi directory
and confirm it returns nothing.

- [ ] **Step 5: Run the tests to verify they pass**

```bash
python install.py -a build-only -c release
./build/release/bin/slideio_ndpi_tests
./build/release/bin/slideio_tests
```

Expected: PASS.

- [ ] **Step 6: Run the driver test under ThreadSanitizer**

```bash
CXXFLAGS="-fsanitize=thread -g -O1" CFLAGS="-fsanitize=thread -g -O1" \
LDFLAGS="-fsanitize=thread" python install.py -a install -c release -bd build-tsan
./build-tsan/release/bin/slideio_ndpi_tests --gtest_filter="*concurrentReads*"
```

Expected: PASS with no ThreadSanitizer warnings.

- [ ] **Step 7: Commit**

```bash
git add src/slideio/drivers/ndpi src/tests/ndpi/test_ndpi_driver.cpp
git commit -m "make NDPI reads concurrent via a per-file TIFF handle pool"
```

---

## Task 15: Documentation

Spec §7. The contract change is invisible at compile time, so the written
record is the only way a caller learns about it.

**Files:**
- Modify: `software-docs/TECH_DEBT.md`
- Modify: `software-docs/BREAKING_CHANGES.md`
- Modify: `CLAUDE.md`
- Modify: `src/slideio/core/refcounter.hpp`
- Modify: `src/tests/main/test_concurrency_contract.cpp`
- Modify: `src/tests/ometiff/` (the OME-TIFF half of the contract-coverage test)

**Interfaces:**
- Consumes: `CVScene::supportsConcurrentReads()` (Task 3) and every conversion.
- Produces: no production code.

- [ ] **Step 1: Update TECH_DEBT #4**

Rewrite the status of `## 4. CVScene serialises every block read, and does so
inconsistently` to record that the inconsistency in `assemble4DBlock` is fixed
outright, and that the serialisation is removed for the scenes of SVS, PHTIFF,
AFI, PKE, SCN, NDPI, CZI and VSI. Keep the entry open, because four drivers
remain serialised, and point at the new entries below. Reference the spec path.

- [ ] **Step 2: Add the constraint to TECH_DEBT #3**

Append to `## 3. SCNSlide passes a TIFF* where SVSSmallScene expects a bool`:

```markdown
**Constraint added 2026-09-07.** SCN scenes now declare concurrent reads
(`software-docs/specs/2026-09-07-parallel-read-block-design.md` §4.5.1). The
handle being silently discarded is what makes SCN's auxiliary scenes safe, so
fixing this must **drop** the argument, not plumb the slide's handle into the
scene -- the latter would put a shared, unsynchronised `TIFF*` back into a
driver that advertises concurrency.
```

- [ ] **Step 3: Add four new TECH_DEBT entries**

One per deferred driver, added to the numbered list and the table of contents.
Each states the mechanism now exists, so the work is bounded:

```markdown
## N. ZVI, DCM, GDAL and OME-TIFF still serialise every block read

**Files:** `src/slideio/drivers/zvi/`, `drivers/dcm/`, `drivers/gdal/`,
`drivers/ome-tiff/`
**Related:** `software-docs/specs/2026-09-07-parallel-read-block-design.md` §3.2
**Status:** Open, deliberately deferred.

These four scene types return `false` from `supportsConcurrentReads()`, so the
base class serialises their reads as it always did. Each was deferred because
its mutable read-path state lives inside a third-party or vendored library
rather than in slideio, and none is in the tiling or converter path.

The work per driver is now bounded: add a `ReadContext` subclass holding the
per-thread object and choose the pool's cap.

- **ZVI** -- the context holds an `ole::compound_document`. `ZVIScene::m_Doc`,
  `ZVIUtils::StreamKeeper` and `ZVIImageItem::readRaster` share mutable cursors
  three layers down in vendored OLE code. A per-thread document costs N x the
  OLE FAT and directory parse, which is small for a ZVI in a way it never is
  for CZI.
- **DCM** -- the context holds a `DCMFile`. `DCMFile::readFrame` builds a fresh
  `DicomImage` per frame from a shared `DcmDataset`, and DCMTK's
  `DcmPixelData` caches decompressed representations inside that dataset. N x
  parse is expensive here: pick the cap accordingly.
- **GDAL** -- the context holds the GDAL dataset. GDAL datasets are not
  re-entrant, and the driver decodes a whole scene per call, so the read
  granularity wants revisiting at the same time.
- **OME-TIFF** -- a different failure class from the other three, and the
  reason it must not be left to chance: `TIFFFiles::getOrOpen` does a `find`
  followed by an insert into a plain `std::unordered_map`. A race there
  corrupts the container -- undefined behaviour, not a bad tile. Either give
  `TIFFFiles` its own lock or move the whole map into a per-thread context.

The alternative to per-thread replicas, for whichever of these ever matters for
throughput: resolve every tile's `(offset, length)` once at `init()` and read
via `FileReader` thereafter, bypassing OLE, DCMTK and GDAL on the read path
entirely. That is faster single-threaded too, but it means owning OLE sector
chains including the mini-FAT for streams under 4096 bytes, and DICOM
encapsulated-pixel-data basic offset tables.
```

Split into four entries or keep as one, following whichever the file's existing
granularity suggests; if split, repeat the shared closing paragraph rather than
cross-referencing, since entries are read individually.

- [ ] **Step 4: Add the `BREAKING_CHANGES.md` entry**

Under the `v2.10.0` heading:

```markdown
### `Scene` block reads may now overlap

Reads of one `Scene` were thread-safe and fully serialised: no two block reads
of one scene ever ran at the same time. For the scenes of SVS, PHTIFF, AFI,
PKE, SCN, NDPI, CZI and VSI they now run concurrently. ZVI, DCM, GDAL and
OME-TIFF are unchanged; `CVScene::supportsConcurrentReads()` reports which
applies.

No signatures changed and there is no source or binary incompatibility. The
break is behavioural: code that relied on reads of one scene being mutually
exclusive in order to protect **its own** state must now take its own lock.

See `software-docs/specs/2026-09-07-parallel-read-block-design.md`.
```

- [ ] **Step 5: Add the contract to `CLAUDE.md`**

In the **Key Design Patterns** list:

```markdown
- **Concurrency contract**: `CVScene::supportsConcurrentReads()` says whether
  two block reads of one scene may overlap. It defaults to `false`, and the
  base class serialises reads for any scene that does not override it, so a new
  driver is safe by construction. A driver overrides it only once every mutable
  object on its read path is either cursor-free (`FileReader`) or per-thread
  (`ContextPool`, which hands out `ReadContext` subclasses one borrower at a
  time). Use `ContextPool` for per-thread read state rather than inventing a
  second mechanism, and never `thread_local` for anything holding a file
  handle. Concurrent today: SVS, PHTIFF, AFI, PKE, SCN, NDPI, CZI, VSI.
```

- [ ] **Step 6: Add the `RefCounter` warning**

In `src/slideio/core/refcounter.hpp`, above the two hooks:

```cpp
    // initializeCounter() and cleanCounter() fire on the 0->1 and 1->0
    // transitions. Nothing overrides them today, and nothing should use them for
    // file handle lifecycle: block reads of one scene may now overlap, so the
    // count oscillates through zero and a driver opening on 0->1 and closing on
    // 1->0 would reopen the file repeatedly and race a close against another
    // thread's open. Per-thread read state belongs in a ReadContext.
```

- [ ] **Step 7: Add the contract-coverage test**

Spec §6's last gate: the §3.2 exemptions must not silently drift. The
per-driver tasks each assert their own driver reports `true`; this asserts the
four deferred ones still report `false`, so a future conversion cannot forget
to update the documentation above.

Add to `src/tests/main/test_concurrency_contract.cpp` (created in Task 3):

```cpp
#include "slideio/drivers/zvi/zviimagedriver.hpp"
#include "slideio/drivers/dcm/dcmimagedriver.hpp"
#include "slideio/drivers/gdal/gdalimagedriver.hpp"
#include "testlib/testtools.hpp"

namespace
{
    void expectSerialised(const std::string& filePath, slideio::ImageDriver& driver) {
        auto slide = driver.openFile(filePath);
        ASSERT_TRUE(slide);
        auto scene = slide->getScene(0);
        ASSERT_TRUE(scene);
        EXPECT_FALSE(scene->supportsConcurrentReads())
            << "this driver now reports concurrent reads -- update TECH_DEBT, "
               "BREAKING_CHANGES.md and CLAUDE.md, and give it a byte-exactness "
               "test, before changing this expectation";
    }
}

TEST(ConcurrencyContract, zviIsStillSerialised) {
    const std::string filePath = TestTools::getTestImagePath("zvi", "Zeiss-1-Stacked.zvi");
    SLIDEIO_SKIP_IF_IMAGE_MISSING(filePath);
    slideio::ZVIImageDriver driver;
    expectSerialised(filePath, driver);
}

TEST(ConcurrencyContract, gdalIsStillSerialised) {
    const std::string filePath = TestTools::getTestImagePath("gdal", "test.svs");
    SLIDEIO_SKIP_IF_IMAGE_MISSING(filePath);
    slideio::GDALImageDriver driver;
    expectSerialised(filePath, driver);
}
```

Take the exact image paths from the existing `multiThreadedTest` call sites in
`test_zvi_driver.cpp` and `test_gdal_driver.cpp` rather than the names above,
which are illustrative. Add the DCM and OME-TIFF equivalents the same way —
DCM's from `test_dcm_driver.cpp`, and OME-TIFF's in
`src/tests/ometiff/` since that driver has its own suite and its
`ImageDriver` is not linked into `slideio_tests`.

Run them:

```bash
python install.py -a build-only -c release
./build/release/bin/slideio_tests --gtest_filter="ConcurrencyContract.*" -v
./build/release/bin/slideio_ometiff_tests --gtest_filter="*Serialised*" -v
```

Expected: PASS.

- [ ] **Step 8: Verify the whole suite and commit**

```bash
python install.py -a build-only -c release
./build/release/bin/slideio_tests
./build/release/bin/slideio_converter_tests
./build/release/bin/slideio_transformer_tests
./build/release/bin/slideio_ndpi_tests
./build/release/bin/slideio_vsi_tests
./build/release/bin/slideio_pke_tests
./build/release/bin/slideio_ometiff_tests
./build/release/bin/slideio_phtiff_tests
```

Expected: PASS everywhere.

```bash
git add software-docs/TECH_DEBT.md software-docs/BREAKING_CHANGES.md CLAUDE.md \
        src/slideio/core/refcounter.hpp \
        src/tests/main/test_concurrency_contract.cpp src/tests/ometiff
git commit -m "document the concurrent-read contract and the deferred drivers"
```

---

## Dependencies between tasks

```
1 (libtiff handlers) ──┐
2 (NDPI handlers) ─────┤
3 (contract) ──────────┼──> 8  (CZI)
4 (FileReader) ────────┤     9  (SVS + PHTIFF)
5 (ContextPool) ───────┤     10 (PKE)
6 (harness) ───────────┤     11 (SCN)
7 (TSan CI) ───────────┘     12 (VsiFileScene mechanism) ──> 13 (VSI/ETS + opt-in)
                             14 (NDPI)
                                     └──> 15 (documentation)
```

Tasks 1–7 are prerequisites and must land in that order for 4→5 and 6→7; 1, 2
and 3 are independent of each other. Tasks 8, 9, 10, 11 and 14 are independent
of each other and can be done in any order or in parallel. Task 13 requires
Task 12. Task 15 requires every conversion, because it lists which formats are
concurrent.
