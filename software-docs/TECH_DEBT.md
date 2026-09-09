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
8. [SCN and OME-TIFF level selection assume parallel pyramid geometry across dimensions](#8-scn-and-ome-tiff-level-selection-assume-parallel-pyramid-geometry-across-dimensions)
9. [`TilerData::relativeZoom` is dead, and its new formula is only valid in one case](#9-tilerdatarelativezoom-is-dead-and-its-new-formula-is-only-valid-in-one-case)
10. [`zSliceRange` / `timeFrameRange` are documented backwards in `scene.hpp`](#10-zslicerange--timeframerange-are-documented-backwards-in-scenehpp)
11. [`TransformerScene` has no level table, so transformed scenes cannot be read by level](#11-transformerscene-has-no-level-table-so-transformed-scenes-cannot-be-read-by-level)
12. [`SCNScene::getChannelDirectories` indexes unchecked, and the 4D level path widens the exposure](#12-scnscenegetchanneldirectories-indexes-unchecked-and-the-4d-level-path-widens-the-exposure)
13. [`slideio-core`'s export-control define breaks the project naming convention](#13-slideio-cores-export-control-define-breaks-the-project-naming-convention)
14. [ZVI now reports concurrent reads (resolved)](#14-zvi-now-reports-concurrent-reads-resolved)
15. [DCM still serialises every block read](#15-dcm-still-serialises-every-block-read)
16. [GDAL still serialises every block read](#16-gdal-still-serialises-every-block-read)
17. [OME-TIFF now reports concurrent reads (resolved)](#17-ome-tiff-now-reports-concurrent-reads-resolved)
18. [CZI rejects a corrupt sub-block position on the main path and tolerates it on the attachment path](#18-czi-rejects-a-corrupt-sub-block-position-on-the-main-path-and-tolerates-it-on-the-attachment-path)
19. [pole read-path defects left in place](#19-pole-read-path-defects-left-in-place)
20. [The ZVI concurrent-read work: what no test covers](#20-the-zvi-concurrent-read-work-what-no-test-covers)
21. [pole's positional read path is ~20% slower single-threaded](#21-poles-positional-read-path-is-20-slower-single-threaded)

---

## 1. `TIFFKeeper` unsafe value semantics

**File:** `src/slideio/imagetools/tiffkeeper.hpp` (+ `tiffkeeper.cpp`)
**Related:** `src/slideio/drivers/ndpi/ndpitiffkeeper.hpp` (`NDPITIFFKeeper` — see problem 6)
**Status:** RAII fix implemented across commits `53d13332..06c456a7` (design,
plan, tests, the move-only class, and the call-site migration). `NDPITIFFKeeper`
was brought up to the same contract afterwards on branch `v2.10.0`. Only the
second half of problem 6 — collapsing the two onto one shared handle — remains
open.

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

**6. Duplication with `NDPITIFFKeeper` — first half done, unification still open.**
This entry originally described near-identical twins, then described
`NDPITIFFKeeper` as the diverged one still carrying the defects problems 1, 2
and 4 record for `TIFFKeeper`. The first half of the fix — bringing
`NDPITIFFKeeper` up to `TIFFKeeper`'s contract — is now done. It moved out of
`ndpitifftools.hpp`/`.cpp` into its own `ndpitiffkeeper.hpp`/`.cpp` and is now
a `SLIDEIO_NDPI_EXPORTS` move-only owning handle: copy explicitly deleted,
move constructor/assignment added with the same deliberately-not-moved message
handler as `TIFFKeeper`, `reset()`/`release()`/`openTiffFile()`/`closeTiffFile()`
replacing the leaking `operator=(TIFF*)`, `operator libtiff::TIFF*()` removed,
`m_hFile` default-initialised, and `<memory>`/`<string>` included directly. The
two `NDPIFile` call sites that relied on the implicit conversion and the raw
assignment were migrated. Covered by `src/tests/ndpi/test_ndpitiffkeeper.cpp`,
which mirrors `test_tiffkeeper.cpp`.

What this entry recorded for problems 1, 2 and 4 held. Two further findings it did
not record, found by doing the work:

- **`NDPITiffTools::closeTiffFile` had no null guard** and called
  `libtiff::TIFFClose` unconditionally, unlike `TiffTools::closeTiffFile`.
  Reached with `nullptr` it was an access violation (observed: SEH `0xc0000005`).
  `~NDPITIFFKeeper` guarded itself with `if (m_hFile)`, so the crash was only
  reachable through the free function — which `~NDPIFile` called directly. Now
  guarded, with a test.
- **The keeper installed no message handler, unlike `TIFFKeeper`.** At `c89bb999`
  `NDPITIFFKeeper` had no handler member at all. `NDPITIFFMessageHandler` was *not*
  dead code, though — the driver installs one as a stack local at four entry points
  (`ndpiimagedriver.cpp:26`, `ndpiscene.cpp:132`, `:369`, `:418`), so anything reached
  through `openFile`, `NDPIScene::init` or a scene read was already covered: warnings
  reached `SLIDEIO_LOG` and `NDPITIFFErrorHandler`'s `RAISE_RUNTIME_ERROR` did fire.
  The gap was code reaching libtiff *outside* those four scopes — chiefly tests calling
  `NDPITiffTools` directly, which ran against libtiff's default handlers.

  Both keeper constructors now install one via a shared `initMessageHandler()`,
  matching `TIFFKeeper`, and the `slideio_ndpi_tests` fixtures install one each so the
  direct-`NDPITiffTools` tests are covered too. On the driver's own paths this changes
  nothing (the handler was already installed and the keeper's merely nests inside it,
  LIFO-safely); for direct `NDPITiffTools` callers it is a **behaviour change** —
  libtiff errors now throw rather than printing to stderr. The full
  `slideio_ndpi_tests` and `slideio_tests` suites pass with it.

  Note the ordering trap the keeper's handler does *not* close: in
  `NDPITIFFKeeper keeper(NDPITiffTools::openTiffFile(path))` the file is opened while
  evaluating the argument, *before* the constructor body installs the handler, so
  whatever handler is already current reports any problem with that open. Opening
  through the `filePath` constructor or `openTiffFile()` has no such gap.

**Still open:** collapsing `TIFFKeeper` and `NDPITIFFKeeper` onto one shared
move-only handle. They now have the same contract but remain two classes, because
the NDPI driver links its own patched libtiff and routes messages through
`NDPITIFFMessageHandler` rather than `TIFFMessageHandler`, and `slideio-imagetools`
is not in the NDPI driver's link closure. Doing it means a header-only handle
template parameterised by close function and handler type — the "Follow-up
(separate change)" already named at the end of the proposed direction below.

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

**Constraint added 2026-09-07.** SCN scenes now declare concurrent reads
(`software-docs/specs/2026-09-07-parallel-read-block-design.md` §4.5.1). The
handle being silently discarded is what makes SCN's auxiliary scenes safe, so
fixing this must **drop** the argument, not plumb the slide's handle into the
scene -- the latter would put a shared, unsynchronised `TIFF*` back into a
driver that advertises concurrency. The code already carries this warning at
the construction site in `scnslide.cpp`.

---

## 4. `CVScene` serialises every block read, and does so inconsistently

**File:** `src/slideio/core/cvscene.cpp`
**Related:** issue #69, the 2026-08-16 explicit-level-reading plan,
`software-docs/specs/2026-09-07-parallel-read-block-design.md`
**Status:** Partially fixed. The `assemble4DBlock` asymmetry is resolved
outright: both branches now take `lockIfSerialised()`, with the lock scoped to
the `readPlane` call only in the multi-plane branch. The serialisation itself
is removed for the scenes of SVS, PHTIFF, AFI, PKE, SCN, NDPI, CZI, VSI,
OME-TIFF (see [§17](#17-ome-tiff-now-reports-concurrent-reads-resolved)) and
ZVI (see [§14](#14-zvi-now-reports-concurrent-reads-resolved)).
Kept open because DCM and GDAL still serialise every block
read — see [§15](#15-dcm-still-serialises-every-block-read) and
[§16](#16-gdal-still-serialises-every-block-read).

`CVScene::readResampledBlockChannels` and `readResampledLevelBlockChannels`
used to each take `m_readBlockMutex` for the whole read, so no two block
reads of one scene ever overlapped. A tiled viewer fetching tiles from a
thread pool — the workload issue #69 describes — therefore got no
concurrency from the library.

The fix: `CVScene::supportsConcurrentReads()` (public virtual, defaulting to
`false`) says whether a scene's block reads may overlap, and
`CVScene::lockIfSerialised()` takes `m_readBlockMutex` only for scenes that
still report `false`. Ten of the twelve formats now override it to `true`,
having made every mutable object on their read path either cursor-free
(`FileReader`) or per-thread (`ContextPool`, handing out `ReadContext`
subclasses one borrower at a time). Measured on the same test and image
before/after: NDPI 471712 → 68667 ms (6.87×), SVS 21239 → 3820 ms (5.6×), SCN
16574 → 2978 ms (5.6×).

---

## 5. `ImageTools::computeSimilarity2` cannot handle more than four channels

**File:** `src/slideio/imagetools/imagetools.cpp:164`
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

## 8. SCN and OME-TIFF level selection assume parallel pyramid geometry across dimensions

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

### OME-TIFF has the same shape

**File:** `src/slideio/drivers/ome-tiff/otscene.cpp` (`extractImagePyramids`,
`readTile`)
**Status:** Open. Pre-existing, unchanged by this work — found during the
whole-branch review for the 2026-08-16 explicit-level-reading plan.

This is not SCN-specific. `OTScene::extractImagePyramids`
(`otscene.cpp:143-153`) builds `m_levels` from
`m_tiffData.front().getTiffDirectory(0)` alone, then `readTile`
(`otscene.cpp:404`, `:410-412`) takes that single `zoomLevel` and applies it
to **every** `TiffData` entry that `collectTiffDataIndices` selected for the
requested channel/z/t (`for (int index : blockInfo->tiffDataIndices) { ...
tiffData.readTile(channelIndices, zSlice, tFrame, zoomLevel, tileIndex,
channelRasters); }`). If two `TiffData` entries differ in subresolution
count or geometry, the level index desynchronises silently, the same failure
mode as the SCN case above.

This is pre-existing and unaffected by the level-addressed read work: the
pre-split code reached `readTile` by the identical route, via
`&levelInfo` carried in the same `BlockInfo`. Recorded here so this entry
does not read as though SCN were the only driver with this structure.

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

---

## 11. `TransformerScene` has no level table, so transformed scenes cannot be read by level

**File:** `src/slideio/transformer/transformerscene.cpp`/`.hpp`
**Related:** `src/slideio/transformer/transformer.cpp:20,27`
(`transformScene`/`transformSceneEx`); `src/slideio/core/cvscene.cpp:221-224`
(the throw site)
**Status:** Open. Found during the whole-branch review for the 2026-08-16
explicit-level-reading plan.

`TransformerScene` never populates `m_levels` and does not override
`getNumZoomLevels()`, so it reports 0 zoom levels via `CVScene`'s default.
`transformScene`/`transformSceneEx` (`transformer.cpp:20`, `:27`) hand back a
public `slideio::Scene` wrapping a `TransformerScene`, so any caller doing a
level-addressed read against a transformed scene hits `CVScene`'s guard and
gets `"... does not report any zoom level and cannot be read by level"`
(`cvscene.cpp:221-224`).

The design spec for this plan's §5.1 asserts that after the §5.6 sweep "no
in-tree driver is in that state" — that claim holds; `TransformerScene` is
not a driver (it wraps one) and was out of scope for that sweep, not missed
by an error in it.

**Open design question, recorded rather than answered:** a transformed scene
arguably *should* expose its source scene's pyramid, with the transformation
applied at the requested level's resolution. But that is a feature with its
own design decision — e.g. what a Gaussian blur kernel radius means at level
3 versus level 0 — not a bug to patch mechanically by forwarding
`getNumZoomLevels()`/`getZoomLevelInfo()` to the origin scene. Do not
implement level support for `TransformerScene` without first deciding what a
transformation means at non-zero levels.

---

## 12. `SCNScene::getChannelDirectories` indexes unchecked, and the 4D level path widens the exposure

**File:** `src/slideio/drivers/scn/scnscene.hpp:64-66`
**Related:** `src/slideio/slideio/scene.hpp` (`readResampledLevel4DBlockChannels`);
`src/slideio/core/cvscene.cpp` (`assemble4DBlock`)
**Status:** Open. The indexing bug is pre-existing; this branch adds a second
entry point to it.

```cpp
const std::vector<TiffDirectory>& getChannelDirectories(int channelIndex, int zIndex) const {
    const int dirIndex = zIndex * m_planeCount + (m_interleavedChannels ? 0 : channelIndex);
    return m_channelDirectories[dirIndex];
}
```

`dirIndex` is used with `operator[]` on `m_channelDirectories`, unvalidated
against its size. Neither `Scene::readResampledLevel4DBlockChannels` nor
`CVScene::assemble4DBlock` validates `zSliceRange` (or `channelIndex`)
against `getNumZSlices()`/`getNumChannels()` before it reaches this call, so
an out-of-range `zSliceRange` is a heap out-of-bounds read, not a thrown
error.

**This is pre-existing, not new.** The identical exposure already reaches
`getChannelDirectories` through `readResampled4DBlockChannels`, which existed
before this plan. What this branch adds is a second entry point —
`readResampledLevel4DBlockChannels` — that reaches the same unvalidated
indexing through a different call path; it does not create the underlying
bug. Fixing it means validating `zSliceRange`/`channelIndices` against scene
dimensions once, upstream of both entry points (e.g. in `assemble4DBlock`),
rather than patching each caller.

---

## 13. `slideio-core`'s export-control define breaks the project naming convention

**File:** `src/slideio/core/CMakeLists.txt:67`, `src/slideio/core/slideio_core_def.hpp`

Every module gates its `__declspec(dllexport)` on a compile definition named
`SLIDEIO_<MODULE>_API` — `SLIDEIO_NDPI_API`, `SLIDEIO_IMAGETOOLS_API`,
`SLIDEIO_CONVERTER_API`, and twelve more. `slideio-core` alone uses
`SLIDEIO_CORE`, with no `_API` suffix.

This is cosmetic — the macro works — but it is a trap for anyone adding a module
by copying core's CMakeLists, and it defeats a grep for `SLIDEIO_.*_API` across
the build.

Noted while merging `slideio-base` into `slideio-core` (2026-08-23), which moved
eight more declarations onto `SLIDEIO_CORE_EXPORTS` and so widened the
inconsistency's reach. It was deliberately left out of that merge: renaming the
define is a two-file change with no functional effect, and it would have landed
inside a commit whose reviewability depended on staying mechanical.

**Proposed direction:** rename the compile definition to `SLIDEIO_CORE_API` in
`core/CMakeLists.txt` and update the `#if defined(...)` guard in
`slideio_core_def.hpp`. Both are private to the build — `SLIDEIO_CORE` is never
defined by consumers, only by the core target itself — so this is not a breaking
change and needs no `BREAKING_CHANGES.md` entry. Two-line diff, one full build
to verify.

---

## 14. ZVI now reports concurrent reads (resolved)

**Files:** `src/slideio/drivers/zvi/`, `extern/pole`
**Related:** [§4](#4-cvscene-serialises-every-block-read-and-does-so-inconsistently);
`software-docs/specs/2026-09-07-parallel-read-block-design.md` §3.2;
`software-docs/specs/2026-09-09-zvi-concurrent-reads-design.md`;
[§19](#19-pole-read-path-defects-left-in-place),
[§20](#20-the-zvi-concurrent-read-work-what-no-test-covers),
[§21](#21-poles-positional-read-path-is-20-slower-single-threaded)
**Status:** Resolved. `ZVIScene` now reports `supportsConcurrentReads() ==
true`.

This entry's mechanism was right and its cost claim was wrong. It recommended a
`ReadContext` subclass holding a per-thread `ole::compound_document`, on the
grounds that "N x the OLE FAT and directory parse" is "small for a ZVI in a way
it never is for CZI". Measured, one document costs **1721 ms and 12 MB** on
`openslide/Zeiss-3-Mosaic.zvi` (1543 streams). The 0–3 ms the "small" estimate
was evidently drawn from is what the four smaller ZVIs in the corpus cost. At
1721 ms and 12 MB per replica a mosaic reader would have ended up slower than
the mutex the pool was meant to remove, so that route was rejected on the
measurement rather than adopted — see
`software-docs/specs/2026-09-09-zvi-concurrent-reads-design.md` §3.2 and §4.

The route actually taken keeps **one** shared `ole::compound_document` and
makes it safe to read from several threads, which meant changing
`extern/pole` rather than the driver. Three races were on the read path — a
shared `std::fstream` cursor inside `StorageIO`, `StreamImpl::_pos`/`_state`
written by the positional read, and `stream_path::stream()` bumping
`_ref_count` on a borrow — and all three were removed by construction:
`StorageIO`'s read path now goes through a `PositionalFile`
(`ReadFile`-with-`OVERLAPPED` on Windows, `pread` elsewhere),
`StreamImpl::read(pos, ...)` is `const` with a tri-state `eof_report`
out-param, and `stream_path` gained a `const stream()` that borrows without
mutating the count. `ZVIImageItem::readRaster` then borrows `const` through
`ZVIUtils::ConstStreamKeeper` and issues one `read_at` where it used to issue
four `seek`s. `ZVIScene::m_Doc` stays a plain member: no `ContextPool`, no
per-thread document, no extra descriptor per thread. See the design's §5.3 and
§5.4.

Two claims this entry used to make should not be carried forward. The
alternative it offered — "resolve every stream's `(offset, length)` once at
`init()` and read via `FileReader` thereafter" — described moving OLE sector
chains and the mini-FAT into slideio; §5.3 of the design is the same idea done
where that logic already lives, so the alternative is not outstanding, it is
what happened. And the route was not free single-threaded: the positional read
path is about 20% slower per read than the buffered `fstream` it replaced, for
reasons and with numbers recorded in
[§21](#21-poles-positional-read-path-is-20-slower-single-threaded).

---

## 15. DCM still serialises every block read

**Files:** `src/slideio/drivers/dcm/`
**Related:** [§4](#4-cvscene-serialises-every-block-read-and-does-so-inconsistently);
`software-docs/specs/2026-09-07-parallel-read-block-design.md` §3.2
**Status:** Open, deliberately deferred.

`DCMScene` returns `false` from `supportsConcurrentReads()`, so the base class
serialises its reads as it always did. It was deferred because its mutable
read-path state lives inside DCMTK rather than in slideio, and it is not in
the tiling or converter path.

The work is now bounded: add a `ReadContext` subclass holding a `DCMFile`.
`DCMFile::readFrame` builds a fresh `DicomImage` per frame from a shared
`DcmDataset`, and DCMTK's `DcmPixelData` caches decompressed representations
inside that dataset. `DCMFile::createImage` constructs the `DicomImage` as
`DicomImage(dataset, xfer, CIF_UsePartialAccessToPixelData, firstFrame,
numFrames)`, and `CIF_UsePartialAccessToPixelData` is precisely the flag that
makes DCMTK retain partial pixel-data state inside that shared `DcmDataset`
between `DicomImage` constructions — which is why per-thread `DCMFile`
replicas are the only route there. N x parse is expensive here, so choose the
pool's cap accordingly — and measure what one replica costs before choosing
it. `software-docs/specs/2026-09-09-zvi-concurrent-reads-design.md` §3.2 is the
method, and [§14](#14-zvi-now-reports-concurrent-reads-resolved) is the
evidence that it matters: §14 used to assume a per-thread document was cheap
for ZVI, and the measurement came back at 1721 ms and 12 MB each.

The alternative, if this ever matters for throughput: resolve every frame's
encapsulated-pixel-data offset once at `init()` and read via `FileReader`
thereafter, bypassing DCMTK on the read path entirely. That is faster
single-threaded too, but it means owning DICOM encapsulated-pixel-data basic
offset tables.

---

## 16. GDAL still serialises every block read

**Files:** `src/slideio/drivers/gdal/`
**Related:** [§4](#4-cvscene-serialises-every-block-read-and-does-so-inconsistently);
`software-docs/specs/2026-09-07-parallel-read-block-design.md` §3.2
**Status:** Open, deliberately deferred.

`GDALScene` returns `false` from `supportsConcurrentReads()`, so the base
class serialises its reads as it always did. It was deferred because its
mutable read-path state lives inside FreeImage rather than in slideio, and it
is not in the tiling or converter path.

**That driver contains no GDAL.** `GDALScene::m_imagePage` is a
`SmallImagePage*` (`gdalscene.hpp:42`), obtained from `GDALSlide`'s
`m_image->readPage(...)` (`gdalslide.cpp:22`) where `m_image` is a
`SmallImage`. The only implementation of `SmallImagePage` in the tree is
`FIWrapper::Page` (`fiwrapper.hpp:25`), and `fiwrapper.hpp` includes
`<FreeImage.h>`. A tree-wide search for `GDALOpen`, `gdal_priv` and
`GDALAllRegister` returns nothing.

The work is now bounded: add a `ReadContext` subclass holding whatever
`FIWrapper`/`FIWrapper::Page` state the read path shares. The real question is
FreeImage's thread-safety — its plugin registry and `FreeImage_Initialise` are
process-global — plus whatever mutable state `FIWrapper` and `FIWrapper::Page`
hold, so the read granularity wants revisiting at the same time as the pool's
cap is chosen.

The alternative, if this ever matters for throughput: resolve tile
`(offset, length)` once at `init()` and read via `FileReader` thereafter,
bypassing FreeImage on the read path entirely. That is faster single-threaded
too, but it means owning whatever container format `FIWrapper` was
abstracting.

---

## 17. OME-TIFF now reports concurrent reads (resolved)

**Files:** `src/slideio/drivers/ome-tiff/`
**Related:** [§4](#4-cvscene-serialises-every-block-read-and-does-so-inconsistently);
`software-docs/specs/2026-09-07-parallel-read-block-design.md` §3.2;
`software-docs/specs/2026-09-08-ometiff-concurrent-reads-design.md`
**Status:** Resolved. `OTScene` now reports `supportsConcurrentReads() ==
true`.

This entry originally claimed `TIFFFiles::getOrOpen` raced on the read path
and called OME-TIFF "a different failure class" from ZVI, DCM and GDAL. Both
were wrong: `getOrOpen`'s only caller was `TiffData::init` at construction, so
the map was never touched by a read and there was no container race. The
entry's first suggested fix — "give `TIFFFiles` its own lock" — would not have
worked either, since a lock protects the map while leaving the handles it
hands out shared. The real blocker was `TiffData::m_tiff`, a raw
`libtiff::TIFF*` cached during `init` and shared by every `TiffData` naming
the same file — the ordinary single-handle blocker SVS, PHTIFF, PKE, SCN and
NDPI each had.

The fix moved `TIFFFiles` off `OTScene` and into a per-thread `OTReadContext`
held by a `ContextPool`, one collection per context rather than one handle,
because a single tile read can span several `TiffData` naming different
files. See `software-docs/specs/2026-09-08-ometiff-concurrent-reads-design.md`
for the full design and its verification.

---

## 18. CZI rejects a corrupt sub-block position on the main path and tolerates it on the attachment path

**Files:** `src/slideio/drivers/czi/czislide.cpp`
(`readSubBlocks`, `validateSubBlockFilePosition`, `readAttachments`,
`addAuxiliaryImage`)
**Related:** `software-docs/specs/2026-09-07-parallel-read-block-design.md` §4.5.3
**Status:** Open, deliberately deferred.

One driver now has two different answers to "what does a corrupt file do".

`readSubBlocks` guards each directory entry's `filePosition` with
`validateSubBlockFilePosition`, which raises `slideio::RuntimeError` on a
negative value or one that would overflow when added to `originPos`. The call
sits deliberately *outside* the two `try` blocks around it, because those
blocks catch `slideio::RuntimeError` — they were widened to it when the driver
moved off `std::ifstream`, since `FileReader` reports a short read that way —
and log it as a warning that truncates the sub-block list. A data-integrity
problem must not be handled like a transient short read, so on the main path
the corruption fails `CZISlide::init()` hard
(`CZIImageDriver.subBlockFilePositionOverflowPropagates` covers it).

The attachment path reaches the same validator by a different route.
`readAttachments` → `addAuxiliaryImage` → `createCZIAttachmentScenes` calls
`readSubBlocks` with a non-zero `originPos` for a CZI embedded as an
attachment, and **both** of those callers catch `std::exception&`
(`czislide.cpp`, the two handlers around `addAuxiliaryImage` and around the
whole of `readAttachments`). `slideio::RuntimeError` derives from
`std::exception`, so the guard's throw is swallowed there and logged as
"Error reading auxiliary image". The result is a slide that opens successfully
carrying a partially parsed auxiliary image, from a file the main path would
have rejected outright.

Which behaviour is right is a product decision, not a mechanical one, and that
is why this is deferred rather than fixed: an unreadable *auxiliary* image is
arguably not a reason to fail opening the whole slide, whereas an unreadable
main pyramid clearly is. What is not defensible is that the difference is an
accident of which `catch` clause the throw happens to meet.

The work, whichever way it is decided:

1. If the attachment path should also reject: narrow those two handlers so
   they do not catch the integrity error — e.g. give the overflow its own
   exception type, or validate before the `try`, as `readSubBlocks` does.
2. If it should keep tolerating: say so at both handlers, and record that an
   auxiliary image can be dropped from a slide that otherwise opens, so a
   caller iterating `getAuxImageNames()` knows the list can be silently short.

Either way the two paths should be tested together, so the next widening of a
`catch` cannot re-open the gap unnoticed.

---

## 19. pole read-path defects left in place

**Files:** `extern/pole/sources/pole/detail/storage.cpp`,
`extern/pole/sources/pole/detail/stream.cpp`,
`extern/pole/includes/pole/detail/storage.hpp`,
`extern/pole/sources/pole/pole.cpp`, `extern/pole/sources/storage.cpp`,
`extern/pole/includes/path.hpp`
**Related:** [§14](#14-zvi-now-reports-concurrent-reads-resolved);
`software-docs/specs/2026-09-09-zvi-concurrent-reads-design.md` §5.1–§5.3
**Status:** Open, deliberately deferred. Each item is pre-existing or was
scoped out; none is a regression introduced by the concurrent-read work.

Seven defects found while giving pole a positional read path and left alone,
because fixing any of them would have changed behaviour on a change whose whole
value was that it did not.

1. **pole opens every compound document read/write.** `StorageIO(const char*)`
   and `StorageIO(const wchar_t*)` open with
   `std::ios::binary | std::ios::in | std::ios::out`, so slideio cannot open a
   read-only ZVI, or one on read-only media, at all. Fixing it changes *when
   opens succeed* and needs its own test. It is also why an open ZVI holds
   **two** descriptors rather than one: the read/write `std::fstream` plus the
   read-only `PositionalFile` opened alongside it. Two per document, not two
   per thread.
2. **`StreamImpl::_state &= StreamImpl::Eof`** keeps the Eof bit and clears
   `Bad`, where `&= ~Eof` was evidently meant (`stream.cpp:200`, `:225`,
   `:254`, `stream.hpp:68`). Carried across verbatim so the positional-read
   change stayed behaviour-preserving. Nothing in slideio consults either flag.
3. **The cursor `read` dereferences a possibly-null `_entry`.**
   `StreamImpl::read(unsigned char*, std::streamsize)` ends with
   `if( _pos == _entry->size() )` and no null check, and `_entry` comes from
   `io->entry(path)`, which can return null. Pre-existing; the guard was
   deliberately not added, for the same behaviour-preservation reason as (2).
4. **`compound_document::path_exist()` is wrong for nested stream paths.**
   It derives the parent storage with `substr(0, path.size() - ++pos)`
   (`sources/storage.cpp:147`), which for `/Image/Contents` yields `/Image/C`
   and finds nothing. Measured against pole's own `test1.bin`: `false` for all
   fifteen nested streams, while `find_storage` + `find_stream` resolve every
   one of them. slideio does not call it, which is why nothing noticed.
   Anything that starts calling it must fix it first.
5. **`Storage::stream()`'s reuse lookup never matches.** It compares
   `(*it)->path()`, the entry's *short* name, against `name`, which every
   caller passes as a full path (`sources/pole/pole.cpp:77`), so `reuse = true`
   always misses and the `streams` list grows one entry per stream and is
   scanned in full each time. Worth 2 ms of the mosaic's original 1721 ms,
   which is why it was left. It is also not safely fixable in isolation:
   keying on the full path makes reuse start working where it never has, and
   keying on the short name makes every item's `Contents` collide.
6. **`StorageIO::get_entry_childrens` takes `result` by value** and so
   discards everything it collects (`detail/storage.hpp:111`). Dead code — no
   caller in pole, its tests, or the zvi driver.
7. **`StorageIO::create()` leaves `_pread` stale.** It replaces `_file` and
   `_stream` without touching the positional handle
   (`detail/storage.cpp:377-393`), so a read after it would come from the
   previously opened file. Dead code — no caller in pole or slideio — and it
   is on the write path, which the concurrency work was scoped out of.

Fixing (1) is the only one with a user-visible payoff, and it is the one that
needs a new test rather than a one-line edit. (6) and (7) are cheapest fixed
by deletion if pole's write path is ever revisited.

**Stale comments and cosmetics, all one-liners, all left because this change
is not reopening the files they sit in:** the doc comment above the positional
`read` in `includes/pole/detail/stream.hpp` still names the out-param
`hit_eof` after it was renamed `eof_report` and made tri-state; the
`mutable std::mutex _stream_mutex` comment (`detail/storage.hpp:159`) reads
"guards `_stream` when `_pread` is NULL" without the qualifier that `load()`
reads `_stream` unguarded either way, which is safe only because `load()` runs
once during construction, before any `StreamImpl` exists;
`sources/pole/detail/dirtree.cpp` pre-checks `visited[prev]`/`visited[next]`
before recursing, duplicating the check `find_siblings` makes on entry
(harmless, saves a call frame, not worth a commit); and
`src/slideio/drivers/zvi/zviscene.hpp` carries a redundant `public:` label that
predates this work and now sits just after the new `supportsConcurrentReads`
override.

---

## 20. The ZVI concurrent-read work: what no test covers

**Files:** `src/tests/main/test_zvi_driver.cpp`,
`extern/pole/sources/pole/detail/storage.cpp` (`PositionalFile`),
`src/slideio/drivers/zvi/zviimageitem.cpp`, `extern/pole/tests/`,
`CMakeLists.txt`, `.github/workflows/build-validation.yml`
**Related:** [§14](#14-zvi-now-reports-concurrent-reads-resolved);
`software-docs/specs/2026-09-09-zvi-concurrent-reads-design.md` §6
**Status:** Open. Five gaps, recorded so the next change here knows what the
green suites do and do not stand behind.

1. **`PositionalFile::read_at`'s short-read/EOF loop is never entered by any
   test**, and neither is its `ERROR_IO_PENDING` / `GetOverlappedResult`
   branch. pole's 12 tests read a small local NTFS document where every read
   completes synchronously and in full.
   `stream.read_at_past_end_is_clamped` does not discharge this: it exercises
   `StreamImpl::read`'s *logical-size* clamp one layer above, and every block
   loader beneath it requests a full block. Two pole-side unit tests over an
   existing fixture would close it, no thread and no mosaic needed:
   `read_at(size - 4, buf, 64)` and `read_at(0, buf, size)` on the largest
   fixture, the second to force more than one loop iteration.

2. **`ZVIImageItem::readRaster`'s JPEG branch is exercised by no test and
   cannot be with the current corpus.** That branch is selected by header word
   6 (`validBits`) being 0 or 1 (`zviscene.cpp:292-295`,
   `zviimageitem.cpp:211` and `:218`). Every ZVI fixture was scanned for such
   an item:

   | fixture | items | JPEG-branch items |
   |---|---|---|
   | `TOMMAlexaFluor647.zvi` | 1 | 0 |
   | `Zeiss-1-Merged.zvi` | 3 | 0 |
   | `Zeiss-1-Stacked.zvi` | 39 | 0 |
   | `mouse/…RING1B_DAPI_T_005.zvi` | 144 | 0 |
   | `mouse/…HA_DAPI_inj_002.zvi` | 117 | 0 |
   | `openslide/Zeiss-3-Mosaic.zvi` | 759 | 0 |

   1063 items, not one JPEG. The branch was equally untested before this work,
   and the positional-read change to it is equivalence-provable by reading:
   `basic_stream::seek(0, std::ios::end)` computes `size() - 0` and
   `StreamImpl::seek` accepts `pos == _entry->size()`, so the old
   `seek(0, end); pos()` returned exactly `size()`, the two length computations
   are arithmetically identical, and `read_at(off, n)` reads the same bytes as
   `seek(off); read(n)`. The only new behaviour is a short-read check — a new
   error path that can fire only where the old code silently handed a truncated
   buffer to `decodeJpegStream`. **The fix is acquiring a JPEG-compressed ZVI
   fixture**; nothing else closes this.

3. **ThreadSanitizer was never run, and the byte-exactness tests are not a
   replacement for it.** MSVC has no TSan and there is no Linux build on the
   development machine. The intended stand-in — revert `readRaster` to its
   cursor form and watch `concurrentReadsAreByteIdenticalMosaic` go red — was
   tried and the plain revert **passed** three times, because that race's
   window is roughly 50 ns against a ~2 ms read, a duty cycle near 1e-5.
   Widening the window with a `sleep_for(50 microseconds)` in the reverted
   build did make it fail, on exceptions, 75/59/77 occurrences across three of
   the four read paths. So what the three tests support is exactly this: they
   detect read corruption on a shared ZVI scene (demonstrated), and they do not
   reliably catch this specific narrow-window race. The `_ref_count` and
   `_state` races were removed by construction and are unverified by any race
   detector; `_ref_count` has one direct property test at the pole layer
   (`stream.const_borrow_does_not_bump_the_ref_count`), which checks that a
   `const` borrow does not increment the count, not the absence of a race under
   contention. **A Linux CI job running TSan is the real fix.** The existing
   `tsan-linux` job covers only the mechanism-level suites (`FileReader.*`,
   `ContextPool.*`) and cannot run the driver suites, which need the image
   corpus CI does not carry.

4. **The POSIX branch of `PositionalFile` has never been compiled.** The
   development machine is Windows-only, so CI is its first real build. It was
   assessed by reading: every symbol has a matching include, and
   `src/slideio/core/tools/filereader.cpp:186` relies on the same `O_CLOEXEC`
   feature-test on the same two platforms. Still, the first Linux or macOS
   configure is the test.

5. **pole's own test suite is built by no CI job, so the "pole's 12 tests"
   figure above is a one-time manual result, not standing coverage.**
   `CMakeLists.txt:204` sets `PACKAGE_TESTS OFF CACHE BOOL … FORCE`, so
   `storage_tests` is never configured in the slideio build; `extern/pole` has
   no `.github/` of its own; and no workflow under `.github/workflows/`
   mentions pole. The seven tests this branch added therefore ran exactly
   once, in a manual standalone configure, and nothing will run them again on
   any push. Two are load-bearing and have no other coverage anywhere:
   `stream.concurrent_read_at_on_one_document`, the only test that exercises
   the new positional-read primitive under contention, and
   `dirtree.every_reported_path_resolves`, the only test that makes the §5.2
   `find_siblings` rewrite behaviour-preserving rather than merely fast.

   This compounds with item 3 above, which is the real risk: the three ZVI
   byte-exactness tests **are** in CI, but item 3 already records that a
   deliberately reverted `readRaster` passed them three times. So the
   automated regression net this branch leaves behind, after merge, is three
   tests shown unable to detect this class of race, plus seven tests that
   nothing runs.

   The fix does not need the slide corpus, so it can run on every push: a job,
   or a step in an existing Linux job, that configures `extern/pole` standalone
   with `-DPACKAGE_TESTS=ON` and builds and runs `storage_tests`. It needs
   `submodules: recursive` (or `git submodule update --init` inside
   `extern/pole`) to pull the nested googletest submodule that `PACKAGE_TESTS`
   requires — see §6.4 of the design.

---

## 21. pole's positional read path is ~20% slower single-threaded

**Files:** `extern/pole/sources/pole/detail/storage.cpp`
(`StorageIO::loadBigBlocks`, `loadBigBlock`),
`extern/pole/sources/storage.cpp` (`compound_document::find_storage`)
**Related:** [§14](#14-zvi-now-reports-concurrent-reads-resolved);
`software-docs/BREAKING_CHANGES.md`, `v2.10.0`;
`software-docs/specs/2026-09-09-zvi-concurrent-reads-design.md` §5.3, §8
**Status:** Open. The first item is a **measured regression** with an
identified fix, not a nicety; the other two are throughput observations.

**1. Sector coalescing in `loadBigBlocks`, the fix for the regression.**
`loadBigBlocks` issues one `ReadFile`-with-`OVERLAPPED` per 512-byte block —
about 5600 of them for the 2.9 MB `/Image/Item(0)/Contents` in
`openslide/Zeiss-3-Mosaic.zvi` — where the `std::fstream` path it replaced was
buffered; `loadBigBlock` also heap-allocates a one-element vector per block.
Measured with one probe source and one set of flags against both pole
revisions, n=15 warm samples each, one warm-up pass discarded, same session,
reading that same item:

| pole | mean | median | range | throughput |
|---|---|---|---|---|
| pristine `3e64e5a` | 1.80 ms | ~1.76 ms | 1.72–1.99 ms | ~1540 MB/s |
| current `4b49f49` | 2.16 ms | ~2.12 ms | 2.06–2.37 ms | ~1280 MB/s |

The ranges do not overlap, so this is real and not noise: **+20% on time,
−17% on throughput** for a warm 2.9 MB read.

Three things make it acceptable rather than blocking, and they belong with the
number. It is 0.36 ms per 2.9 MB tile. It is recovered immediately by a second
reader thread, which the mutex this work removed made impossible. And on the
same file it is dwarfed by the open-time win: 759 items × 0.36 ms is about
273 ms of extra read against about 1583 ms saved opening the document
(1721 ms → ~138 ms), so even a full-slide **single-threaded** read of the
mosaic is net faster than before.

Coalescing runs of contiguous sectors into one positional read would cut those
~5600 syscalls to a handful and should recover more than the regression. It
was scoped out of the concurrency change deliberately, so that a regression in
it would be separable from the concurrency work; it wants its own commit with
its own before/after. The cold case is worse still and coalescing is the same
fix: the same tile measured 61 MB/s cold against 1437 MB/s warm, which is the
per-block syscall count showing through once the page cache is not absorbing
it.

**2. `compound_document::find_storage` is a linear scan, now on the read
path.** It walks the whole `_storages` tree comparing strings — about 1543
comparisons per `readRaster` on the mosaic, since `ConstStreamKeeper` resolves
a path per item. It mutates nothing, so this is throughput, not correctness,
and it wants its own measurement before anyone restructures the tree into a
map.

**3. A multi-block short read now shifts the destination.** `loadBigBlocks`
advances `bytes` by the actual count returned, so if block *i* reads short,
blocks *i+1..n* land at the wrong offsets; the old `bytes += p` preserved the
alignment. Only `load()` passes a chain, and it ignores the return value; the
path is reachable only on a truncated or erroring file, where both the old and
the new code produce junk, and the `pos + p > _size` clamp guarantees a full
read for the legitimate last block. The call site already carries a comment
explaining why it advances by the true count; what it does not say is that the
old code's alignment was a property, so one more sentence there is the whole
fix.
