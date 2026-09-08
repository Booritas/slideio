# OME-TIFF: concurrent reads

**Date:** 2026-09-08
**Branch:** v2.10.0
**Status:** Design approved, pending implementation plan
**Baseline:** `slideio` @ `bbcfe922`
**Companion:** `software-docs/specs/2026-09-07-parallel-read-block-design.md`
(the design that built the machinery this one consumes, and whose §3.2 deferred
OME-TIFF)
**Corrects:** `TECH_DEBT.md` §17 (OME-TIFF) and §16 (the driver named GDAL)

---

## 1. Goal

Make two `read_block` calls on one OME-TIFF `Scene` run at the same time, so
`OTScene` can report `supportsConcurrentReads() == true`.

The machinery is already in the tree: `CVScene::supportsConcurrentReads()` and
`lockIfSerialised()`, `ContextPool` handing out `ReadContext` subclasses one
borrower at a time, and the byte-exactness harness. Six driver libraries
covering seven formats were converted against it, with AFI inheriting an
eighth. OME-TIFF is one of the four formats that were deferred, and this
document converts it.

It is a smaller job than the deferred-driver entry claims, for the reason §2
sets out — but it has one structural problem none of the seven converted
drivers had, which is what §3 is about and what the design is shaped around.

---

## 2. The blocker, and the correction to §17

`TECH_DEBT.md` §17 says:

> `TIFFFiles::getOrOpen` does a `find` followed by an insert into a plain
> `std::unordered_map`. A race there corrupts the container — undefined
> behaviour, not a bad tile.

and calls that "a **different failure class**" from ZVI, DCM and GDAL, which is
the stated reason OME-TIFF was singled out.

**That is wrong on its central fact.** `getOrOpen` has exactly one caller:
`TiffData::init` (`tiffdata.cpp:56`, as of `bbcfe922`), reached from
`OTScene::extractTiffData` (`otscene.cpp:231`) inside `OTScene::initialize()`
(`otscene.cpp:157`), called from the constructor (`otscene.cpp:40`). The map is
populated while the scene is being built and is never touched again by a read.
So there is no container race, and OME-TIFF is not a distinct failure class.

### 2.1 What the read path actually touches

`OTScene::readTile` → `TiffData::readTile` → `TiffData::readTileChannels` →
`TiffTools::readTile` / `readStripedDir`.

Along that path:

- `TiffData::readTile` and `readTileChannels` are both `const`. They work on
  locals (`myChannelIndices`, `localChannelIndices`, `coords`,
  `globalChannelToChannelOrder`, `localRaster`, `channelRaster`) and on members
  written once during `init` and read-only thereafter (`m_directories`,
  `m_dimensions`, `m_coordinatesFirst`, `m_planeCount`, `m_filePath`).
- `OTScene::m_tiffData` is a `std::vector<TiffData>` built once in
  `extractTiffData` and only read afterwards.
- The `rasters` vector `TiffData::readTile` fills is `channelRasters`, a local
  of `OTScene::readTile` — per call, not shared.
- `BlockInfo`, the userData, is stack-allocated per read in each `…Ex` entry
  point.

**The only shared mutable state left is `TiffData::m_tiff`** — a raw
`libtiff::TIFF*` cached during `init` and shared by every `TiffData` that names
the same file. libtiff is not re-entrant on one handle. That is the ordinary
blocker, identical to the one SVS, PHTIFF, PKE, SCN and NDPI each had, and
`ContextPool` exists to solve it.

### 2.2 Why §17's first suggested fix does not work

§17 offers two routes: "either give `TIFFFiles` its own lock, or move the whole
map into a per-thread `ReadContext`."

The lock route is not merely weaker — it does not address the blocker at all.
A mutex around `getOrOpen` would make the *map* safe, but the map is not what
two threads contend over: they contend over the `TIFF*` handles the map hands
out, and a lock around the lookup leaves both threads using the same handle
afterwards. Only the per-context route is real, and this design takes it.

---

## 3. What makes OME-TIFF different

`OTScene::readTile` loops over `blockInfo->tiffDataIndices` — the set of
`TiffData` that cover the requested channels, z-slice and time frame — and
calls `TiffData::readTile` on each. Different `TiffData` may name **different
files**: the file is taken from the `<TiffData>` element's `UUID/FileName`
attribute, defaulting to the OME-TIFF itself.

So **one tile read can need handles to several files at once.** This is not
hypothetical; multi-file OME-TIFF datasets are in the corpus and tested today
(`ometiff/Multifile/multifile-Z1.ome.tiff`, `Multifile2/`, `4D-Series/`,
`tubhiswt-4D/`).

That is the structural difference. For every converted TIFF-family driver — SVS,
PHTIFF, PKE, SCN, NDPI and `VsiFileScene` — the unit of per-thread state was
*one handle*, so the context held one `TIFFKeeper`. (CZI needed no context at
all, being cursor-free through `FileReader`; VSI/ETS's context holds a scratch
buffer rather than a handle.) Here the unit is *a handle per file this thread
touches*, so the context has to hold a collection.

Two alternatives were considered and rejected:

- **A pool per referenced file** (`map<path, ContextPool>`) is
  descriptor-minimal, but a read touching N files would hold N borrows at once,
  and with bounded pools two threads acquiring in different orders can
  deadlock. That would be the first driver needing a lock-ordering discipline,
  for a resource saving §4.5 shows the existing laziness already delivers.
- **A per-driver pool bound** derived from the file count. Deferred, not
  rejected outright: it is available if §4.5's arithmetic ever bites, but
  inventing the first non-default bound for a cost that laziness contains is
  premature.

---

## 4. Design

### 4.1 `OTReadContext` and the pool

```cpp
// otscene.hpp, next to OTScene
namespace slideio { namespace ometiff {

    /// Per-thread libtiff handles for one OME-TIFF read.
    ///
    /// A collection rather than a single handle, because one tile read can span
    /// several TiffData that name different files (see the spec's section 3).
    /// TIFFFiles opens lazily, so a context only ever holds handles to the
    /// files the thread it served actually read.
    class OTReadContext : public ReadContext
    {
    public:
        TIFFFiles files;
    };

}}
```

`TIFFFiles` needs no modification. It is already the right abstraction — a
lazily-populated map from path to owned handle — and the only change is that it
becomes per-context instead of per-scene. `OTScene::m_files` is removed.

The context type is declared in the **scene header** next to the pool that
serves it, not in `otstructs.hpp`. That is deliberate: the SCN conversion first
put its userData wrapper in the driver's plain-data header behind a forward
declaration and had to be corrected, so the rule is that concurrency-era types
live in the scene header.

`OTScene` gains:

```cpp
    protected:
        ContextPool::Borrow acquireContext() { return m_contextPool.acquire(); }
    private:
        ContextPool m_contextPool;   // declared last -- see section 4.4
```

constructed with a factory that captures nothing — `TIFFFiles` needs no path,
since it opens per request — and with the second `ContextPool` argument omitted
so the bound is `ContextPool::defaultMax()`. **Not `kUnbounded`:** a libtiff
handle is a descriptor, and `kUnbounded` is reserved for contexts holding
nothing scarce.

One divergence from the sibling drivers to note, because a reviewer comparing
against `SVSReadContext` will look for it: their context constructors open the
file and throw on failure, which is what removes null-handle guards from the
call sites downstream. `OTReadContext`'s constructor opens nothing. The same
property still holds, but it comes from `TIFFFiles::getOrOpen`, which raises on
a failed `TIFFOpen` rather than returning null — so no call site needs a null
check either way. The consequence is that a context is cheap to construct here
and does its first I/O on first use.

`supportsConcurrentReads()` returns `true`.

### 4.2 How `TiffData` gets its handle

`TiffData::m_tiff` is removed. The read methods take the context's file
collection and resolve the handle themselves:

```cpp
    void readTile(const std::vector<int>& channelIndices, int zSlice, int tFrame,
                  int zoomLevel, int tileIndex, TIFFFiles& files,
                  std::vector<cv::Mat>& rasters) const;

    void readTileChannels(const TiffDirectory& dir, int tileIndex,
                          const std::vector<int>& channelIndices,
                          libtiff::TIFF* tiff, cv::OutputArray raster) const;
```

`readTile` does `libtiff::TIFF* tiff = files.getOrOpen(m_filePath);` once and
passes it to `readTileChannels`.

`OTScene::readTile` acquires nothing — it reads the context out of the userData
and hands the collection down:

```cpp
    auto* blockInfo = static_cast<const BlockInfo*>(userData);
    TIFFFiles& files = blockInfo->context->files;
    for (int index : blockInfo->tiffDataIndices) {
        m_tiffData[index].readTile(channelIndices, zSlice, tFrame, zoomLevel,
                                   tileIndex, files, channelRasters);
    }
```

**This is a deliberate deviation from the pattern PKE established**, where the
caller resolved the handle and passed `libtiff::TIFF*` down as a parameter. It
is worth stating why, because consistency with the template is otherwise the
default.

OME-TIFF is the only driver where one read touches several files. Under the PKE
shape, `OTScene::readTile` would have to pair the right handle with the right
`TiffData` on every iteration of the loop — while the lookup key, `m_filePath`,
is a `TiffData` member. That splits a correspondence across two classes for no
benefit, and mispairing it would read the wrong file's tile without any type
error. Keeping the resolution inside the class that owns the path removes the
opportunity. The added coupling is to `TIFFFiles`, which `TiffData::init`
already takes today.

`readTileChannels` still receives a bare `TIFF*`, because at that depth the
file is settled and passing the collection would invite a second lookup.

### 4.3 `init()`, and why fail-fast does not move

`TiffData::init` needs a handle regardless of any of this: it calls
`TiffTools::scanTiffDir` once per plane to build `m_directories`. So
`OTScene::initialize()` takes a **local borrow** and passes that context's
`TIFFFiles&` into `init`, exactly as `SCNScene::init` does for its own
directory scanning:

```cpp
    auto borrow = m_contextPool.acquire();
    TIFFFiles& files = borrow.as<OTReadContext>().files;
    // ... extractTiffData(pixels) passes `files` into each TiffData::init
```

`TiffData::init`'s signature changes from `TIFFFiles* files` to
`TIFFFiles& files` — it already raises on null, so the reference removes a
check rather than adding a risk.

Two consequences worth having deliberately:

- **Every referenced file is still opened at construction, so nothing about
  failure timing or behaviour moves.** Be precise about what that behaviour
  actually is, because it is easy to assume: `extractTiffData` wraps each
  `TiffData::init` in a per-element `catch (std::exception&)` that logs a
  warning and skips that element. So an unopenable or non-TIFF member file does
  **not** fail `openSlide` today — the scene is built without that element.
  This design neither improves nor worsens that; `init` still opens every file
  at construction, and a failure is still a warning and a skipped element. The
  point of using a local borrow in `init` is precisely that this stays
  unchanged: the `VsiFileScene` conversion moved its failure timing from
  construction to first read and had to record it in `BREAKING_CHANGES.md`,
  and here nothing moves.

  That per-element tolerance is a soft failure on malformed input, the same
  shape as the CZI attachment-path asymmetry recorded in `TECH_DEBT.md` during
  the parallel-read-block work. It is pre-existing, orthogonal to concurrency,
  and out of scope here — noted only so a reader of this section is not misled
  about what "validated at construction" buys.
- The borrow is released at the end of `initialize()`, so the context returns
  to the pool with its handles already warm — the first read of that scene on
  that context pays no reopen.

### 4.4 Member declaration order

`m_contextPool` must be declared **after** `m_tiffData` in `OTScene`.

This is not style. `~ContextPool` blocks until every outstanding `Borrow` is
returned, but it is a member, and members are destroyed in reverse declaration
order. If the pool were declared first it would be destroyed last — after
`m_tiffData`, whose `TiffData` objects an in-flight read is dereferencing. The
whole-branch review of the parallel-read-block work found exactly this defect
in several of the eight converted formats, and it was fixed by moving pools to
be declared last. A comment at the member records that the order is load-bearing,
because it will otherwise be tidied.

Note also that the pool's blocking guarantee covers the **contexts**, not the
scene. As `parallel-read-block`'s §4.7 records after the same review, a
`Scene` or `Slide` must not be destroyed while a read on it is in flight; that
requirement is documented on `Scene` and applies here unchanged.

### 4.5 Descriptor arithmetic

This is the one place the design costs something new, so it is stated rather
than assumed.

| | Descriptors |
|---|---|
| Today | scenes × distinct files per scene |
| After | scenes × pool size × files opened per context |

What contains it is that `TIFFFiles::getOrOpen` already opens **lazily**. A
context opens only the files the thread it served actually read, and a single
tile read touches only the `TiffData` named in `blockInfo->tiffDataIndices`. So
the realistic peak is `pool × files per read`, not `pool × files per scene`.

Concretely, against the corpus: the tested `retina_large.ims Resolution Level 1`
scene has 128 `TiffData` items and **one** distinct file
(`test_ometiff_driver.cpp:299-301`), so its ceiling is
`defaultMax()` = `min(8, hardware_concurrency())` handles. A `Multifile/`
dataset is that multiplied by the handful of files a read spans.

The design ships the shared default bound and records this arithmetic. If it
ever bites on a large multi-file dataset, §3's deferred alternative — a bound
derived from the file count — is the change to make, and it is local to the
pool's construction.

---

## 5. Verification

**Invert the existing contract test.**
`OTImageDriverTests.concurrentReadsAreStillSerialised`
(`test_ometiff_driver.cpp:963`, asserting `EXPECT_FALSE` at `:971`) was added
to stop the deferred exemptions drifting silently. Converting OME-TIFF means
replacing it with a `reportsConcurrentReadSupport` assertion and removing
OME-TIFF from the deferred-driver coverage — leaving ZVI, DCM and GDAL asserted
`false`.

**Byte-exactness, including the multi-file case.**
`TestTools::concurrentReadIdentityTest` computes a single-threaded baseline per
ROI, then has 16 threads read every ROI repeatedly and compares **every** read
byte-for-byte. Run it on:

- a single-file scene (`Subresolutions/Leica-2.ome.tiff` or `retina_large`), and
- **`Multifile/multifile-Z1.ome.tiff`** — not optional. Multi-file is the case
  this design is shaped around, and a single-file scene exercises exactly one
  `getOrOpen` per context, which would leave the collection's whole reason for
  existing untested.

Use the all-paths variants added by the parallel-read-block fix wave, so
channel subsets and the level-addressed entry point are covered as well as the
plain 2D path. OME-TIFF's per-channel logic walks `TiffData` coordinate ranges
and filters by `isInRange`, which is precisely the per-read-state-heavy code
the narrower harness was found to be missing.

`concurrentReadIdentityTestAllScenes` is the right variant where a slide has
several scenes, since OME-TIFF slides routinely do.

**Redefine `getNumTiffFiles()`.** It currently returns
`m_files.getNumberOfOpenFiles()` — the scene's map size. With the map
per-context that number becomes per-context and would read 0 before the first
read, breaking `test_ometiff_driver.cpp:299` for a reason unrelated to what the
test is checking. Redefine it as the count of **distinct files referenced**,
computed from `m_tiffData`'s paths: deterministic, independent of read history,
still 1 for that scene, and closer to what the assertion means.
`OTImageDriverTests.TIFFFiles`, which unit-tests the class directly, is
unaffected.

**ThreadSanitizer.** MSVC has no TSan and there is no Linux build on the
primary development machine, so as with every driver in the previous branch the
byte-exactness gate carries the weight locally. The `tsan-linux` CI job covers
`FileReader` and `ContextPool` only; a Linux TSan run of `slideio_ometiff_tests`
is the natural gate before this ships and should be treated as required rather
than eventual, since the eight-format claim in `BREAKING_CHANGES.md` becomes a
nine-format claim.

**Suites to run:** `slideio_ometiff_tests` and `slideio_tests` (the latter
carries the deferred-driver contract coverage that this change edits).

---

## 6. Documentation updates

### 6.1 `TECH_DEBT.md` §17 — rewritten

The entry's claim that `getOrOpen` races on the read path is wrong (§2), its
"different failure class" framing is wrong, and its first suggested fix does
not work (§2.2). On conversion the entry is closed with a pointer to this spec;
if this spec is not implemented, it still needs correcting to describe the real
blocker — `TiffData::m_tiff`, shared across every `TiffData` naming one file.

### 6.2 `TECH_DEBT.md` §16 — rewritten, independently of this work

§16 tells whoever picks up the driver named GDAL to "add a `ReadContext`
subclass holding the GDAL dataset. GDAL datasets are not re-entrant."

**That driver contains no GDAL.** `GDALScene::m_imagePage` is a
`SmallImagePage*` (`gdalscene.hpp:42`), obtained from
`GDALSlide`'s `m_image->readPage(...)` (`gdalslide.cpp:22`) where `m_image` is a
`SmallImage`. The only implementation of `SmallImagePage` in the tree is
`FIWrapper::Page` (`fiwrapper.hpp:25`), and `fiwrapper.hpp` includes
`<FreeImage.h>`. A tree-wide search for `GDALOpen`, `gdal_priv` and
`GDALAllRegister` returns nothing.

So the entry sends a reader to the wrong library. The real question for that
driver is FreeImage's thread-safety — its plugin registry and
`FreeImage_Initialise` are process-global — plus whatever state `FIWrapper` and
`FIWrapper::Page` hold. The driver id and the format name stay as they are;
only the entry's mechanism claim changes.

This correction is worth landing whether or not OME-TIFF is converted, and does
not depend on any of §4.

### 6.3 `TECH_DEBT.md` §15 — one detail added

§15's claim about DCM is accurate and the mechanism is sharper than written:
`DCMFile::createImage` constructs `DicomImage(dataset, xfer,
CIF_UsePartialAccessToPixelData, firstFrame, numFrames)`, and
`CIF_UsePartialAccessToPixelData` is precisely the flag that makes DCMTK retain
partial pixel-data state inside the shared `DcmDataset` between
`DicomImage` constructions. Worth naming in the entry, because it is the reason
per-thread `DCMFile` replicas are the only route there.

§14 (ZVI) was verified accurate and needs no change: `ZVIScene::m_Doc` is an
`ole::compound_document` member (`zviscene.hpp:72`) passed by reference into
`ZVITile::readTile` on the read path (`zviscene.cpp:137`).

### 6.4 `BREAKING_CHANGES.md`

Under `v2.10.0` (or the release this lands in): OME-TIFF scenes begin reporting
concurrent reads, taking the count from eight formats to nine. No signature
changes on the public API. Exported-driver API does change and must be
recorded: `TiffData::init` takes `TIFFFiles&` rather than `TIFFFiles*`;
`TiffData::readTile` and `readTileChannels` gain parameters;
`OTScene::getNumTiffFiles()` changes meaning; and `OTScene` loses `m_files`,
which is a layout change for anything holding one by value.

### 6.5 `CLAUDE.md`

The concurrency-contract bullet lists the concurrent formats; add OME-TIFF.

---

## 7. Out of scope

- **Converting ZVI, DCM or the FreeImage-backed driver.** Only their
  `TECH_DEBT.md` entries are touched, and only to make them accurate.
- **OME-TIFF read granularity.** `readTileChannels` reads a whole striped
  directory when `dir.tiled` is false, and rejects any `tileIndex != 0` there.
  That shapes how much work one "tile" is, and it interacts with throughput,
  but it is orthogonal to whether two reads can overlap.
- **The per-tile allocations** in `OTScene::readTile` (`channelRasters`) and
  `TiffData::readTile` (`localRaster`, per-channel `cv::extractChannel`
  copies). Real, and the kind of thing the read-batch analysis measured
  elsewhere, but a single-threaded cost that this change neither helps nor
  worsens.
- **A public tuning knob** for the pool bound. The parallel-read-block design
  declined one and nothing here earns it.
- **`read_batch` itself.** This removes the last blocker in this driver for a
  batched reader built on top; it does not build one.
