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

**3. ~~Constructors are inconsistent.~~ Fixed.**
The `(filePath, readOnly)` constructor used to call `openTiffFile` while leaving
`m_messageHandler` null, so whether the libtiff message handler was installed
depended on which constructor was used. Both constructors now create it
(`tiffkeeper.cpp:11-20`). Kept as a struck-through entry rather than deleted
because the proposed direction below still refers to a shared init path; the
remaining value there is factoring the duplication, not fixing a bug.

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

**Status:** Closed. All nine items raised by the whole-branch review of the
v2.9.0 Philips work were fixed across commits `75a48f65..a9f179aa`: the
tile-count and parallel-arrays guards in `phCropLevelPadding`; the
level-number-based directory matching in `extractImages` that replaces size-only
matching; the rounding test for `phLevelContentSize`; tolerance for a
non-numeric attribute value in `phReadInt`; removal of the dead
`Tools::isXml`; the new `svsdriverids.hpp` header that fixes the driver-id
layering and drops the dead `svsimagedriver.hpp` include from `svsslide.cpp`;
the header note on `TIFFKeeper`'s non-LIFO handler-swap hazard; and the
switch to `PHTIFF_DRIVER_ID` plus full four-file coverage in
`test_phtiff_driver.cpp`'s accept-side detection test. See
`git log --oneline 75a48f65..a9f179aa` for the individual commits and their
messages, which cover the detailed before/after of each item.

### Consciously accepted, not debt

Detection has no fallback if the claiming driver then fails. A `.tif` carrying
Philips metadata that the driver cannot fully read used to open through GDAL —
flat, no pyramid — and now throws out of `openSlide`. This is inherent to
`findDriver` and identical for every other format; the strictness trade is
documented in the design's error-handling section. Recorded so it is a decision
rather than a discovery.
