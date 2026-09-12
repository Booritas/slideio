# SlideIO Technical Debt

Running log of known technical debt: latent bugs, unsafe abstractions, and
refactoring opportunities identified during development that are not yet
scheduled. Each entry records the problem, impact, and a proposed direction so
the work can be picked up later without re-doing the analysis.

Every entry below was re-verified against the tree on 2026-09-09; the ones that
turned out to be fixed were removed and are listed, with the evidence, under
[Resolved and removed](#resolved-and-removed).

---

## Table of Contents

Numbers are stable identifiers cited from outside this file, so a fixed entry is
removed without renumbering the rest and its number is retired in
[Resolved and removed](#resolved-and-removed) rather than reused.

1. [`TIFFKeeper` and `NDPITIFFKeeper` are two classes with one contract](#1-tiffkeeper-and-ndpitiffkeeper-are-two-classes-with-one-contract)
2. *retired -- fixed, see [Resolved and removed](#resolved-and-removed)*
3. [`SCNSlide` passes a `TIFF*` where `SVSSmallScene` expects a `bool`](#3-scnslide-passes-a-tiff-where-svssmallscene-expects-a-bool)
4. [`CVScene` still serialises every block read for two drivers](#4-cvscene-still-serialises-every-block-read-for-two-drivers)
5. [`ImageTools::computeSimilarity2` cannot handle more than four channels](#5-imagetoolscomputesimilarity2-cannot-handle-more-than-four-channels)
6. [`CZIScene::getRect()` returns non-zero-based coordinates that block reads cannot use](#6-cziscenegetrect-returns-non-zero-based-coordinates-that-block-reads-cannot-use)
7. [`SCNScene::getRect()` has the same problem](#7-scnscenegetrect-has-the-same-problem)
8. [SCN and OME-TIFF level selection assume parallel pyramid geometry across dimensions](#8-scn-and-ome-tiff-level-selection-assume-parallel-pyramid-geometry-across-dimensions)
9. [`TilerData::relativeZoom` is dead, and its new formula is only valid in one case](#9-tilerdatarelativezoom-is-dead-and-its-new-formula-is-only-valid-in-one-case)
10. [`zSliceRange` / `timeFrameRange` are documented backwards in `cvscene.hpp`](#10-zslicerange--timeframerange-are-documented-backwards-in-cvscenehpp)
11. [`TransformerScene` has no level table, so transformed scenes cannot be read by level](#11-transformerscene-has-no-level-table-so-transformed-scenes-cannot-be-read-by-level)
12. [`SCNScene::getChannelDirectories` indexes unchecked, and the 4D level path widens the exposure](#12-scnscenegetchanneldirectories-indexes-unchecked-and-the-4d-level-path-widens-the-exposure)
13. [`slideio-core`'s export-control define breaks the project naming convention](#13-slideio-cores-export-control-define-breaks-the-project-naming-convention)
14. *retired -- fixed, see [Resolved and removed](#resolved-and-removed)*
15. [DCM still serialises every block read](#15-dcm-still-serialises-every-block-read)
16. [GDAL still serialises every block read](#16-gdal-still-serialises-every-block-read)
17. *retired -- fixed, see [Resolved and removed](#resolved-and-removed)*
18. [CZI rejects a corrupt sub-block position on the main path and tolerates it on the attachment path](#18-czi-rejects-a-corrupt-sub-block-position-on-the-main-path-and-tolerates-it-on-the-attachment-path)
19. [pole read-path defects left in place](#19-pole-read-path-defects-left-in-place)
20. [The ZVI concurrent-read work: what no test covers](#20-the-zvi-concurrent-read-work-what-no-test-covers)
21. [pole's positional read path is ~20% slower single-threaded](#21-poles-positional-read-path-is-20-slower-single-threaded)

Not debt, recorded so it stays a decision:
[Consciously accepted, not debt](#consciously-accepted-not-debt).

---

## 1. `TIFFKeeper` and `NDPITIFFKeeper` are two classes with one contract

**Files:** `src/slideio/imagetools/tiffkeeper.hpp`/`.cpp`,
`src/slideio/drivers/ndpi/ndpitiffkeeper.hpp`/`.cpp`
**Related:** `software-docs/specs/2026-08-15-tiffkeeper-ownership-design.md`
**Status:** Open — the one surviving item of what was originally a nine-item
entry. The RAII work itself is done and its eight other items are gone from
this file; see [Resolved and removed](#resolved-and-removed).

Both classes are now move-only owning handles: copy deleted, move
constructor/assignment, `reset()`/`release()` in place of the leaking
`operator=(libtiff::TIFF*)`, a shared message-handler init path used by both
constructors, no implicit `operator libtiff::TIFF*()`, `TIFFKeeperPtr` a
`using` rather than a macro, and `m_hFile` default-initialised. `TIFFKeeper`
landed across `53d13332..06c456a7`; `NDPITIFFKeeper` was brought up to the same
contract on `v2.10.0`, moving out of `ndpitifftools.hpp`/`.cpp` into its own
files, and is covered by `src/tests/ndpi/test_ndpitiffkeeper.cpp`, which
mirrors `test_tiffkeeper.cpp`.

What remains is that they are still **two** classes with one contract, kept in
step by hand — the class comment in `ndpitiffkeeper.hpp` says so and points
here. Collapsing them means a header-only handle template parameterised by
close function and handler type, because the NDPI driver links its own patched
libtiff, routes messages through `NDPITIFFMessageHandler` rather than
`TIFFMessageHandler`, and does not have `slideio-imagetools` in its link
closure.

Three things found while doing the NDPI half are worth carrying here, because
none of them is in the design spec:

- **`NDPITiffTools::closeTiffFile` had no null guard** and called
  `libtiff::TIFFClose` unconditionally, unlike `TiffTools::closeTiffFile`.
  Reached with `nullptr` it was an access violation (observed: SEH
  `0xc0000005`). `~NDPITIFFKeeper` guarded itself, so the crash was reachable
  only through the free function — which `~NDPIFile` called directly. Guarded
  now, with a test.
- **Installing the keeper's handler is a behaviour change for direct
  `NDPITiffTools` callers:** libtiff errors now throw rather than printing to
  stderr. The driver's own four entry points (`ndpiimagedriver.cpp`,
  `ndpiscene.cpp` ×3) already installed a handler as a stack local, so nothing
  changed on any driver path; the affected callers are chiefly tests reaching
  `NDPITiffTools` directly.
- **An ordering trap the keeper's handler does not close:** in
  `NDPITIFFKeeper keeper(NDPITiffTools::openTiffFile(path))` the file is opened
  while the argument is evaluated, *before* the constructor body installs the
  handler, so whatever handler is already current reports any problem with that
  open. Opening through the `filePath` constructor or `openTiffFile()` has no
  such gap.

---

## 3. `SCNSlide` passes a `TIFF*` where `SVSSmallScene` expects a `bool`

**File:** `src/slideio/drivers/scn/scnslide.cpp` — the `new SVSSmallScene(...)`
call inside `constructScenes`, which carries a `TECH_DEBT #3` comment
**Related:** `src/slideio/drivers/svs/svssmallscene.hpp`, the `SVSSmallScene`
constructor declaration
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

## 4. `CVScene` still serialises every block read for two drivers

**File:** `src/slideio/core/cvscene.cpp`
**Related:** issue #69, the 2026-08-16 explicit-level-reading plan,
`software-docs/specs/2026-09-07-parallel-read-block-design.md`
**Status:** Partially fixed, and what is left of it is DCM and GDAL. The
`assemble4DBlock` asymmetry this entry also recorded is resolved outright: both
branches now take `lockIfSerialised()` (`cvscene.cpp:150`, `:156`), with the
lock scoped to the `readPlane` call only in the multi-plane branch. The
serialisation itself is removed for the scenes of SVS, PHTIFF, AFI, PKE, SCN,
NDPI, CZI, VSI, OME-TIFF and ZVI — the last two per
`software-docs/specs/2026-09-08-ometiff-concurrent-reads-design.md` and
`software-docs/specs/2026-09-09-zvi-concurrent-reads-design.md`. Kept open
because DCM and GDAL still serialise every block read — see
[§15](#15-dcm-still-serialises-every-block-read) and
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
(`otscene.cpp:145-155`) builds `m_levels` from
`m_tiffData.front().getTiffDirectory(0)` alone, then `readTile`
(`otscene.cpp:417`, `:423-425`) takes that single `zoomLevel` and applies it
to **every** `TiffData` entry that `collectTiffDataIndices` selected for the
requested channel/z/t (`for (int index : blockInfo->tiffDataIndices) { ...
tiffData.readTile(channelIndices, zSlice, tFrame, zoomLevel, tileIndex, files,
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

## 10. `zSliceRange` / `timeFrameRange` are documented backwards in `cvscene.hpp`

**File:** `src/slideio/core/cvscene.hpp:145`, `:158`, `:171`, `:185`
**Related:** `src/slideio/slideio/scene.cpp:25-29` (`tupleToRange`);
`src/slideio/slideio/scene.hpp` (fixed)
**Status:** Open, and now located. The public `scene.hpp` comments were
corrected during the 2026-08-16 explicit-level-reading documentation pass, and
that entry noted the wrong phrasing "may still be out there" without searching
for it. A tree-wide search on 2026-09-09 found it: all four `zSliceRange`
comments in `src/slideio/core/cvscene.hpp` still say
`std::tuple<indexOfFirstSliceToRead,numberOfSlicesToRead>`, where `scene.hpp`
now says `indexAfterLastSliceToRead`. Nothing else in the tree repeats it
(the remaining hits are the 2026-08-16 plan and this file).

The comments describe `std::tuple<indexOfFirstSliceToRead,
numberOfSlicesToRead>` — a `<start, count>` pair — but `tupleToRange` builds
`cv::Range(get<0>, get<1>)`, a `<start, end>` pair. They coincide only when
start is 0. The Python layer documents it correctly as "(first, last+1)" and
computes `numSlices = stop - start`, so the code is right and only the C++ doc
comments are wrong, on every method taking those parameters.

The fix is a four-line comment-only edit in `cvscene.hpp`, copying the wording
`scene.hpp` already carries. Left as an entry rather than folded into this
review because the review deliberately changed no code.

---

## 11. `TransformerScene` has no level table, so transformed scenes cannot be read by level

**File:** `src/slideio/transformer/transformerscene.cpp`/`.hpp`
**Related:** `src/slideio/transformer/transformer.cpp:15`, `:25`
(`transformScene`/`transformSceneEx`); `src/slideio/core/cvscene.cpp:222-228`
(`validateLevel`, the throw site)
**Status:** Open. Found during the whole-branch review for the 2026-08-16
explicit-level-reading plan.

`TransformerScene` never populates `m_levels` and does not override
`getNumZoomLevels()`, so it reports 0 zoom levels via `CVScene`'s default.
`transformScene`/`transformSceneEx` (`transformer.cpp:15`, `:25`) hand back a
public `slideio::Scene` wrapping a `TransformerScene`, so any caller doing a
level-addressed read against a transformed scene hits `CVScene`'s guard and
gets `"... does not report any zoom level and cannot be read by level"`
(`cvscene.cpp:222-228`).

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

**File:** `src/slideio/drivers/scn/scnscene.hpp:99-102`
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

## 15. DCM still serialises every block read

**Files:** `src/slideio/drivers/dcm/`
**Related:** [§4](#4-cvscene-still-serialises-every-block-read-for-two-drivers);
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
method, and ZVI is the evidence that it matters: that work started from the
assumption that a per-thread `ole::compound_document` would be cheap, and the
measurement came back at 1721 ms and 12 MB per replica on
`openslide/Zeiss-3-Mosaic.zvi` (1543 streams) — which is why ZVI ended up
sharing one document rather than pooling replicas.

The alternative, if this ever matters for throughput: resolve every frame's
encapsulated-pixel-data offset once at `init()` and read via `FileReader`
thereafter, bypassing DCMTK on the read path entirely. That is faster
single-threaded too, but it means owning DICOM encapsulated-pixel-data basic
offset tables.

---

## 16. GDAL still serialises every block read

**Files:** `src/slideio/drivers/gdal/`
**Related:** [§4](#4-cvscene-still-serialises-every-block-read-for-two-drivers);
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
**Related:** the ZVI concurrent-read work (§14, now closed — see
[Resolved and removed](#resolved-and-removed));
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
   (`sources/storage.cpp:164`, inside `path_exist` at `:147`), which for
   `/Image/Contents` yields `/Image/C` and finds nothing. Measured against
   pole's own `test1.bin`: `false` for all fifteen nested streams, while
   `find_storage` + `find_stream` resolve every one of them. slideio does not call it, which is why nothing noticed.
   Anything that starts calling it must fix it first.
5. **`Storage::stream()`'s reuse lookup never matches.** It compares
   `(*it)->path()`, the entry's *short* name, against `name`, which every
   caller passes as a full path (`sources/pole/pole.cpp:98`, in
   `Storage::stream` at `:78`), so `reuse = true` always misses and the
   `streams` list grows one entry per stream and is scanned in full each time. Worth 2 ms of the mosaic's original 1721 ms,
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
**Related:** the ZVI concurrent-read work (§14, now closed — see
[Resolved and removed](#resolved-and-removed));
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
**Related:** the ZVI concurrent-read work (§14, now closed — see
[Resolved and removed](#resolved-and-removed));
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

---

## 22. The colour/ICC extraction work: what no test covers

**Files:** `src/tests/main/test_scn_driver.cpp`, `src/tests/pke/test_pke_driver.cpp`,
`src/tests/vsi/test_vsi_driver.cpp`, `src/tests/ometiff/test_ometiff_colorprofile.cpp`,
`src/tests/main/test_dcm_driver.cpp`, `src/tests/main/test_icctransform.cpp`
**Related:** `software-docs/specs/2026-09-12-color-icc-api-design.md`;
the colour/ICC public API work on `v2.10.0`
**Status:** Open. Three gaps, recorded so the next change here knows what the
green suites do and do not stand behind.

1. **Four drivers' ICC wiring is verified by no test that could fail.** The scn,
   pke, vsi and ome-tiff scenes each carry a `setColorProfile(ColorProfile(...))`
   call, and each has a test asserting an unprofiled scene reports an empty
   profile. Those tests pass with the production line deleted. The reason is
   structural, not sloppiness: `ColorProfile(emptyVector)`, a default-constructed
   `ColorProfile()`, and the base `CVScene::getColorProfile()` are observably
   identical — `isEmpty()` true, source `None` — so on a file carrying no tag
   the wired and unwired cases cannot be told apart. Each line was reverted and
   the suite re-run to confirm this.

   The cause is the corpus, and merging every image root did not change it. All
   TIFF-family fixtures were walked at the IFD level for `TIFFTAG_ICCPROFILE`
   (34675), independently of the library under test:

   | format | files scanned | carrying a profile |
   |---|---|---|
   | scn | 4 | 0 |
   | pke (QPTIFF) | 5 | 0 |
   | vsi | 27 | 0 |
   | ome-tiff | 133 | 0 |
   | philips (PHTIFF) | 4 | 0 |
   | hamamatsu (NDPI) | 10 | 0 |
   | afi | 5 | 0 |
   | svs | 14 | **4** |

   Only SVS has a profiled fixture, so only SVS has real positive coverage
   (`JP2K-33003-1.svs`, 141,992 bytes). **Acquiring a profiled fixture of any of
   the four formats closes this** — but so does a synthetic path, and three
   already exist in-tree to copy from: PHTIFF injects a profile through the real
   `createImageScene`/`createAuxScenes` using the existing `MockPHTIFFSlide`
   harness; NDPI writes a synthetic single-directory TIFF
   (`src/tests/ndpi/synthetic_tiff.hpp`); DCM builds a synthetic WSI object with
   DCMTK's own write API. Each of those three is falsifiable; the four here are
   not.

2. **One DCM test is non-falsifiable by construction and says so in its name.**
   `DCMImageDriver.colorProfileAbsentPathOnly_notFalsifiableForEmbedded`
   (`test_dcm_driver.cpp`) runs against a plain radiograph that cannot carry the
   tag, so its `Embedded` branch never executes. It is kept as a regression
   guard against a driver that starts fabricating profiles, not as coverage, and
   is named that way so no future reader mistakes it. DCM's real coverage is two
   other tests: a corpus WSI aux image with ground truth extracted via raw DCMTK
   calls that bypass `DCMFile`, and the synthetic WSI object above.

3. **`ColorProfileInfo::manufacturer`, `::model` and `::intent` are parsed but
   asserted by no test.** `IccTransform::describe()` populates all three; no test
   in the tree reads them. The adjacent field `version` had a truncation bug
   found only in review — `cmsGetProfileVersion()` returns a `cmsFloat64Number`
   and was cast to an unsigned int, reporting `"4"` for `"4.4"` — which is
   exactly the class of defect an unasserted field hides. A single test over the
   synthetic sRGB profile, whose values are known, would close all three.

---

## 23. DCMTK codec registration is process-wide but tied to one instance's lifetime

**Files:** `src/slideio/drivers/dcm/dcmimagedriver.cpp`
(`initializeDCMTK`/`clieanUpDCMTK`, the constructor and destructor),
`src/slideio/core/imagedrivermanager.cpp`
**Related:** found while adding ICC extraction to the dcm driver on `v2.10.0`;
not caused by that work and deliberately not fixed there
**Status:** Open. Pre-existing, order-dependent, presents as flakiness.

`DCMImageDriver`'s constructor registers the JPEG, RLE and JP2K codecs with
DCMTK's `DcmCodecList`, and its destructor unregisters them. Both are
**process-wide**: `DcmCodecList` is global state, not per-instance.

`ImageDriverManager` caches a single shared `DCMImageDriver`, but nothing stops
other code — the DCM tests do it throughout — from constructing local instances.
When such a local instance goes out of scope, its destructor tears down the codec
registry that the cached driver still depends on, and the next compressed read
through the shared driver fails. Whether it fails depends on construction and
destruction order, so it surfaces as an intermittent failure in an unrelated
test rather than as a clean, attributable error.

There is also an undocumented platform asymmetry: an existing `#ifndef __GNUC__`
guard means the registration lifecycle differs between MSVC and GCC builds, so a
reproduction on one toolchain may not reproduce on the other.

**Reproduction:** run a DCM test that constructs a local `DCMImageDriver` and
lets it destruct, then read a JPEG-compressed DICOM through the driver
`ImageDriverManager` returns, in the same process.

Three fixes, in increasing cost:

1. Refcount the registrations, so teardown happens when the last instance dies
   rather than the first.
2. Funnel construction through a factory so only one instance exists per process
   — which is what the global state already assumes.
3. At minimum, document the `#ifndef __GNUC__` asymmetry where it sits, so the
   next person does not rediscover it from a failing test.

The ICC work sidestepped this by making its own test use a local instance like
the rest of that file. That is a workaround in one test, not a fix.

---

## 24. GDAL and CZI scenes hold raw pointers into slide-owned state

**Files:** `src/slideio/drivers/gdal/gdalscene.hpp` (`m_imagePage`),
`src/slideio/drivers/gdal/gdalslide.cpp` (scene construction),
`src/slideio/drivers/czi/cziscene.hpp` (`m_slide`),
`src/slideio/slideio/scene.hpp` (the lifetime contract),
`D:/Projects/slideio/slideio-python/src/pyscene.hpp` (why Python is immune)
**Related:** hit during the colour/ICC work on `v2.10.0` as non-deterministic
SEH / `bad_alloc` crashes while writing a pixel-reading transformer test
**Status:** Open. Pre-existing, silent for metadata, crashes on pixel reads.

A `Scene` can outlive the `Slide` it came from, and for two drivers that is a
use-after-free rather than merely unsupported:

- `GDALSlide` owns `std::shared_ptr<SmallImage> m_image`, and
  `m_image->readPage(i)` returns a **raw** `SmallImagePage*` owned by that
  `SmallImage`. `GDALScene` stores it as `SmallImagePage* m_imagePage`.
- `CZIScene` stores `CZISlide* m_slide`, also raw.

Nothing in `slideio::Scene` keeps the slide alive — it holds only
`std::shared_ptr<CVScene> m_scene`. So the natural one-liner

```cpp
auto scene = openSlide(path, driver)->getScene(0);   // Slide dies here
scene->readBlock(rect, buffer, size);                // reads freed memory
```

destroys the `Slide` at the end of the first full expression and leaves the
scene pointing at freed state. It is easy to write and hard to notice: metadata
accessors that touch none of the slide-owned state keep working, so the pattern
looks correct until something reads pixels, and then it fails
non-deterministically — as `bad_alloc`, as an SEH exception, or not at all
depending on what reclaimed the memory.

**The documented contract is weaker than the real requirement.** `scene.hpp`
says a `Scene` or the `Slide` it came from "must not be destroyed while a read
on it is still in flight". A reader takes that as *do not destroy mid-read*,
which the one-liner above does not do — the slide is long gone before the read
starts. The actual requirement is that the slide outlive the scene entirely.

**The Python binding is already immune**, and by construction rather than by
luck: `PyScene` holds `std::shared_ptr<slideio::Slide> m_slide` alongside its
scene, so a Python `Scene` keeps its slide alive. Only C++ callers are exposed.

Three fixes, in increasing cost:

1. Sharpen the `scene.hpp` contract to say the slide must outlive the scene, not
   merely the read. Cheapest, and leaves the hazard in place.
2. Have `GDALScene` and `CZIScene` hold a `shared_ptr` to the owner — the
   `SmallImage` and the `CZISlide` respectively — so the dependency is expressed
   in the type system instead of in prose.
3. Have `slideio::Scene` retain its `Slide`, as `PyScene` already does, which
   closes it for every driver at once and makes the documented caveat
   unnecessary.

Only the third removes the class of bug rather than this instance of it.

---

## Consciously accepted, not debt

**PHTIFF detection has no fallback if the claiming driver then fails.** A
`.tif` carrying Philips metadata that the driver cannot fully read used to open
through GDAL — flat, no pyramid — and now throws out of `openSlide`. This is
inherent to `findDriver` and identical for every other format; the
`canOpenFile`-level half of the trade is in
`software-docs/specs/2026-08-11-phtiff-format-detection-design.md` §6. Recorded
here so it stays a decision rather than becoming a discovery.

---

## Resolved and removed

Entry numbers in this file are used as identifiers from outside it — source
comments, a CI job comment, plans and design specs all cite them — so a fixed
entry is removed without renumbering the ones around it, and its number is
retired here rather than reused.

| # | Entry | Verified fixed by | Record |
|---|---|---|---|
| 2 | Philips TIFF driver follow-ups | All nine items landed across `75a48f65..a9f179aa`. Spot-verified: the tile-count and parallel-arrays guards are in `phCropLevelPadding` (`phtiffslide.cpp:278-298`), `svsdriverids.hpp` exists, and `Tools::isXml` is gone from the tree. | `git log --oneline 75a48f65..a9f179aa`; `software-docs/specs/2026-08-11-phtiff-format-detection-design.md`. The one item that was never debt is kept above, under [Consciously accepted, not debt](#consciously-accepted-not-debt). |
| 14 | ZVI serialised every block read | `ZVIScene::supportsConcurrentReads()` returns `true` (`zviscene.hpp:62`), on one shared `ole::compound_document` made safe by pole's positional read path. | `software-docs/specs/2026-09-09-zvi-concurrent-reads-design.md` §3.2, §4, §5.3, §5.4. Its follow-ups are still open as [§19](#19-pole-read-path-defects-left-in-place), [§20](#20-the-zvi-concurrent-read-work-what-no-test-covers) and [§21](#21-poles-positional-read-path-is-20-slower-single-threaded). |
| 17 | OME-TIFF serialised every block read | `OTScene::supportsConcurrentReads()` returns `true` (`otscene.hpp:88`); `TIFFFiles` moved off `OTScene` into a per-thread `OTReadContext` held by a `ContextPool`. | `software-docs/specs/2026-09-08-ometiff-concurrent-reads-design.md`; the contract assertion is `OTImageDriverTests.reportsConcurrentReadSupport`. |

Also removed: eight of the nine sub-items of [§1](#1-tiffkeeper-and-ndpitiffkeeper-are-two-classes-with-one-contract) — the
copy/move semantics, the `operator=` and `openTiffFile` leaks, the
constructor inconsistency, the implicit `TIFF*` conversion, the
`TIFFKeeperPtr` macro, the header hygiene and the missing `m_hFile`
initialiser. All were verified fixed in `tiffkeeper.hpp`/`.cpp` and
`ndpitiffkeeper.hpp`/`.cpp`; the design record is
`software-docs/specs/2026-08-15-tiffkeeper-ownership-design.md`. Only the
unification of the two classes remains open, and §1 is now about that alone.
