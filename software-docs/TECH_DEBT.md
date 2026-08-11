# SlideIO Technical Debt

Running log of known technical debt: latent bugs, unsafe abstractions, and
refactoring opportunities identified during development that are not yet
scheduled. Each entry records the problem, impact, and a proposed direction so
the work can be picked up later without re-doing the analysis.

---

## Table of Contents

1. [`TIFFKeeper` unsafe value semantics](#1-tiffkeeper-unsafe-value-semantics)
2. [Philips TIFF driver follow-ups](#2-philips-tiff-driver-follow-ups)

---

## 1. `TIFFKeeper` unsafe value semantics

**File:** `src/slideio/imagetools/tiffkeeper.hpp` (+ `tiffkeeper.cpp`)
**Related:** `src/slideio/drivers/ndpi/ndpitifftools.hpp:134` (`NDPITIFFKeeper`, near-identical class)
**Status:** Open — proposal only, not implemented.

### Context

`TIFFKeeper` is a thin RAII wrapper around a `libtiff::TIFF*`: it owns the
handle, closes it in the destructor, and forwards a handful of operations to the
free functions in `TiffTools`. It is held by value as a member in a few places
(`SmallTiffWrapper::m_pTiff`, `ScnSlide::m_tiff`), by `shared_ptr` in the
converter (`tiffconverter.cpp:861`), and constructed on the stack in several
drivers/tests.

### Problems

**1. Looks like an owning handle but has unsafe value semantics (main issue).**
The destructor calls `TiffTools::closeTiffFile(m_hFile)`, so the class claims
ownership. But:
- The copy constructor and copy assignment are **implicitly generated and
  public** — copying a `TIFFKeeper` duplicates the raw pointer, leading to
  **double-close / use-after-free**. Any class holding one by value silently
  becomes unsafe to copy.
- There is **no move constructor / move assignment**, so ownership cannot be
  handed around cleanly (part of why the converter resorts to
  `shared_ptr<TIFFKeeper>`).

**2. `operator=(libtiff::TIFF*)` leaks.**
```cpp
TIFFKeeper& operator=(libtiff::TIFF* hFile) { m_hFile = hFile; return *this; }
```
Overwrites the owned handle **without closing the previous one** → resource
leak; also does not touch `m_messageHandler`.

**3. Constructors are inconsistent.**
The `TIFF*` constructor creates `m_messageHandler`; the `(filePath, readOnly)`
constructor calls `openTiffFile` and **leaves `m_messageHandler` null**. Whether
the libtiff message handler is installed depends on which constructor was used —
a latent bug.

**4. Implicit conversion `operator libtiff::TIFF*()`.**
Redundant with `getHandle()`, and implicit conversion to a raw owned pointer is
a footgun (accidental `delete`, pointer arithmetic, overload ambiguity with the
raw-pointer `operator=`). Most call sites already use `getHandle()` explicitly.

**5. `#define TIFFKeeperPtr std::shared_ptr<slideio::TIFFKeeper>`.**
A macro where a namespace-scoped alias belongs. Ignores scope, leaks into every
including TU, cannot be qualified.

**6. Duplication with `NDPITIFFKeeper`.**
Same handle/isValid/conversion/assignment shape and same latent issues; the two
differ only in the close function and export macro.

**7. Minor header hygiene.**
Relies on transitive includes for `<memory>` (shared_ptr), `<cstdint>`, and
`cv::Mat`; the `OPENCV_`-prefixed include guard is copy-paste residue.

### Proposed direction

Make `TIFFKeeper` a proper **move-only owning handle**, fix the leak/inconsistency,
and drop the footguns:

```cpp
namespace slideio
{
    class TIFFMessageHandler;

    class SLIDEIO_IMAGETOOLS_EXPORTS TIFFKeeper
    {
    public:
        explicit TIFFKeeper(libtiff::TIFF* hFile = nullptr);
        explicit TIFFKeeper(const std::string& filePath, bool readOnly = true);
        ~TIFFKeeper();

        // Move-only: an owning handle must not be copied.
        TIFFKeeper(const TIFFKeeper&)            = delete;
        TIFFKeeper& operator=(const TIFFKeeper&) = delete;
        TIFFKeeper(TIFFKeeper&& other) noexcept;
        TIFFKeeper& operator=(TIFFKeeper&& other) noexcept;

        libtiff::TIFF* getHandle() const { return m_hFile; }
        bool isValid() const             { return m_hFile != nullptr; }

        // Take ownership of a raw handle, closing any currently held one.
        void reset(libtiff::TIFF* hFile = nullptr);
        // Relinquish ownership without closing.
        libtiff::TIFF* release();

        void openTiffFile(const std::string& filePath, bool readOnly = true);
        void closeTiffFile();
        // ... (unchanged forwarding methods) ...

    private:
        libtiff::TIFF* m_hFile = nullptr;
        std::shared_ptr<TIFFMessageHandler> m_messageHandler;
    };

    using TIFFKeeperPtr = std::shared_ptr<TIFFKeeper>;
}
```

Concrete changes:
- **Delete copy, add move** (move ctor/assign transfer `m_hFile` + `m_messageHandler`
  and null the source).
- **Replace `operator=(libtiff::TIFF*)` with `reset()`** that closes the old handle
  first. Call sites doing `keeper = TiffTools::openTiffFile(...)` become
  `keeper.reset(...)`.
- **Create `m_messageHandler` in a shared init path** used by both constructors, so
  behavior is consistent regardless of entry point.
- **Remove `operator libtiff::TIFF*()`**; standardize on `getHandle()`. Widest
  call-site impact — sites passing a `keeper` where a `TIFF*` is expected
  (`pkeslide.cpp:61`, `otslide.cpp:92`, `svsslide.cpp:143`, ...) need `.getHandle()`.
- **Replace the macro** with `using TIFFKeeperPtr = ...;`.
- Add `<memory>`, `<cstdint>`, and the OpenCV core include; consider `#pragma once`.

Follow-up (separate change): collapse `TIFFKeeper` and `NDPITIFFKeeper` onto a
shared move-only handle template (e.g. header-only `TiffHandle` parameterized by
close function).

### Impact & compatibility

- Shared-library (`slideio-imagetools`) header used by **7 drivers, the converter,
  and tests** — an API change. Making it move-only will **surface any accidental
  copies at compile time** (likely none, since by-value members live in
  non-copyable slide classes — the build will confirm).
- Call sites needing edits: the raw-pointer `operator=` assignments and the
  implicit-conversion sites above — a bounded, mechanical set (~10 files).
- No behavioral change to reading/writing TIFF data; risk is confined to
  ownership/lifetime. Validation gate: `slideio_tests`, `slideio_ometiff_tests`,
  `slideio_ndpi_tests`, `slideio_converter_tests`.

### Scope options (for whoever picks this up)

1. **Full RAII fix** — move-only, `reset()`/`release()`, consistent handler init,
   drop implicit conversion + macro (~10 call sites). *Recommended.*
2. **Safety-only, keep API** — delete copy / add move and fix the `operator=` leak,
   but keep `operator libtiff::TIFF*()` and the macro to minimize churn.
3. **Full fix + unify with NDPI** — option 1 plus collapsing `NDPITIFFKeeper`
   (largest blast radius).

---

## 2. Philips TIFF driver follow-ups

**Files:** `src/slideio/drivers/svs/svsslide.cpp`, `svsimagedriver.cpp`,
`src/slideio/core/tools/tools.cpp`, `src/tests/phtiff/test_phtiff_driver.cpp`
**Related:** `software-docs/specs/2026-08-11-phtiff-format-detection-design.md`
**Status:** Open — raised by the whole-branch review of the v2.9.0 Philips work
(commits `36a0df2f`..`b4ad5b48`), triaged as non-blocking and deliberately not fixed.

### Context

Four defects in the Philips TIFF support were fixed on v2.9.0: zoom level tile
padding, auxiliary image identity, absent format detection, and a driver that
claimed every `*.tif`. The review that gated the branch raised no Critical
findings and its three Important findings were fixed. The items below are what it
left on the table, recorded here so the analysis is not re-done.

### Correctness hardening (highest value first)

**1. The tile-count invariant is relied on but never asserted.**
`phCropLevelPadding` shrinks a level directory to its content size. That is only
safe because `ceil(content/tile) == stored/tile` — true for all 35 levels of the
four Philips test files, and a consequence of Philips padding each level to its
own tile grid, but nothing checks it. A violation would skew tile indices
silently: wrong pixels, no error. A guard next to the existing
`contentSize > dir` check ("if the tile count would change, warn and do not
crop") turns the worst case into a visible degradation. Cheap insurance on a
clinical read path.

**2. Size matching can still bind the wrong level, in one narrow case.**
Levels are matched to directories by declared size. If an *undeclared* tiled
directory happens to have the exact pixel dimensions of one of two
identically-sized declared levels, and sits between their real directories in
file order, the interloper can claim the declared level (marked corroborated, so
it gets cropped) and the real directory is dropped. Requires a coincidental size
collision on top of an interloper; the pre-fix positional code mishandled the
same input differently. A focused test would pin it:
`phExtractImages_sameSizedInterloperBetweenTiedLevelsBindsWrongDirectory`.

**3. The rounding rule in `phLevelContentSize` is never exercised.** Every real
base size is a multiple of the 512 tile grid, so `base / 2^level` divides exactly
for all levels ≤ 9 and the `ceil` never rounds — on real files or in the
synthetic tests. `ceil` versus floor is therefore an untested decision. One
synthetic case with a base that does not divide (base 4098 → level 1 content
2049) plus a comment on why rounding up is right would settle it.

**4. `phCropLevelPadding` trusts its two arguments to be parallel.** It indexes
`dirs[index]` over `imagePyramid`'s range and reads `dirs.front()` with no size
check; only its single caller guarantees that. One guard line.

**5. One malformed zoom level aborts the whole slide open.** The level number is
read with `getAttributeInt`, which raises when the attribute is missing or
non-numeric, while the size attributes two lines below use `hasAttribute` and
warn. Pre-existing, and out of step with the warn-and-continue posture of the
rest of that function.

### Structure and consistency

**6. `Tools::isXml` is dead production code.** Added by `391ed0e3` for this work,
then deliberately bypassed by `PHTDescription::isPhilipsDescription` for a
documented performance reason (a single parse instead of two over descriptions
that reach 844 KB). It has no caller outside its own unit test: either use it or
remove it.

**7. Layering points the wrong way.** `svsslide.cpp` and `svstiledscene.cpp`
include `svsimagedriver.hpp` purely for `PHTIFF_DRIVER_ID` — the format's data
classes depending on the driver class. A three-line `svsdriverids.hpp`, or the
ids in `svstools.hpp`, would keep the dependency pointing down.

**8. `TIFFKeeper`'s handler swap is global and not order-safe.** The path
constructor now swaps libtiff's process-global error/warning handlers for the
keeper's lifetime. With overlapping, non-LIFO keeper lifetimes a destructor can
restore a handler while another keeper is still alive, routing later libtiff
messages to stderr. No dangling-pointer risk (both handlers are static
functions), and the `TIFF*` constructor has always behaved this way, so this did
not create the pattern. Worth a line in the header. See also item 1 of this
document.

**9. Test consistency.** Six places in `test_phtiff_driver.cpp` still hardcode
`"PHTIFF"` after `PHTIFF_DRIVER_ID` was introduced to name it. Accept-side
detection coverage is also one file: adding the other three Philips files to the
accept assertions would pin that the predicate does not depend on the XML
prolog, which Philips-4 omits.

### Consciously accepted, not debt

Detection has no fallback if the claiming driver then fails. A `.tif` carrying
Philips metadata that the driver cannot fully read used to open through GDAL —
flat, no pyramid — and now throws out of `openSlide`. This is inherent to
`findDriver` and identical for every other format; the strictness trade is
documented in the design's error-handling section. Recorded so it is a decision
rather than a discovery.
