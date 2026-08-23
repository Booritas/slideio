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
