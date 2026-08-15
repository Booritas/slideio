# TIFFKeeper Ownership Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Make `TIFFKeeper` a move-only owning handle that never silently drops or double-closes a `libtiff::TIFF*`.

**Architecture:** Tests for the ownership contract land first, against the class as it is, so the defects are demonstrated before they are fixed. The class then becomes move-only with `reset()`/`release()`, the implicit conversion and the macro go, and the call sites the change breaks are migrated. Every breaking change is a compile error by construction.

**Tech Stack:** C++17, libtiff, OpenCV, GoogleTest, CMake, Conan.

**Spec:** `software-docs/specs/2026-08-15-tiffkeeper-ownership-design.md`

## Global Constraints

- Build: `python install.py -a build-only -c release` from the repo root. Run a suite: `./build/bin/Release/<name>.exe` (Windows) or `./build/release/bin/<name>` (Linux/macOS).
- Baseline: `slideio_tests` 490, `slideio_phtiff_tests` 103, `slideio_pke_tests` 15, `slideio_ometiff_tests` 98, `slideio_ndpi_tests` 29, `slideio_vsi_tests` 30, `slideio_converter_tests` 140, `slideio_transformer_tests` 39. This plan ADDS tests to `slideio_tests`; no existing test may change its assertions.
- **If the build reports an accidental COPY of a `TIFFKeeper`, stop and report it.** That is a live double-close this change has just found. Do not reinstate copying, and do not convert the copy to a move without saying so — the copy's presence means some object's lifetime is not what its author thought.
- C++17. Every file keeps its three-line licence header. Comments explain *why*.
- `NDPITIFFKeeper` in `src/slideio/drivers/ndpi/ndpitifftools.hpp` is OUT OF SCOPE. It is a near-twin with the same defects and stays as it is.

---

## File Map

**Modify:**
- `src/slideio/imagetools/tiffkeeper.hpp` — the class: move-only, `reset()`, no implicit conversion, alias instead of macro, `<memory>`/`<cstdint>` included directly.
- `src/slideio/imagetools/tiffkeeper.cpp` — move operations, `reset()`, shared constructor init, `openTiffFile` routed through `reset()`.
- Five `reset()` migrations: `src/slideio/drivers/pke/pkescene.cpp:40`, `src/slideio/drivers/scn/scnscene.cpp:232`, `src/slideio/drivers/scn/scnslide.cpp:25`, `src/slideio/drivers/svs/svsscene.cpp:40`, `src/slideio/drivers/vsi/vsifilescene.cpp:73`.
- Five conversion migrations: `src/slideio/drivers/scn/scnscene.cpp:211`, `src/slideio/drivers/scn/scnslide.cpp:29`, `src/slideio/drivers/vsi/vsifilescene.cpp:29`, `src/slideio/drivers/pke/pkescene.cpp:50`, `src/slideio/drivers/svs/svsscene.cpp:50`.
- `src/slideio/converter/tiffconverter.hpp:161` — the alias instead of the macro.
- `src/tests/main/test_fiwrapper.cpp:106,171` — direct initialisation.
- `src/tests/main/CMakeLists.txt` — the new test file.

**Create:**
- `src/tests/main/test_tiffkeeper.cpp` — the ownership contract suite.

**Not touched:** `ndpitifftools.hpp`, the converter's `shared_ptr<TIFFKeeper>` indirection, and every use of `getHandle()` that is already explicit.

---

## Task 1: Test the ownership contract as it stands

Tests first, against the unmodified class, so the defects are demonstrated rather than asserted. Two of these tests will FAIL against the class as it stands, and two more cannot compile until Task 2 adds `reset()`. The failures are the bug reports.

**Files:**
- Create: `src/tests/main/test_tiffkeeper.cpp`
- Modify: `src/tests/main/CMakeLists.txt`

**Interfaces:**
- Produces: `slideio_tests` grows by 7 tests, 490 → 497.

- [ ] **Step 1: Write the suite**

Ownership is observable rather than inferred: libtiff opens without `FILE_SHARE_DELETE`, so on Windows an open handle keeps the file undeletable. `canDelete` below is how each test asks "was it closed?". The same technique covers the Philips open-failure leak in `test_phtiff_driver.cpp`.

```cpp
// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#include <gtest/gtest.h>
#include "slideio/imagetools/tiffkeeper.hpp"
#include "slideio/imagetools/tifftools.hpp"
#include "tests/testlib/testtools.hpp"
#include <filesystem>
#include <string>

using namespace slideio;

namespace
{
    // A private copy of a real tiff, so a test can assert the handle was closed by
    // deleting the file. libtiff opens without FILE_SHARE_DELETE, so a file with a live
    // handle cannot be removed -- which makes "closed" observable rather than assumed.
    std::filesystem::path copyTestTiff(const char* name) {
        const std::string source = TestTools::getTestImagePath("svs", "CMU-1-Small-Region-page-1.tif");
        const std::filesystem::path copy = std::filesystem::temp_directory_path() / name;
        std::error_code ignored;
        std::filesystem::remove(copy, ignored);
        std::filesystem::copy_file(source, copy);
        return copy;
    }

    bool canDelete(const std::filesystem::path& path) {
        std::error_code error;
        return std::filesystem::remove(path, error);
    }
}

TEST(TIFFKeeper, destructorClosesTheHandle) {
    const std::filesystem::path file = copyTestTiff("tiffkeeper-dtor.tif");
    {
        TIFFKeeper keeper(TiffTools::openTiffFile(file.string()));
        ASSERT_TRUE(keeper.isValid());
        EXPECT_FALSE(canDelete(file)) << "the handle is open, so the file is locked";
    }
    EXPECT_TRUE(canDelete(file)) << "the destructor must close the handle";
}

TEST(TIFFKeeper, releaseGivesUpOwnershipWithoutClosing) {
    const std::filesystem::path file = copyTestTiff("tiffkeeper-release.tif");
    libtiff::TIFF* handle = nullptr;
    {
        TIFFKeeper keeper(TiffTools::openTiffFile(file.string()));
        handle = keeper.release();
        EXPECT_FALSE(keeper.isValid()) << "release leaves the keeper empty";
        EXPECT_TRUE(handle != nullptr);
    }
    EXPECT_FALSE(canDelete(file)) << "release must NOT close: the caller owns it now";
    TiffTools::closeTiffFile(handle);
    EXPECT_TRUE(canDelete(file));
}

// The defect the debt entry names: assigning a new handle over a held one dropped the
// old handle without closing it.
TEST(TIFFKeeper, resetClosesTheHandleItReplaces) {
    const std::filesystem::path first = copyTestTiff("tiffkeeper-reset-first.tif");
    const std::filesystem::path second = copyTestTiff("tiffkeeper-reset-second.tif");
    {
        TIFFKeeper keeper(TiffTools::openTiffFile(first.string()));
        keeper.reset(TiffTools::openTiffFile(second.string()));
        EXPECT_TRUE(keeper.isValid());
        EXPECT_TRUE(canDelete(first)) << "reset must close the handle it replaced";
    }
    EXPECT_TRUE(canDelete(second));
}

// The same defect by its other door, which the debt entry does not record.
TEST(TIFFKeeper, openTiffFileClosesTheHandleItReplaces) {
    const std::filesystem::path first = copyTestTiff("tiffkeeper-open-first.tif");
    const std::filesystem::path second = copyTestTiff("tiffkeeper-open-second.tif");
    {
        TIFFKeeper keeper(TiffTools::openTiffFile(first.string()));
        keeper.openTiffFile(second.string(), true);
        EXPECT_TRUE(keeper.isValid());
        EXPECT_TRUE(canDelete(first)) << "openTiffFile must close the handle it replaced";
    }
    EXPECT_TRUE(canDelete(second));
}

// The empty case, which the close-first logic must not get wrong: resetting a keeper
// that holds nothing is just taking ownership, with nothing to close.
TEST(TIFFKeeper, resetOnAnEmptyKeeperTakesOwnership) {
    const std::filesystem::path file = copyTestTiff("tiffkeeper-reset-empty.tif");
    {
        TIFFKeeper keeper;
        EXPECT_FALSE(keeper.isValid());
        keeper.reset(TiffTools::openTiffFile(file.string()));
        EXPECT_TRUE(keeper.isValid());
        EXPECT_FALSE(canDelete(file)) << "the keeper holds it open";
    }
    EXPECT_TRUE(canDelete(file));
}

// TIFFKeeper is move-only by design, so copying one is a compile error rather than a
// test: an owning handle with two owners closes twice. There is no test here because a
// test that must fail to compile is not worth the build machinery -- the deleted copy
// constructor in tiffkeeper.hpp is the guarantee.

TEST(TIFFKeeper, moveConstructionTransfersOwnership) {
    const std::filesystem::path file = copyTestTiff("tiffkeeper-move-ctor.tif");
    {
        TIFFKeeper source(TiffTools::openTiffFile(file.string()));
        TIFFKeeper target(std::move(source));
        EXPECT_TRUE(target.isValid());
        EXPECT_FALSE(source.isValid()) << "a moved-from keeper owns nothing";
        EXPECT_FALSE(canDelete(file)) << "the target still holds it open";
    }
    EXPECT_TRUE(canDelete(file)) << "one close, by the target";
}

TEST(TIFFKeeper, moveAssignmentClosesItsOwnHandleFirst) {
    const std::filesystem::path own = copyTestTiff("tiffkeeper-move-own.tif");
    const std::filesystem::path taken = copyTestTiff("tiffkeeper-move-taken.tif");
    {
        TIFFKeeper target(TiffTools::openTiffFile(own.string()));
        TIFFKeeper source(TiffTools::openTiffFile(taken.string()));
        target = std::move(source);
        EXPECT_TRUE(canDelete(own)) << "move assignment must close what it gives up";
        EXPECT_FALSE(source.isValid());
    }
    EXPECT_TRUE(canDelete(taken));
}
```

Note the constructions are direct-initialisation (`TIFFKeeper keeper(...)`), which keeps compiling both before and after the constructor becomes `explicit` in Task 2.

- [ ] **Step 2: Add the file to the test target**

In `src/tests/main/CMakeLists.txt`, add `test_tiffkeeper.cpp` to the `TEST_SOURCES` list, next to `test_tifffiles.cpp`.

- [ ] **Step 3: Build**

Run: `python install.py -a build-only -c release`
Expected: clean. `reset()` does not exist yet, so `resetClosesTheHandleItReplaces` and `resetOnAnEmptyKeeperTakesOwnership` will not compile — comment out ONLY those two test bodies with a `// Task 2 adds reset()` marker, build, and restore them in Task 2. Leave the other five tests live.

- [ ] **Step 4: Run and record which fail**

Run: `./build/bin/Release/slideio_tests.exe --gtest_filter="TIFFKeeper.*"`
Expected: `destructorClosesTheHandle`, `releaseGivesUpOwnershipWithoutClosing` and `moveConstructionTransfersOwnership` PASS — the last only because copy-construction happens to do the same thing a move would, which is exactly the confusion this change removes.
`openTiffFileClosesTheHandleItReplaces` FAILS: the first file cannot be deleted, because the handle leaked.
`moveAssignmentClosesItsOwnHandleFirst` FAILS to compile OR fails at runtime, depending on how the implicit copy-assignment resolves — record which, verbatim, in your report.

Write down exactly which tests failed and how. These failures are the defect reports the rest of the plan fixes.

- [ ] **Step 5: Commit**

```bash
git add src/tests/main/test_tiffkeeper.cpp src/tests/main/CMakeLists.txt
git commit -m "test the TIFFKeeper ownership contract before fixing it"
```

Commit with the failing tests present is deliberate: the next task's commit is what turns them green, and the pair is the evidence. If your harness refuses to commit a red suite, note that in your report and commit Tasks 1 and 2 together instead.

---

## Task 2: Make TIFFKeeper move-only

**Files:**
- Modify: `src/slideio/imagetools/tiffkeeper.hpp`, `src/slideio/imagetools/tiffkeeper.cpp`
- Modify: `src/tests/main/test_tiffkeeper.cpp` (restore the commented test)

**Interfaces:**
- Consumes: the suite from Task 1.
- Produces: `TIFFKeeper` with `explicit` constructors, deleted copy, `TIFFKeeper(TIFFKeeper&&) noexcept`, `TIFFKeeper& operator=(TIFFKeeper&&) noexcept`, `void reset(libtiff::TIFF* hFile = nullptr)`, `libtiff::TIFF* release()`, no `operator libtiff::TIFF*()`, no `operator=(libtiff::TIFF*)`, and `using TIFFKeeperPtr = std::shared_ptr<TIFFKeeper>;` replacing the macro.

- [ ] **Step 1: Rewrite the header's public surface**

In `tiffkeeper.hpp`, add `#include <memory>` and `#include <cstdint>` beside the existing includes, and replace the constructor/operator block:

```cpp
    public:
        explicit TIFFKeeper(libtiff::TIFF* hFile = nullptr);
        explicit TIFFKeeper(const std::string& filePath, bool readOnly = true);
        ~TIFFKeeper();

        // An owning handle must not be copied: two owners means two closes, and the
        // second one operates on a pointer libtiff has already freed.
        TIFFKeeper(const TIFFKeeper&)            = delete;
        TIFFKeeper& operator=(const TIFFKeeper&) = delete;
        TIFFKeeper(TIFFKeeper&& other) noexcept;
        TIFFKeeper& operator=(TIFFKeeper&& other) noexcept;

        libtiff::TIFF* getHandle() const {
            return m_hFile;
        }
        bool isValid() const {
            return m_hFile != nullptr;
        }
        // Takes ownership of a raw handle, closing any handle already held. Replaces the
        // old operator=(TIFF*), which overwrote the member and leaked what it replaced.
        void reset(libtiff::TIFF* hFile = nullptr);
        // Gives up ownership without closing: the caller closes it from here on.
        libtiff::TIFF* release();
```

Delete `operator libtiff::TIFF*()` and `operator=(libtiff::TIFF*)` entirely, and delete the inline body of `release()` (it moves to the `.cpp` beside `reset()`). Leave every forwarding method as it is. Give the member its initialiser: `libtiff::TIFF* m_hFile = nullptr;`.

At the bottom of the file, replace the macro with the alias, inside the namespace:

```cpp
    using TIFFKeeperPtr = std::shared_ptr<TIFFKeeper>;
}
```
so the `#define TIFFKeeperPtr ...` line after the closing brace is gone.

- [ ] **Step 2: Implement in the .cpp**

In `tiffkeeper.cpp`, replace the two constructors and add the new operations:

```cpp
TIFFKeeper::TIFFKeeper(libtiff::TIFF* hFile) : m_hFile(hFile)
{
    m_messageHandler = std::make_shared<TIFFMessageHandler>();
}

TIFFKeeper::TIFFKeeper(const std::string& filePath, bool readOnly)
{
    m_messageHandler = std::make_shared<TIFFMessageHandler>();
    openTiffFile(filePath, readOnly);
}

TIFFKeeper::TIFFKeeper(TIFFKeeper&& other) noexcept
    : m_hFile(other.m_hFile), m_messageHandler(std::move(other.m_messageHandler))
{
    other.m_hFile = nullptr;
}

TIFFKeeper& TIFFKeeper::operator=(TIFFKeeper&& other) noexcept
{
    if (this != &other) {
        // Close what we own before taking what they own, or ours leaks.
        reset(other.m_hFile);
        other.m_hFile = nullptr;
        m_messageHandler = std::move(other.m_messageHandler);
    }
    return *this;
}

void TIFFKeeper::reset(libtiff::TIFF* hFile)
{
    if (m_hFile != hFile) {
        TiffTools::closeTiffFile(m_hFile);
        m_hFile = hFile;
    }
}

libtiff::TIFF* TIFFKeeper::release()
{
    libtiff::TIFF* handle = m_hFile;
    m_hFile = nullptr;
    return handle;
}
```

and route `openTiffFile` through `reset` so it stops leaking:

```cpp
void TIFFKeeper::openTiffFile(const std::string& filePath, bool readOnly)
{
    reset(TiffTools::openTiffFile(filePath, readOnly));
}
```

`TiffTools::closeTiffFile` null-checks, so `reset()` needs no guard of its own. The `m_hFile != hFile` check makes `reset(sameHandle)` a no-op instead of a close-then-dangle.

- [ ] **Step 3: Restore the commented tests**

Uncomment the bodies of `resetClosesTheHandleItReplaces` and `resetOnAnEmptyKeeperTakesOwnership` in `src/tests/main/test_tiffkeeper.cpp` and remove the `// Task 2 adds reset()` markers.

- [ ] **Step 4: Build**

Run: `python install.py -a build-only -c release`
Expected: **compile errors at the call sites**, and that is the point. Removing `operator TIFF*()` and `operator=(TIFF*)` turns every affected site into a diagnostic. Do NOT fix them yet — record the full list in your report first, because it is the evidence that the migration in Task 3 is complete. If any error is an accidental COPY of a keeper rather than a conversion or an assignment, STOP and report it: that is a live double-close.

- [ ] **Step 5: Commit the class change alone**

The tree does not build yet; that is expected and the next task closes it.

```bash
git add src/slideio/imagetools/tiffkeeper.hpp src/slideio/imagetools/tiffkeeper.cpp src/tests/main/test_tiffkeeper.cpp
git commit -m "make TIFFKeeper a move-only owning handle"
```

---

## Task 3: Migrate the call sites

**Files:**
- Modify: `src/slideio/drivers/pke/pkescene.cpp:40,50`
- Modify: `src/slideio/drivers/scn/scnscene.cpp:211,232`
- Modify: `src/slideio/drivers/scn/scnslide.cpp:25,29`
- Modify: `src/slideio/drivers/svs/svsscene.cpp:40,50`
- Modify: `src/slideio/drivers/vsi/vsifilescene.cpp:29,73`
- Modify: `src/slideio/converter/tiffconverter.hpp:161`
- Modify: `src/tests/main/test_fiwrapper.cpp:106,171`

**Interfaces:**
- Consumes: `reset()`, `getHandle()` and `TIFFKeeperPtr` from Task 2.

- [ ] **Step 1: The five assignments become `reset()`**

Each of these currently reads `<keeper> = TiffTools::openTiffFile(<path>);` and becomes `<keeper>.reset(TiffTools::openTiffFile(<path>));`:

- `pkescene.cpp:40` — `m_tiffKeeper`
- `scnscene.cpp:232` — `m_tiff`
- `scnslide.cpp:25` — `m_tiff`
- `svsscene.cpp:40` — `m_tiffKeeper`
- `vsifilescene.cpp:73` — `m_tiff`

Do not change the surrounding `isValid()` checks or the error handling around them.

- [ ] **Step 2: The five conversions become `.getHandle()`**

- `scnscene.cpp:211` — `TiffTools::scanTiffDir(m_tiff, ...)` becomes `TiffTools::scanTiffDir(m_tiff.getHandle(), ...)`
- `scnslide.cpp:29` — `TiffTools::scanFile(m_tiff, directories)` becomes `TiffTools::scanFile(m_tiff.getHandle(), directories)`
- `vsifilescene.cpp:29` — `TiffTools::readStripedDir(m_tiff, ...)` becomes `TiffTools::readStripedDir(m_tiff.getHandle(), ...)`
- `pkescene.cpp:50` — `return m_tiffKeeper;` becomes `return m_tiffKeeper.getHandle();`
- `svsscene.cpp:50` — `return m_tiffKeeper;` becomes `return m_tiffKeeper.getHandle();`

The compiler's error list from Task 2 step 4 is the authority. If it names a site not listed here, migrate it the same way and say so in your report. If it does NOT name one of these, say that too — it would mean the site was already explicit.

- [ ] **Step 3: The macro user picks up the alias**

`tiffconverter.hpp:161` declares `TIFFKeeperPtr m_file;`. With the macro replaced by a namespace-scoped alias, this needs qualification unless the file already has `using namespace slideio;`. Check, and write `slideio::TIFFKeeperPtr m_file;` if it does not. `tiffconverter.cpp:861`'s `m_file.reset(new TIFFKeeper(filePath, false));` is `shared_ptr::reset` with a direct-initialised argument and needs no change.

- [ ] **Step 4: The two test sites take direct initialisation**

`test_fiwrapper.cpp:106` and `:171` read `TIFFKeeper tiff = TiffTools::openTiffFile(<path>);`. With the constructor `explicit`, each becomes:

```cpp
    TIFFKeeper tiff(TiffTools::openTiffFile(filePath));
```

Change nothing else in those tests.

- [ ] **Step 5: Build**

Run: `python install.py -a build-only -c release`
Expected: clean. Any remaining error is a site the list missed — migrate it the same way and record it.

- [ ] **Step 6: Run every suite**

```
slideio_tests, slideio_phtiff_tests, slideio_pke_tests, slideio_ometiff_tests,
slideio_ndpi_tests, slideio_vsi_tests, slideio_converter_tests, slideio_transformer_tests
```
Expected: 497, 103, 15, 98, 29, 30, 140, 39. The seven `TIFFKeeper` tests must now all PASS, including the two that failed in Task 1.

The scn, vsi, pke and converter suites matter most here: those drivers hold keepers by value and their scenes re-open files lazily, so a broken `reset()` shows up as a failure to read rather than as a compile error.

- [ ] **Step 7: Commit**

```bash
git add src/slideio/drivers/pke/pkescene.cpp src/slideio/drivers/scn/scnscene.cpp \
        src/slideio/drivers/scn/scnslide.cpp src/slideio/drivers/svs/svsscene.cpp \
        src/slideio/drivers/vsi/vsifilescene.cpp src/slideio/converter/tiffconverter.hpp \
        src/tests/main/test_fiwrapper.cpp
git commit -m "migrate the TIFFKeeper call sites to reset and getHandle"
```

---

## Task 4: Close the debt entry

**Files:**
- Modify: `software-docs/TECH_DEBT.md`
- Modify: `software-docs/specs/2026-08-15-tiffkeeper-ownership-design.md`

- [ ] **Step 1: Confirm the class has no way left to double-close**

Run these and paste the output into your report:

```
grep -n "operator libtiff::TIFF\|operator = (libtiff::TIFF\|operator=(libtiff::TIFF" src/slideio/imagetools/tiffkeeper.hpp
grep -rn "TIFFKeeperPtr" src/
grep -rn "= TiffTools::openTiffFile" src/slideio/
```
Expected: the first returns nothing; the second finds the alias definition and `tiffconverter.hpp` only, with no `#define`; the third finds only the three assignments to raw `libtiff::TIFF*` locals, in `otslide.cpp`, `pkeslide.cpp` and `svsslide.cpp`. It must NOT find `tiffkeeper.cpp`, because `openTiffFile` now passes the result to `reset()` as an argument rather than assigning it, and it must not find any keeper.

- [ ] **Step 2: Strike the fixed items in the debt log**

In section 1 of `software-docs/TECH_DEBT.md`, strike through problems 1, 2, 4, 5 and 7 with a pointer to the commits, in the same `~~...~~ Fixed (...)` style section 2 used. Problem 3 is already struck.

Problem 6, the duplication with `NDPITIFFKeeper`, is NOT fixed and must remain open — update its text to say that `TIFFKeeper` has since become move-only while `NDPITIFFKeeper` has not, so the two have diverged and unifying them now means bringing ndpi up rather than merging equals.

Add the two problems this work found and fixed, so the record is complete: `openTiffFile()` leaked exactly as `operator=` did, and `m_hFile` had no default member initialiser.

- [ ] **Step 3: Mark the spec implemented**

Set `**Status:** Implemented 2026-08-15` in `software-docs/specs/2026-08-15-tiffkeeper-ownership-design.md`.

- [ ] **Step 4: Commit**

```bash
git add software-docs/TECH_DEBT.md software-docs/specs/2026-08-15-tiffkeeper-ownership-design.md
git commit -m "close the TIFFKeeper ownership items in the tech debt log"
```
