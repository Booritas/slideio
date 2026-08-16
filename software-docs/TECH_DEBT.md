# SlideIO Technical Debt

Running log of known technical debt: latent bugs, unsafe abstractions, and
refactoring opportunities identified during development that are not yet
scheduled. Each entry records the problem, impact, and a proposed direction so
the work can be picked up later without re-doing the analysis.

---

## Table of Contents

1. [`TIFFKeeper` unsafe value semantics](#1-tiffkeeper-unsafe-value-semantics)
2. [Philips TIFF driver follow-ups](#2-philips-tiff-driver-follow-ups)
3. [`SCNSlide` passes a `TIFF*` where `SVSSmallScene` expects a `bool`](#3-scnslide-passes-a-tiff-where-svssmallscene-expects-a-bool)
4. [`CVScene` serialises every block read, and does so inconsistently](#4-cvscene-serialises-every-block-read-and-does-so-inconsistently)
5. [`ImageTools::computeSimilarity2` cannot handle more than four channels](#5-imagetoolscomputesimilarity2-cannot-handle-more-than-four-channels)
6. [`CZIScene::getRect()` returns non-zero-based coordinates that block reads cannot use](#6-cziscenegetrect-returns-non-zero-based-coordinates-that-block-reads-cannot-use)
7. [`SCNScene::getRect()` has the same problem](#7-scnscenegetrect-has-the-same-problem)
8. [SCN level selection assumes parallel pyramid geometry across channels and z-slices](#8-scn-level-selection-assumes-parallel-pyramid-geometry-across-channels-and-z-slices)
9. [`TilerData::relativeZoom` is dead, and its new formula is only valid in one case](#9-tilerdatarelativezoom-is-dead-and-its-new-formula-is-only-valid-in-one-case)
10. [`zSliceRange` / `timeFrameRange` are documented backwards in `scene.hpp`](#10-zslicerange--timeframerange-are-documented-backwards-in-scenehpp)

---

## 1. `TIFFKeeper` unsafe value semantics

**File:** `src/slideio/imagetools/tiffkeeper.hpp` (+ `tiffkeeper.cpp`)
**Related:** `src/slideio/drivers/ndpi/ndpitifftools.hpp:134` (`NDPITIFFKeeper`, now a diverged twin — see problem 6)
**Status:** RAII fix implemented across commits `53d13332..06c456a7` (design,
plan, tests, the move-only class, and the call-site migration). Only problem 6,
unifying with `NDPITIFFKeeper`, remains open.

### Context

`TIFFKeeper` is a thin RAII wrapper around a `libtiff::TIFF*`: it owns the
handle, closes it in the destructor, and forwards a handful of operations to the
free functions in `TiffTools`. It is held by value as a member in a few places
(`SmallTiffWrapper::m_pTiff`, `ScnSlide::m_tiff`), by `shared_ptr` in the
converter (`tiffconverter.cpp:861`), and constructed on the stack in several
drivers/tests.

### Problems

**1. ~~Looks like an owning handle but has unsafe value semantics (main issue).~~
Fixed** (`tiffkeeper.hpp`/`tiffkeeper.cpp`, commit `20c7663d`). Copy is now
`= delete`d and a move constructor/move assignment were added, so ownership
transfers explicitly instead of being duplicated by an implicit copy.

**2. ~~`operator=(libtiff::TIFF*)` leaks.~~ Fixed** (commit `20c7663d`). The
operator is gone; `reset(libtiff::TIFF*)` (`tiffkeeper.cpp:50-56`) replaces it
and closes the previously held handle before taking the new one.

**3. ~~Constructors are inconsistent.~~ Fixed.**
The `(filePath, readOnly)` constructor used to call `openTiffFile` while leaving
`m_messageHandler` null, so whether the libtiff message handler was installed
depended on which constructor was used. Both constructors now create it
(`tiffkeeper.cpp:11-20`). Kept as a struck-through entry rather than deleted
because the proposed direction below still refers to a shared init path; the
remaining value there is factoring the duplication, not fixing a bug.

**4. ~~Implicit conversion `operator libtiff::TIFF*()`.~~ Fixed** (commits
`20c7663d`, `06c456a7`). The conversion operator was removed and every call
site that relied on it — including a few beyond the ones originally scoped,
found by the compiler — was migrated to explicit `.getHandle()`.

**5. ~~`#define TIFFKeeperPtr std::shared_ptr<slideio::TIFFKeeper>`.~~ Fixed**
(commit `20c7663d`). Replaced with `using TIFFKeeperPtr = std::shared_ptr<TIFFKeeper>;`
inside `namespace slideio` (`tiffkeeper.hpp:73`).

**6. Duplication with `NDPITIFFKeeper` — the two have since diverged.**
This entry originally described near-identical twins. That is no longer
accurate: `TIFFKeeper` is now the move-only owning handle described above —
copy deleted, move added, `reset()`/`release()` instead of the leaking
`operator=`, no implicit conversion. `NDPITIFFKeeper`
(`src/slideio/drivers/ndpi/ndpitifftools.hpp:134`) was left untouched by this
work and still has the copyable, implicitly-converting, leaking-assignment
shape both classes used to share — it carries today exactly the defects this
entry's problems 1, 2 and 4 recorded for `TIFFKeeper`. Unifying the two is no
longer "merge two equals"; it means bringing `NDPITIFFKeeper` up to the
contract `TIFFKeeper` now has, then collapsing them onto a shared move-only
handle. Remains open.

**7. ~~Minor header hygiene.~~ Fixed** (commit `20c7663d`). `<memory>` and
`<cstdint>` are now included directly in `tiffkeeper.hpp` rather than relied
on transitively.

**8. ~~`openTiffFile()` leaked exactly as `operator=` did.~~ Fixed** (commit
`20c7663d`). `TIFFKeeper::openTiffFile` used to assign
`TiffTools::openTiffFile(...)` straight into `m_hFile`, leaking any handle
already held — the same bug as problem 2, just reached through a different
entry point. It was not recorded when this entry was first written; it was
found while designing the fix (see the spec's "Two problems the debt entry
does not record"). `openTiffFile` now routes through `reset()`
(`tiffkeeper.cpp:45-48`), which closes the old handle first.

**9. ~~`m_hFile` had no default member initialiser.~~ Fixed** (commit
`20c7663d`). Harmless at the time — a throwing constructor meant the
destructor never ran — but latent, since it would stop being harmless the
moment a member whose construction can throw was declared after it. Also
found while designing the fix and not originally recorded here.
`m_hFile` is now declared `libtiff::TIFF* m_hFile = nullptr;`
(`tiffkeeper.hpp:68`).

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

---

## 3. `SCNSlide` passes a `TIFF*` where `SVSSmallScene` expects a `bool`

**File:** `src/slideio/drivers/scn/scnslide.cpp:89`
**Related:** `src/slideio/drivers/svs/svssmallscene.hpp:20-25`
**Status:** Open. Found while reviewing the `TIFFKeeper` ownership change;
pre-existing and unrelated to it.

`SCNSlide` builds a `supplementalImage` scene with:

```cpp
std::shared_ptr<SVSSmallScene> scene(new SVSSmallScene(m_filePath, getDriverId(), tagName,
    directory, m_tiff.getHandle()));
```

`SVSSmallScene`'s fifth constructor parameter is `bool auxiliary = true`, not a
`TIFF*`. The `libtiff::TIFF*` returned by `m_tiff.getHandle()` silently
converts to `bool` — non-null, so `true` — and the handle itself is discarded;
`SVSSmallScene` never sees it. The call is equivalent to omitting the argument
and taking the default.

**Impact today: none.** `m_tiff` is validated non-null before this code runs,
so the converted value always matches the default every other call site
already passes. This is a latent trap, not a live bug.

**Trap for whoever fixes it:** the obvious repair — add a `TIFF*`-taking
`SVSSmallScene` overload/parameter so the scene reuses the slide's already-open
handle instead of implying it should open its own — creates a real double
close if implemented naively. `SCNSlide::m_tiff` keeps ownership of that handle
and closes it in `~SCNSlide`. Handing the same raw pointer to `SVSSmallScene`
without transferring ownership means two owners closing one handle. Any fix
that shares the handle must go through `TIFFKeeper::release()` (or equivalent
explicit ownership transfer), not a bare `getHandle()` passed to a second
owner.

---

## 4. `CVScene` serialises every block read, and does so inconsistently

**File:** `src/slideio/core/cvscene.cpp`
**Related:** issue #69, the 2026-08-16 explicit-level-reading plan
**Status:** Open. Found while adding the level-addressed read path; predates it
and is out of scope for it.

`CVScene::readResampledBlockChannels` and `readResampledLevelBlockChannels`
each take `m_readBlockMutex` for the whole read, so no two block reads of one
scene ever overlap. A tiled viewer fetching tiles from a thread pool — the
workload issue #69 describes — therefore gets no concurrency from the
library, and this is likely to dominate whatever the level-addressed read
path saves it.

Separately, inside `assemble4DBlock` the same mutex is taken in the
single-plane branch and not in the multi-plane one. That asymmetry predates
the level API; it was carried over unchanged when the plane loop was
extracted for 2.9.0, deliberately, so that the extraction stayed
behaviour-preserving.

Fixing either needs a thread-safety audit of the driver state each `readTile`
implementation touches — the TIFF handle above all, which several drivers
share across a whole slide. Out of scope for the level API and recorded here
so it is not lost.

---

## 5. `ImageTools::computeSimilarity2` cannot handle more than four channels

**File:** `src/slideio/imagetools/imagetools.cpp:163`
**Related:** `src/slideio/imagetools/imagetools.hpp:51` (exported public API)
**Status:** Open. Found while working with a multiplex fluorescence fixture.

`cv::Scalar sums = cv::sum(diffd)` returns a four-element `cv::Scalar`, and
`cv::sum` asserts `cn <= 4`. `computeSimilarity2` therefore throws on exactly
the multiplex fluorescence images the PKE and CZI drivers exist to read — it
was hit on `LuCa-7color_Scan1.qptiff` (5 channels).

The function is exported public API. Any test author reaching for it on a
multiplex fixture with more than four channels rediscovers this the hard way.
Fixing it means summing per-channel (e.g. looping planes and accumulating, or
reshaping before calling `cv::sum`) instead of relying on `cv::Scalar`'s
four-slot limit.

---

## 6. `CZIScene::getRect()` returns non-zero-based coordinates that block reads cannot use

**File:** `src/slideio/drivers/czi/cziscene.cpp` (`computeSceneRect`,
`updateTileRects`)
**Status:** Open. Found while adding the level-addressed read path.

`computeSceneRect` builds `m_sceneRect` from the raw union of sub-block rects
in file coordinates — observed origins `x=-90720` and `x=-421920` on two
fixtures — while `updateTileRects` builds each tile's addressable rect as
`zoom * (tile.rect - m_sceneRect.{x,y})`, which is zero-based.
`TileComposer::composeRect` intersects `blockRect` with those tile rects, so
passing `scene->getRect()` straight in as a `blockRect` silently reads the
wrong region for mosaic and split-region files. This is the obvious call and
it is wrong.

No fix is proposed here; recorded so the mismatch between what `getRect()`
returns and what the tile geometry expects is not rediscovered by trial and
error.

---

## 7. `SCNScene::getRect()` has the same problem

**File:** `src/slideio/drivers/scn/scnscene.cpp` (`parseGeometry`)
**Related:** [§6](#6-cziscenegetrect-returns-non-zero-based-coordinates-that-block-reads-cannot-use)
**Status:** Open. Found while adding the level-addressed read path.

`parseGeometry` sets `m_rect.x`/`m_rect.y` from the `<view>` element's
`offsetX`/`offsetY`, a physical-position origin — observed
`[4737x6338 from (16306,40361)]` on `Leica-Fluorescence-1.scn`. Same
consequence as §6: passing `getRect()` straight into a block read silently
reads the wrong region.

Worth checking whether any other driver shares this pattern before fixing
either.

---

## 8. SCN level selection assumes parallel pyramid geometry across channels and z-slices

**File:** `src/slideio/drivers/scn/scnscene.cpp`
**Related:** `zStack`, `zStackMissingChannels` tests
**Status:** Open. Found while adding the level-addressed read path.

`SCNScene`'s level-addressed read derives the level index and the level
geometry from channel 0 at z=0, because that is what `m_levels` is built
from, then uses that single level index to address every requested channel's
own z-specific directory list. The pre-split code instead searched per
channel at the requested z. The two agree only if every channel's pyramid
shares the same scale sequence across z-slices — an assumption, not an
invariant.

The `zStack` and `zStackMissingChannels` tests cover the one z-stack fixture
in the suite, and both pass under this assumption. A file that violates it
would misregister the level rect silently. Fixing it means resolving the
level per channel/z-slice combination rather than once from channel 0.

---

## 9. `TilerData::relativeZoom` is dead, and its new formula is only valid in one case

**File:** the CZI and DICOM-WSI read paths that populate `TilerData`
**Status:** Open. `relativeZoom` itself predates this work; its formula
changed during the 2026-08-16 explicit-level-reading work.

The field is written by the CZI and DICOM-WSI read paths and **read by
nothing** — that was already true before this work. During the level split
its computation changed from `levelZoom / zoom` to
`levelRect.width / blockSize.width`. Those are equal only when the zoom is
width-dominant; `zoom` is `max(zoomX, zoomY)`, so on a height-dominant
anisotropic resize they diverge by the `zoomY/zoomX` ratio. Additionally
`Tools::scaleRect` floors the origin and ceils the far corner independently,
so `levelRect.width` drifts from `w*scale` by a pixel or two, and that drift
propagates into the new formula too.

Harmless today because the field is unread. **Anyone reviving `relativeZoom`
must either fix the formula or delete the field** — do not assume the current
formula is correct just because it compiles and nothing reads it.

---

## 10. `zSliceRange` / `timeFrameRange` are documented backwards in `scene.hpp`

**File:** `src/slideio/slideio/scene.hpp`
**Related:** `src/slideio/slideio/scene.cpp:25-29` (`tupleToRange`)
**Status:** Partially fixed. The doc comments in `scene.hpp` were corrected
as part of the 2026-08-16 explicit-level-reading documentation pass
(comment-only change, no behaviour touched). Any other place repeating the
old, wrong wording may still be out there and was not searched for.

The doc comments used to describe `std::tuple<indexOfFirstSliceToRead,
numberOfSlicesToRead>` — a `<start, count>` pair — but `tupleToRange` builds
`cv::Range(get<0>, get<1>)`, a `<start, end>` pair. They coincide only when
start is 0. The Python layer documents it correctly as "(first, last+1)" and
computes `numSlices = stop - start`, so the code was always right and only
the C++ doc comments were wrong, on every method taking those parameters.

Recorded here (rather than only fixed silently) because the same wrong
phrasing may be copy-pasted elsewhere — e.g. other headers, external
documentation, or code comments outside `scene.hpp` — and that was not
audited as part of this pass.
