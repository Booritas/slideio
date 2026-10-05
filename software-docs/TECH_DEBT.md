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
3. *retired -- fixed, see [Resolved and removed](#resolved-and-removed)*
4. [`CVScene` still serialises every block read for two drivers](#4-cvscene-still-serialises-every-block-read-for-two-drivers)
5. [`ImageTools::computeSimilarity2` cannot handle more than four channels](#5-imagetoolscomputesimilarity2-cannot-handle-more-than-four-channels)
6. [`CZIScene::getRect()` returns non-zero-based coordinates that block reads cannot use](#6-cziscenegetrect-returns-non-zero-based-coordinates-that-block-reads-cannot-use)
7. [`SCNScene::getRect()` has the same problem](#7-scnscenegetrect-has-the-same-problem)
8. [SCN and OME-TIFF level selection assume parallel pyramid geometry across dimensions](#8-scn-and-ome-tiff-level-selection-assume-parallel-pyramid-geometry-across-dimensions)
9. [`TilerData::relativeZoom` is dead, and its new formula is only valid in one case](#9-tilerdatarelativezoom-is-dead-and-its-new-formula-is-only-valid-in-one-case)
10. [`zSliceRange` / `timeFrameRange` are documented backwards in `cvscene.hpp`](#10-zslicerange--timeframerange-are-documented-backwards-in-cvscenehpp)
11. *retired -- fixed, see [Resolved and removed](#resolved-and-removed)*
12. [`SCNScene::getChannelDirectories` indexes unchecked, and the 4D level path widens the exposure](#12-scnscenegetchanneldirectories-indexes-unchecked-and-the-4d-level-path-widens-the-exposure)
13. [`slideio-core`'s export-control define breaks the project naming convention](#13-slideio-cores-export-control-define-breaks-the-project-naming-convention)
14. *retired -- fixed, see [Resolved and removed](#resolved-and-removed)*
15. [DCM still serialises every block read](#15-dcm-still-serialises-every-block-read)
16. [GDAL still serialises every block read](#16-gdal-still-serialises-every-block-read)
17. *retired -- fixed, see [Resolved and removed](#resolved-and-removed)*
18. [CZI rejects a corrupt sub-block position on the main path and tolerates it on the attachment path](#18-czi-rejects-a-corrupt-sub-block-position-on-the-main-path-and-tolerates-it-on-the-attachment-path)
19. [pole read-path defects left in place](#19-pole-read-path-defects-left-in-place)
20. [The ZVI concurrent-read work: what no test covers](#20-the-zvi-concurrent-read-work-what-no-test-covers)
21. [pole read-path throughput: two remaining items](#21-pole-read-path-throughput-two-remaining-items)
22. [The colour/ICC extraction work: what no test covers](#22-the-colouricc-extraction-work-what-no-test-covers)
23. [DCMTK codec registration is process-wide but tied to one instance's lifetime](#23-dcmtk-codec-registration-is-process-wide-but-tied-to-one-instances-lifetime)
24. *retired -- fixed, see [Resolved and removed](#resolved-and-removed)*
25. *retired -- fixed, see [Resolved and removed](#resolved-and-removed)*
26. [The positional read path has no buffer for small sequential reads](#26-the-positional-read-path-has-no-buffer-for-small-sequential-reads)
27. *retired -- fixed, see [Resolved and removed](#resolved-and-removed)*
28. [OME-TIFF plane timestamps have no file that states one](#28-ome-tiff-plane-timestamps-have-no-file-that-states-one)

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

3. **ThreadSanitizer has now been run (2026-09-29) and reported nothing on the
   ZVI paths; the byte-exactness tests are still not a replacement for it.**
   ~~MSVC has no TSan and there is no Linux build on the
   development machine.~~ There is one now: WSL Ubuntu 24.04 on this machine
   builds and runs the suites under TSan, which is how this was closed. The
   ZVI concurrency tests in `slideio_tests` were part of a 38-test pass across
   every concurrent driver and came back clean — see `BREAKING_CHANGES.md`,
   "`Scene` block reads may now overlap", for the conditions and the caveats.

   That result does not retire the rest of this item. It says no race was
   *observed* on the exercised paths, which is weaker than the paragraph below
   asks for: the `_ref_count` and `_state` races were removed by construction,
   and a clean TSan run over the existing tests exercises them only as far as
   those tests reach. The intended stand-in — revert `readRaster` to its
   cursor form and watch `concurrentReadsAreByteIdenticalMosaic` go red — was
   tried and the plain revert **passed** three times, because that race's
   window is roughly 50 ns against a ~2 ms read, a duty cycle near 1e-5.
   Widening the window with a `sleep_for(50 microseconds)` in the reverted
   build did make it fail, on exceptions, 75/59/77 occurrences across three of
   the four read paths. So what the three tests support is exactly this: they
   detect read corruption on a shared ZVI scene (demonstrated), and they do not
   reliably catch this specific narrow-window race. The `_ref_count` and
   `_state` races were removed by construction; the 2026-09-29 TSan pass
   reported nothing against them, but that pass reaches them only through the
   existing tests, so it is evidence and not proof. `_ref_count` has one direct
   property test at the pole layer
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

## 21. pole read-path throughput: two remaining items

**Files:** `extern/pole/sources/pole/detail/storage.cpp`
(`StorageIO::loadBigBlocks`),
`extern/pole/sources/storage.cpp` (`compound_document::find_storage`)
**Related:** the ZVI concurrent-read work (§14, now closed — see
[Resolved and removed](#resolved-and-removed));
`software-docs/BREAKING_CHANGES.md`, `v2.10.0`;
`software-docs/specs/2026-09-09-zvi-concurrent-reads-design.md` §5.3, §8
**Status:** Open. Both remaining items are throughput observations, neither a
regression. The 20% single-threaded read regression this entry was opened for
is **fixed**, on both the big-block and the small-block paths — see *pole
coalesces contiguous block runs into one read* and *pole stops re-reading a
container block per small block* in `BREAKING_CHANGES.md`. A 2.75 MB item of
the mosaic went from 705 positional reads at 2.09 ms to **1 read at 0.25 ms**,
which is also ~7× faster than the `std::fstream` path that preceded the
regression; a 390-byte small stream went from 7 reads to 3, which is the number
of distinct container blocks it actually touches.

Two things the original entry got wrong, recorded because they cost time:
the per-block loop was in `StreamImpl::read`, **not** `loadBigBlocks` — which
on that path is only ever handed a one-element vector, so coalescing there as
prescribed would have changed nothing — and "~5600 blocks" assumed 512-byte
sectors where the mosaic uses 4096 (705 reads). The small-block path had the
identical shape for the identical reason. The cold-cache figure quoted in the
original entry was never re-measured after either fix.

The regression is closed for large reads but **not for every pattern**: many
small sequential cursor reads are still slower than pre-concurrency pole, by
more than the 20% this entry was opened for, and coalescing cannot reach them.
That is [§26](#26-the-positional-read-path-has-no-buffer-for-small-sequential-reads),
recorded separately because the cause is different — an absent buffer rather
than a block walk.

**1. `compound_document::find_storage` is a linear scan, now on the read
path.** It walks the whole `_storages` tree comparing strings — about 1543
comparisons per `readRaster` on the mosaic, since `ConstStreamKeeper` resolves
a path per item. It mutates nothing, so this is throughput, not correctness,
and it wants its own measurement before anyone restructures the tree into a
map.

**2. A multi-block short read shifts the destination in `loadBigBlocks`.**
`loadBigBlocks` advances `bytes` by the actual count returned, so if block *i*
reads short, blocks *i+1..n* land at the wrong offsets; the old `bytes += p`
preserved the alignment. Unaffected by the coalescing fix, which left
`loadBigBlocks` alone: only `load()` passes it a chain, and it ignores the
return value. The path is reachable only on a truncated or erroring file, where
both the old and the new code produce junk, and the `pos + p > _size` clamp
guarantees a full read for the legitimate last block. The call site already
carries a comment explaining why it advances by the true count; what it does
not say is that the old code's alignment was a property, so one more sentence
there is the whole fix.

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

## 26. The positional read path has no buffer for small sequential reads

**Files:** `extern/pole/sources/pole/detail/stream.cpp` (`StreamImpl::read`, both
overloads, and `update_cache`), `sources/pole/detail/storage.cpp`
(`PositionalFile::read_at`)
**Related:** [§21](#21-pole-read-path-throughput-two-remaining-items), whose
coalescing fix cannot reach this; `software-docs/BREAKING_CHANGES.md`,
`v2.10.0`, *pole gained a positional read path* and *pole coalesces contiguous
block runs into one read*
**Status:** Open. A **measured regression** against pre-concurrency pole —
larger than the 20% §21 was opened for, on a pattern §21's fix does not touch.
Found while checking whether the coalescing had closed §21 completely.

Pristine pole read through a buffered `std::fstream`. The positional path that
replaced it has no buffering beneath it: every `read_at` is a syscall. For one
large read that is a win, because the syscall count collapses. For many small
sequential reads it is a loss, because the `fstream` buffer used to absorb them.

Measured against pristine pole `3e64e5a` built from a worktree, same probe
source compiled against both, same session, `/Image/Item(0)/Contents` of
`zvi/openslide/Zeiss-3-Mosaic.zvi`. Content hashes identical throughout.

| Pattern | pristine `3e64e5a` | current `bd6319e` | |
|---|---|---|---|
| `compound_document` open | 1721.5 ms | 139.0 ms | **12× faster** |
| one 2.75 MB read (n=15 warm) | 1.72 ms, 1604 MB/s | 0.24 ms, 11418 MB/s | **7.2× faster** |
| 20000 × 4-byte cursor reads | 1.849 µs each | 2.430 µs each | **+31% slower** |
| 20000 × 64-byte cursor reads | 1.919 µs each | 2.808 µs each | **+46% slower** |

**Coalescing cannot fix this, which is why §21's work left it behind.** A run of
contiguous blocks is collapsed into one read only within a single `read` call;
a 4-byte read spans one block, so there is nothing to coalesce. The cost is the
absent buffer, not the block walk.

**Who hits it.** The split inside the ZVI driver is exact: pixels go through
`read_at` (`zviimageitem.cpp:223`, `:237`) and are firmly on the winning side of
the table, while metadata and tag parsing go through the cursor `read`
(`zviutils.cpp:39`, reached from `skipItems`, `readIntItem`, `readItem`) and are
on the losing side. Any other consumer parsing structured data field by field is
in the same position.

**Net effect is probably still a large win, but the second half of that is an
estimate and should not be quoted as measured.** End-to-end `openSlide` on the
mosaic is ~2750 ms, of which only 139 ms is `compound_document` construction;
the remaining ~2610 ms is tag parsing, dominated by small reads. Scaling that
back by the measured penalty puts pristine near 1865 ms of parsing plus its
1721 ms open, so about 3590 ms against today's 2750 ms. A measured end-to-end
comparison is **not available**: pristine pole has no `read_at`, so the current
ZVI driver cannot be built against it, and getting the number would mean
reverting the driver too.

**ZVI no longer pays this, but pole still has it.** The driver now reads each
metadata stream once into a `ZVIUtils::BufferedStream` and parses from memory,
which took `openSlide` on the mosaic from ~2750 ms to ~1130 ms — a 2.4×
speedup, and well beyond the ~700 ms the estimate above projected, because each
parsed field cost a whole `StreamImpl::read` block walk and not merely the
syscall. See *ZVI parses metadata from a buffer* in `BREAKING_CHANGES.md`. That
fixes the consumer, not the cause: this entry stays open because any other pole
consumer parsing field by field still meets it, and because the numbers in the
table above are still what pole does.

**The fix, and the constraint that shapes it.** A read-through buffer belongs on
the **cursor** path only. `StreamImpl::read(unsigned char*, std::streamsize)` is
non-`const` and single-threaded by contract, and `StreamImpl` already carries
`_cache_data`/`update_cache()` — used today by `getch()` alone — which is the
natural place to put it. What must **not** happen is buffering inside
`StreamImpl::read(size_t pos, ...) const`: that is the positional overload
`read_at` forwards to, it is called concurrently by design, and a shared mutable
buffer there would reinstate exactly the race the whole concurrency conversion
removed. Wants its own before/after on both patterns, since a buffer that fixes
the small-read case must not slow the large-read case back down.

---

## 28. OME-TIFF plane timestamps have no file that states one

**Files:** `src/slideio/drivers/ome-tiff/ottools.cpp` (`collectPlaneTimestamps`)
**Related:** `BREAKING_CHANGES.md`, `ometiff-plane-times`.
**Status:** Open, and blocked on corpus rather than on code. Recorded so the next
person does not repeat the survey.

`CVScene::getPlaneTimestamp()` is implemented for VSI, ZVI, CZI, DCM and
OME-TIFF. Four of the five are backed by files that state the times. OME-TIFF is
not:
`LAMBDA-ModuloAlongZ-ModuloAlongT` states all 50 `Plane` elements and
`SPIM-ModuloAlongZ` all 192, and **none carries `DeltaT`**. The code is written
and the schema is unambiguous; it wants a file -- anything Bio-Formats wrote from
a time-lapse, where `DeltaT` is routine.

**Two CZI branches the corpus does not reach**, now that the driver reads the
sub-block metadata:

- a file whose stated `AcquisitionDateAndTime` is *later* than its earliest
  plane, which drops the acquisition time rather than let the two getters
  disagree;
- a main scene that states an `AcquisitionDateAndTime` but no per-plane times,
  which reports the acquisition time anyway. Every corpus file states either
  both or neither.

Both mirror `OTScene::initializePlaneTimes`, where they are reachable from
OME-XML a test can write directly. A CZI cannot be written that way here, so
they are carried on the OME-TIFF tests. The third branch of that trio -- a scene
that states sub-block times and no start, which takes its earliest plane as the
origin, as Bio-Formats does (`ZeissCZIReader`:
`if (startTime == null) startTime = p.timestamp;`) -- **is** covered: the slide
preview of `jxr-16bit-4chnls.czi` is exactly that, an attachment whose own
metadata the driver does not parse.

**NDPI, SCN and PKE state neither, and that is a finding rather than a gap.**
All three were surveyed on this entry's account and are not on the list above
because the formats have nothing to read, not because nobody looked. All three
are TIFFs, and the shape repeats: no tag for significant bits anywhere in TIFF,
and a scan time that belongs to the file rather than to a plane.

NDPI:

- TIFF has no tag for significant bits at all. BitsPerSample is the storage
  width `getChannelDataType()` already reports, and every NDPI in the corpus is
  8 bits per sample with SampleFormat unset. Bio-Formats reads nothing further.
- Every directory of every corpus NDPI states the same TIFFTAG_DATETIME, and
  `NDPIScene` models one plane -- `getNumZSlices()` and `getNumTFrames()` are
  both 1, and a read of any other index throws. One plane and one time leaves a
  per-plane timestamp nothing to distinguish.

SCN, which is a TIFF too and comes out the same way:

- BitsPerSample again, 8 with SampleFormat unset in every corpus file.
- The xml states one `<creationDate>` per `<image>` and nothing per plane: a
  `<dimension>` carries `c`, `z`, `r` and `ifd` and no time. Not one directory
  of any corpus SCN carries TIFFTAG_DATETIME either -- 0 of 18, 0 of 410, 0 of
  53 -- so there is nothing in the TIFF layer to fall back on, and a z-stack's
  slices are positions in depth rather than in time.

PKE (qptiff), where the xml states two things that look like the answers and
are not:

- `<Bits>12</Bits>` is inside `<ScanProfile><root><CameraSettings>`: the
  camera's digitisation depth, not the stored sample's. Both corpus files are
  `DT_Byte`, so 12 would exceed the width of the sample it claims to describe.
- `<Date>` is inside `<Responsivity><Filter>`: when that filter was calibrated.
  `LuCa-7color_Scan1.qptiff` states five, one per filter, days apart from each
  other and from the scan -- DAPI's is 2017-09-25 against a scan of 2017-10-05.
- `<ExposureTime>` is a duration per channel, which is all Bio-Formats takes it
  for (`VectraReader`, `setPlaneExposureTime`).

The scan time is in TIFFTAG_DATETIME, in all 33 directories of the one and all
8 of the other, one value each.

SVS and PHTIFF are not in this group at all, and the first version of the SVS
work wrongly put it here. Aperio states **both** answers in its image description: the scan time
as `Date = 12/29/09|Time = 09:59:15`, and the significant bits as
`Acquisition Bit Depth = 10`, which `jp2k_1chnl.svs` carries against 16-bit
samples. The survey behind that first version read the TIFF tags and stopped,
even though the same change was already parsing the Aperio properties for the
time -- and it pinned the wrong answer in a test before review caught it. Only
`getPlaneTimestamp()` is genuinely absent from SVS: the header states one time
per slide and `SVSScene` models one plane.

**The lesson, and it is the second time this entry has recorded one like it:**
a format that keeps metadata in two places needs both read before anything is
recorded as absent. The ZVI note below says a reference implementation can
supply what the corpus cannot; this one says the corpus can supply what a
partial look at it cannot.

PHTIFF states both as well, in DICOM attributes by another spelling:
`DICOM_ACQUISITION_DATETIME` (0008,002A) on the `DPUfsImport` root and
`DICOM_BITS_STORED` (0028,0101) on the whole slide image's `DPScannedImage` --
and on that one alone: the auxiliary images declare no pixel format attributes,
so they report 0. It carries a decoy
worth naming beside PKE's: `DICOM_DATE_OF_LAST_CALIBRATION` with
`DICOM_TIME_OF_LAST_CALIBRATION` sits on the same root element and is the
scanner's calibration -- four and a half minutes before the scan in
`Philips-4.tiff`, which is close enough to pass a test that only checked the
value looked like a date. Only `getPlaneTimestamp()` is absent from PHTIFF: one
time per slide, one plane per scene.

GDAL states no significant bits either -- no tiff tag, no png `sBIT` in the
corpus, nothing from FreeImage beyond a bit depth -- and it carries the
subtlest decoy of the set, because the decoy and the answer are the same tag
number. TIFF defines 306 as the time of image creation, which is why NDPI, PKE
and the tiff half of GDAL read it. Exif defines 0x0132 as the *file change*
date, and FreeImage publishes a png's `tIME` chunk -- last modification by the
png spec's own wording -- under that same key. So the FreeImage half reads
`DateTimeOriginal` (0x9003) and nothing else, and both corpus files that carry
a date-looking value report 0. No file states `DateTimeOriginal`, so that half
is carried by the rule and by a tiff the test writes.

AFI needed no work of its own: an `.afi` is an index over one `.svs` per channel
and its scenes are `SVSScene` objects, so it inherited both getters from the SVS
work. `fs.afi` is 10 bits in 16-bit samples across three files of one scan.
Tests pin it anyway -- disabling the SVS source fails them -- because nothing
else would catch a future change to how `AFISlide` builds its scenes.

`getAcquisitionTime()` on `NDPIScene`, `SCNScene`, `PKEScene`, `SVSScene` and
`PHTIFFTiledScene` reads what the formats do state, and a
`noPlaneTimestampsOrSignificantBits` test in the first three pins the other two
getters at 0 and false so none of those decisions is quietly reversed by someone
wiring BitsPerSample -- or `<Bits>` -- in. SVS's test of that name now pins only
the plane timestamps, and says in so many words that its 0 for the bits is that
one file stating none; PHTIFF has a `noPlaneTimestamps` test for the same reason.

**What the drivers agree on, and where they differ on purpose.** A consistency
review across all thirteen scene classes settled the following, so the next
person changing one of them knows which differences are load bearing:

- Every implementation of `getChannelSignificantBits()` bounds its channel
  index and returns 0 for one out of range, and every driver that reads a
  stated depth refuses one wider than the sample holds. 0 means unknown
  throughout; none of them clamps to the storage width, because that would be
  indistinguishable from a file saying every stored bit is significant.
- Every driver with plane timestamps builds its vector only on full coverage,
  so `hasPlaneTimestamps()` cannot disagree with the getter, and every one
  guarantees a non-negative offset -- three by checking the origin against the
  earliest plane and dropping the acquisition time when it fails, one by
  deriving the origin from the planes.
- `TransformerScene` forwards all four getters, so wrapping a scene in a
  transform keeps them.

Deliberately different, and not to be "fixed" into agreement:

- **Auxiliary images.** CZI and DCM give theirs no acquisition time because the
  label and the preview are separately acquired files embedded in the slide,
  with metadata the driver does not parse. SVS, PKE and PHTIFF give theirs the
  slide's, because there the label and macro come off the same scan run.
- **DCM reports 0 for a palette image**, where `BitsStored` is the width of the
  index into the lookup table rather than of a sample.
- **VSI accepts a per-time-frame timestamp list** broadcast across channels and
  z, where the others require one value per plane. OME-TIFF does something
  similar across `SamplesPerPixel`.

**What ZVI needed, and why it is no longer here.** Its tag was enumerated in
`zvitags.hpp` and appeared in no corpus file, so the *encoding* was unknown --
`int32`, a `double` serial date, a string were all plausible and each converts to
an epoch differently. Implementing against a guess would have meant tests that
confirm the guess, which is how the `PhysicalSizeT` defect reached `master`.
Bio-Formats settled it: `BaseZeissReader.parseTimestamp` reads a serial date,
days since 1900-01-01 with that date as day 1 and Excel's phantom 1900-02-29
past day 60. That is verifiable independently of our own code -- serial 40000 is
2009-07-06 -- so the conversion is unit-tested against an outside oracle even
though no file we hold exercises the path end to end.

**What retired CZI from this entry** was neither of those: the blocker was that
the sub-block metadata was never read, so nobody could say whether the corpus
stated the times. It does. `T_3_CH_2.czi` states six `<AcquisitionTime>` values
against three time frames and two channels, and `doughnut.czi` states 87 across
the tiles of a single plane. **The lesson for what is left: a blocker phrased as
"the corpus has nothing" is worth re-reading, because sometimes it is really "we
never looked".**

---

## 29. ThreadSanitizer reports four data races in OpenCV's thread pool, reached through `TransformerScene::applyChain`

**Files:** `src/slideio/transformer/transformerscene.cpp:274`,
`src/tests/transformer/test_readlock.cpp`
**Related:** `BREAKING_CHANGES.md`, "`Scene` block reads may now overlap";
§20.3 for why a passing test is not evidence of absence here
**Status:** Open, uninvestigated. Found by the 2026-09-29 TSan pass; not a
regression — nothing had ever looked.

The TSan run that closed the OME-TIFF gate found every driver suite clean and
`slideio_transformer_tests` **not** clean: four data races, while all four of
its concurrency tests **passed**. That combination is the whole point of §20.3 —
a green suite is not evidence of absence.

All four reports are the same shape:

```
SUMMARY: ThreadSanitizer: data race
  /usr/include/c++/13/ext/atomicity.h:66 in __gnu_cxx::__exchange_and_add(int volatile*, int)
```

The raced memory is the reference count of a `shared_ptr<cv::ParallelJob>` —
OpenCV's thread-pool job object, allocated in `cv::ThreadPool`
(`modules/core/src/parallel_impl.cpp`) and reached from
`TransformerScene::applyChain` (`transformerscene.cpp:274`) calling OpenCV's
`parallel_for_`. They occur in
`TransformedSceneReadLock.twoTransformsOverOneOriginDoNotReadItConcurrently`
(2 races) and
`TransformedSceneReadLock.aConcurrentOriginIsStillReadConcurrentlyThroughATransform`
(2 races) — that is, when two threads drive `parallel_for_` through a transform
at the same time. slideio's own read lock is held across the access.

**What this is not.** It is not in the `ContextPool`/`FileReader` machinery the
2.10 concurrency work added, and not in any driver: those 34 concurrency tests
came back with zero warnings. It is in third-party code that slideio calls from
more than one thread.

**Why it was invisible until now, and why CI will not see it.** The `tsan-linux`
job's comment states that Conan's dependencies "are prebuilt binaries and are
NOT instrumented — TSan will miss races inside them". That is true of that job
and false of the run that found these: Conan **built OpenCV from source** in the
WSL environment, so it inherited `-fsanitize=thread` and was statically linked
into `libslideio-transformer_d.so` (80 `cv::ThreadPool`/`cv::ParallelJob`
symbols in the library). Any future run wanting to see this class must force
OpenCV to build from source; a cache hit on a prebuilt binary silently reports
zero.

**Unresolved, and deliberately not guessed at here:** whether this is a genuine
defect in OpenCV's thread pool, a known-benign pattern in its refcounting, or an
artifact of an instrumented OpenCV built this particular way. Deciding that
needs a standalone reproducer calling `cv::parallel_for_` from two threads with
no slideio in the picture, which is its own piece of work. Until someone does
that, the honest statement is: reported, unexplained, and not known to affect
correctness — no test has ever failed because of it.

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
| 3 | `SCNSlide` passed a `TIFF*` where `SVSSmallScene` expects a `bool` | Fixed the way the entry required: the argument is dropped, not plumbed through, so the supplemental-image scene still opens its own per-thread handles and SCN's concurrent-read contract is untouched. `auxiliary` takes its default `true`, the value the pointer used to convert to, so behaviour is unchanged. `SVSSmallScene` also declares a deleted constructor taking `const volatile void*` in that position (`svssmallscene.hpp`), which out-ranks the pointer-to-`bool` conversion, so passing a handle there is now a compile error rather than a silent `true`. | Watched the deleted overload reject the old `scnslide.cpp` call before dropping the argument. `SceneLifetime.scn` and the SCN driver tests read the `Macro` supplemental image. |
| 11 | `TransformerScene` has no level table, so transformed scenes cannot be read by level | `TransformerScene` copies the origin's `m_levels` in its constructor and overrides `readResampledLevelBlockChannelsEx` to read the origin at the level it was asked for, inflating in level coordinates. The entry's open design question is answered in `transformerscene.hpp`: a transformation's parameters are in the pixels of the level being read, which is what `computeInflatedRectParams` already does for a scaled read. | `TransformerSceneLevels.*` in `slideio_transformer_tests` — four tests, all watched failing first. `aLevelReadAgreesWithTheEquivalentScaledRead` is the one that pins the semantics: a full read of level 1 is bit-identical to a half-scale read of the scene. |
| 14 | ZVI serialised every block read | `ZVIScene::supportsConcurrentReads()` returns `true` (`zviscene.hpp:62`), on one shared `ole::compound_document` made safe by pole's positional read path. | `software-docs/specs/2026-09-09-zvi-concurrent-reads-design.md` §3.2, §4, §5.3, §5.4. Its follow-ups are still open as [§19](#19-pole-read-path-defects-left-in-place), [§20](#20-the-zvi-concurrent-read-work-what-no-test-covers) and [§21](#21-pole-read-path-throughput-two-remaining-items). |
| 17 | OME-TIFF serialised every block read | `OTScene::supportsConcurrentReads()` returns `true` (`otscene.hpp:88`); `TIFFFiles` moved off `OTScene` into a per-thread `OTReadContext` held by a `ContextPool`. | `software-docs/specs/2026-09-08-ometiff-concurrent-reads-design.md`; the contract assertion is `OTImageDriverTests.reportsConcurrentReadSupport`. |
| 24 | GDAL and CZI scenes hold raw pointers into slide-owned state | The entry's second fix, applied to three drivers rather than two: `GDALScene` shares ownership of the `SmallImage` that owns its page (`fddde520`); `NDPIScene` shares the slide's `NDPIFile`, which the entry did not list and which threw "Invalid directory index" after the release; `CZIScene` copies resolution, magnification and the z and t resolutions during `init()` and clears `m_slide` there, and `CZIThumbnail` reads through the slide's shared `FileReader` instead of the slide, which owns it (`83e0d63e`). With every scene owning what it reads, the `scene.hpp` contract -- do not destroy a scene or its slide while a read is in flight -- is the real requirement again and was left as written. The entry's third fix, `slideio::Scene` retaining its `Slide` as `PyScene` does, was not taken, so a new driver can reintroduce the bug; the lifetime test is what would catch it. | `SceneLifetime.*` in `slideio_tests` opens one file per driver -- all twelve -- and compares what every scene and auxiliary image reports and reads before and after the slide is released. It failed for NDPI and crashed for CZI before the fix; GDAL's own case is `GDALDriver.sceneOutlivesItsSlide`, watched crashing first. |
| 25 | A transformed scene bypasses its origin's read lock | `CVScene::lockIfSerialised()` now takes the mutex named by the new virtual `readSerialisationMutex()`, and `TransformerScene` overrides it to return its origin's (`transformerscene.hpp`), so a wrap chain contends on one lock instead of one mutex per scene. | The two exposures are `TransformedSceneReadLock.twoTransformsOverOneOriginDoNotReadItConcurrently` and `.aDirectReadOfTheOriginExcludesATransformedRead` in `slideio_transformer_tests`; both were watched failing on the unfixed code. `.aConcurrentOriginIsStillReadConcurrentlyThroughATransform` guards `TransformerScene::supportsConcurrentReads()`'s forwarding to the origin against a fix that re-serialises what it made concurrent. |
| 27 | VSI scales X, Y and Z resolution by a hardcoded 1e-6 and ignores the unit the file states | `VSITools::unitToMeters` parses the length unit as `unitToSeconds` parses the time one, both now through one base-parameterised parser that also refuses a power other than 1 — `m^2` is an area. `Volume::setResolution(rawX, rawY, unit)` and `setZResolution(raw, unit)` convert on the way in and store nothing when the unit cannot be read, giving `Volume` the same invariant for length it already had for time. `vsifile.cpp` passes `RWC_FRAME_UNIT` for X/Y and the dimension's sibling `UNITS` for Z. | `VSITools.UnitToMeters`, `.AUnitRaisedToAnotherPowerIsNotThatUnit`, `Volume.ResolutionIsStoredInMetres`, `.ResolutionRequiresAParseableUnit`, `.ZResolutionIsStoredInMetres`, `.ZResolutionRequiresAParseableUnit`. Every corpus file states `10^-6m^1`, so the four existing resolution assertions in `slideio_vsi_tests` are unchanged — which is the check that the conversion agrees with the factor it replaced. |

Also removed: eight of the nine sub-items of [§1](#1-tiffkeeper-and-ndpitiffkeeper-are-two-classes-with-one-contract) — the
copy/move semantics, the `operator=` and `openTiffFile` leaks, the
constructor inconsistency, the implicit `TIFF*` conversion, the
`TIFFKeeperPtr` macro, the header hygiene and the missing `m_hFile`
initialiser. All were verified fixed in `tiffkeeper.hpp`/`.cpp` and
`ndpitiffkeeper.hpp`/`.cpp`; the design record is
`software-docs/specs/2026-08-15-tiffkeeper-ownership-design.md`. Only the
unification of the two classes remains open, and §1 is now about that alone.
