# Parallel `read_block`: concurrent reads on one `Scene`

**Date:** 2026-09-07
**Branch:** v2.10.0
**Status:** Design approved, pending implementation plan
**Baseline:** `slideio` @ `cca7fa74` (branch `v2.10.0`)
**Companion:** `software-docs/specs/2026-09-06-read-batch-performance-design.md`
(the source analysis; this document specifies items 2, 5 and 6 of its §5
sequencing table)

---

## 1. Goal

Make two `read_block` calls on one `Scene` run at the same time.

Today they cannot. `CVScene::readResampledBlockChannels` and
`readResampledLevelBlockChannels` each take `m_readBlockMutex` around the whole
read, so reads of one scene are thread-**safe** and completely **serialised**.
The Python bindings already release the GIL on all four read paths, which means
a worker thread drops the GIL and then immediately blocks on the C++ mutex:
threading over one `Scene` yields exactly zero parallelism, silently. That is
the worst shape for the failure to take, because threading a `Scene` is the
first thing a user tries.

After this change, a scene whose driver has been converted reports
`supportsConcurrentReads() == true` and takes no lock; the drivers that have
not been converted keep the mutex and behave exactly as they do now.

The beneficiary is not only the ML tiling work that prompted the analysis.
`TiffConverter` already pays for a full `cloneScene()` — `openSlide` plus
`getScene` per reader thread — for precisely this reason, and can stop.

### 1.1 What this is not

It is not a re-entrancy audit of eleven drivers. The decode path is already
thread-clean, and that is the finding that makes the work tractable:

- Scene metadata (`m_levels`, `m_directories`, `m_zoomLevels`,
  `m_componentToChannelIndex`, `m_sceneParams`) is written in `initialize()`
  and read-only thereafter.
- `CZIScene::readTile` works entirely on locals — the encoded buffer, the
  decoded vector, the channel rasters.
- The codecs hold no mutable globals: `jp2kcodec.cpp`, `jpegcodec.cpp`,
  `jxrcodec.cpp` and `jp2kmem.cpp` contain only file-scope *functions* plus
  per-call OpenJPEG/JXR objects.
- `getMetadata()` / `getChannelAttributes()` are already `std::call_once`.
- `RefCounter` is an `std::atomic<int>`.

What the lock actually protects is two things: stateful file I/O, and the
process-global libtiff message-handler swap. There are therefore two
mechanisms to replace, not eleven drivers to audit.

---

## 2. Relationship to the `read_batch` performance analysis

The companion analysis measured four costs on the tile-read path and ranked
them. The read lock is #1 by ceiling — it caps throughput at one core — but
items #2 to #4 are local, single-threaded costs that together amount to more
than the JPEG decoding they exist to support:

| # | Cost | Where | Measured |
|---|---|---|---|
| 1 | reads on one `Scene` are serialised | `CVScene::m_readBlockMutex` | caps at 1 core |
| 2 | RGBA repack on the YCbCr-JPEG path | `TiffTools::readNotRGBTile` | 0.214 ms/tile vs 0.031 fused |
| 3 | `composeRect` walks every tile of the level | `TileComposer::composeRect` | 0.235 ms/read at level 0 of 100k×80k |
| 4 | output grid misaligned with codec tiles | caller | 2.25× surplus decodes |

**Sequencing precondition, stated here because it constrains how this work is
judged rather than how it is built:** items 2 and 3 (the fused repack and
`Tiler::getTileIndices`) should land *before* anyone benchmarks this
specification's output. Otherwise the concurrency measurement multiplies a read
path that spends more than half its time on avoidable work, and overstates the
value of the lock removal. This document does not specify those items; it only
records that a benchmark taken before them is not evidence.

The analysis also proposed a pool of open `Slide`s as an interim workaround.
That is explicitly **not** what this document specifies. A `Slide` pool costs
N× metadata parse and N× the parsed model in memory, and only the caller that
builds it benefits. Removing the serialisation benefits every caller.

---

## 3. Scope decisions

### 3.1 Drivers made concurrent

Six driver libraries, covering seven formats:

- SVS and PHTIFF (both in `slideio-svs`)
- PKE
- SCN
- NDPI
- CZI
- VSI — both halves: the ETS scenes and `VsiFileScene`

AFI needs no work: it aggregates SVS slides and its scenes *are* SVS scenes, so
it inherits the conversion. That makes eight of the twelve formats concurrent,
and it is why the tree has 11 driver libraries for 12 formats.

**The contract is per scene, not per slide, and CZI shows why that matters.**
`SVSSmallScene` and `PKESmallScene` derive from their driver's scene base and so
inherit `true` for their auxiliary images, but CZI's auxiliary images are
`CZIThumbnail`, which derives from `CVSmallScene` and reports the default
`false`. So a CZI slide's main scenes are concurrent while its thumbnail is
not. That is safe — `false` only means the base class serialises that scene —
but it means the "every scene of one slide gives the same answer" property that
`VSIImageDriverTests.allScenesAgreeOnTheConcurrencyContract` asserts for VSI
does **not** hold library-wide. A caller deciding whether to build a thread
pool must ask each `Scene`, not one scene of the slide.

### 3.2 Drivers declared serialised

Four scene types stay serialised in this change and are recorded in
`TECH_DEBT.md` (§7.2):

| Driver | Why deferred |
|---|---|
| ZVI | mutable OLE cursors three layers down in vendored code |
| DCM | DCMTK `DcmDataset` state, including its decompressed-representation cache |
| GDAL | GDAL datasets are not re-entrant, and the driver decodes a whole scene per call |
| OME-TIFF | `TIFFFiles::getOrOpen` mutates an `unordered_map` of handles on the read path |

Each is deferred because its mutable state lives inside a third-party or
vendored library rather than in slideio, and each would drag that library's
threading model into scope. None is in the tiling or converter path.

The OME-TIFF case deserves separating from the others because it is a
different *failure class*. The rest are cursors, and a race on a cursor yields
wrong pixels. `TIFFFiles::getOrOpen` does a `find` followed by an insert into a
non-thread-safe `unordered_map`, so a race there corrupts the container —
undefined behaviour, not a bad tile. It must therefore stay behind the mutex,
not be left to chance.

### 3.3 No new public API

The pool is bounded internally at `min(8, std::thread::hardware_concurrency())`
with no knob, no environment variable and no `setMaxReaders`. Nothing in a
converted pool holds an expensive resource — the TIFF contexts hold one
descriptor each and the stream contexts hold only scratch memory — so the
tuning knob the analysis contemplated is not yet earned. Adding it later is
source-compatible; removing it would not be.

`CVScene::supportsConcurrentReads()` is public and virtual, because the
guarantee genuinely differs per scene during and after phased delivery, and a
caller deciding whether to build a thread pool needs to be able to ask.

---

## 4. Design

### 4.1 The concurrency contract

`CVScene` gains one virtual:

```cpp
// core/cvscene.hpp
/// True if two block reads of this scene may run concurrently on different
/// threads. False (the default) means the base class serialises them.
virtual bool supportsConcurrentReads() const { return false; }
```

and one protected helper that replaces the three `std::lock_guard`
constructions:

```cpp
protected:
    /// Engaged only when the scene does not support concurrent reads.
    std::unique_lock<std::mutex> lockIfSerialised() const {
        return supportsConcurrentReads()
                   ? std::unique_lock<std::mutex>()
                   : std::unique_lock<std::mutex>(m_readBlockMutex);
    }
```

The default is deny. A driver that has not been converted, and any driver added
later, is serialised by construction — the safe direction for a default whose
wrong value produces silent corruption.

Three call sites in `cvscene.cpp` change — the three `std::lock_guard`
constructions that `grep m_readBlockMutex` finds alongside the member
declaration:

1. `readResampledBlockChannels` — replace the guard with `lockIfSerialised()`.
2. `readResampledLevelBlockChannels` — same.
3. `assemble4DBlock` — see below.

**The 4D asymmetry (TECH_DEBT #4, second half).** `assemble4DBlock` takes the
mutex around `readPlane` in the `planeMatrix` branch and not in the other one.
The asymmetry was carried over deliberately when the plane loop was extracted
for 2.9.0, to keep that extraction behaviour-preserving. It is fixed here:
both branches take `lockIfSerialised()` for the duration of their `readPlane`
call.

There is no nesting hazard in doing so. `readPlane` is a `std::function`
supplied by `readResampled4DBlockChannels` and
`readResampledLevel4DBlockChannels`, and both lambdas call the **`Ex`**
variants — `readResampledBlockChannelsEx`, `readResampledLevelBlockChannelsEx`
— which do not lock. The lock is therefore taken at most once per plane on a
non-recursive mutex. Any future change that makes a plane callback re-enter a
locking entry point deadlocks; the helper's comment says so.

Granularity is deliberately per-plane rather than hoisted out of the loop: it
matches the 2D path and keeps a 4D read from holding the lock across an
arbitrary number of planes.

### 4.2 `ReadContext` and `ContextPool`

One mechanism serves every converted driver. `ReadContext` is the abstract
carrier for whatever per-thread mutable state a driver's read path needs;
`ContextPool` owns a bounded set of them and hands them out under RAII.

```cpp
// core/tools/readcontext.hpp
namespace slideio
{
    /// Per-thread mutable state for one block read. Drivers derive and add
    /// whatever their read path cannot share between threads: a file handle,
    /// a scratch buffer, a parsed container object.
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

```cpp
// core/tools/contextpool.hpp
namespace slideio
{
    class SLIDEIO_CORE_EXPORTS ContextPool
    {
    public:
        /// Pass as maxContexts when the context holds no scarce resource.
        static constexpr int kUnbounded = 0;
        static int defaultMax();      // min(8, hardware_concurrency()), >= 1

        using Factory = std::function<std::unique_ptr<ReadContext>()>;

        explicit ContextPool(Factory factory, int maxContexts = defaultMax());
        ~ContextPool();               // blocks until every Borrow is returned

        ContextPool(const ContextPool&) = delete;
        ContextPool& operator=(const ContextPool&) = delete;

        class Borrow                  // move-only
        {
        public:
            Borrow(Borrow&&) noexcept;
            Borrow& operator=(Borrow&&) noexcept;
            ~Borrow();                // returns the context to the free list

            ReadContext& get() const;
            template <class T> T& as() const;   // static_cast; dynamic check in debug
        };

        /// Reuse a free context; else construct one via the factory; else
        /// block until another thread returns one.
        Borrow acquire();
    };
}
```

Both live in `slideio-core`, which is where `CVScene` lives and below every
driver.

**Free-list, not thread-affine.** `acquire()` hands out any free context rather
than keying off a `thread_local`. This is the load-bearing choice in the class:

- it is genuinely bounded — a `thread_local` cannot be capped;
- contexts are released deterministically when the borrow dies, not when the
  thread does, so a scene's descriptors are not pinned by an idle worker;
- it survives a churning thread pool, where thread-affine storage leaks one
  context per thread that has ever read and then exited;
- it needs no `thread_local` in the tree at all, which matters because
  thread-local file handles couple a descriptor's lifetime to a thread rather
  than to the `Scene` that owns it — on Windows that shows up as a file the
  user cannot delete after closing the slide.

It requires that contexts be interchangeable between threads. Every context
specified here is: a TIFF handle repositioned per read, or a scratch buffer.

**Construction is lazy.** A pool for a scene nobody reads concurrently opens
exactly one handle, so single-threaded callers pay nothing. The factory runs
outside the pool's own lock, because `TIFFOpen` does I/O.

**Blocking, not growth, past the cap.** A ninth simultaneous reader waits on a
condition variable rather than opening a ninth descriptor. Waiting is bounded
by one tile read of an already-warm pool, which is the right trade against
unbounded descriptor use on a machine with many cores.

`kUnbounded` exists because capping a pool whose context holds nothing scarce
would serialise a path that has no contention at all. CZI and VSI/ETS pass it.

**Where the pool lives: with the resource, not with the scene.** For
scene-owned handles the pool is a scene member. For NDPI the handle belongs to
the shared `NDPIFile`, so the pool does too — putting it on the scene would
give each scene of one file its own handle set and multiply descriptors by
scene count.

**How a borrow reaches `readTile`.** It rides in the driver's existing
userData. `Tiler`'s three methods take an opaque `void* userData`, and every
driver already declares its own per-call struct and stack-allocates it in
`readResampledBlockChannelsEx` (`CZIScene::TilerData`, `WSIScene::TilerData`,
`vsi::TileComposerUserData`, `NDPIUserData`). Each converted driver adds a
`ReadContext*` field to its struct. **The `Tiler` interface does not change.**

The shape per read is therefore:

```cpp
void XScene::readResampledBlockChannelsEx(...)
{
    auto borrow = m_contextPool.acquire();          // once per read
    XUserData userData;
    userData.context = &borrow.as<XReadContext>();
    ...
    TileComposer::composeRect(this, channelIndices, blockRect, blockSize,
                              output, &userData);
}
```

One acquisition per `read_block`, not per tile. This also fixes
`PKETiledScene::readTile` structurally: it calls `getFileHandle()` up to six
times in one operation today, which under a naive per-call borrow could hand a
thread different handles inside one tile. Six reads of one borrowed context
cannot.

### 4.3 `FileReader`: positional I/O

For CZI and VSI the file state is a cursor slideio itself owns, so it can be
removed rather than replicated. One descriptor then serves every thread — no
pool, no per-thread handle, no descriptor growth.

```cpp
// core/tools/filereader.hpp
class SLIDEIO_CORE_EXPORTS FileReader
{
public:
    explicit FileReader(const std::string& path);
    ~FileReader();
    /// Thread-safe. No shared cursor. Fills size bytes or throws.
    void readAt(uint64_t offset, void* dst, size_t size) const;
    uint64_t size() const;
private:
#ifdef _WIN32
    void* m_handle;    // CreateFileW(..., FILE_FLAG_OVERLAPPED
                       //                | FILE_FLAG_RANDOM_ACCESS, ...)
#else
    int m_fd;          // open(path, O_RDONLY | O_CLOEXEC);
                       // posix_fadvise(POSIX_FADV_RANDOM)
#endif
    uint64_t m_size;
};
```

Two platform details that are easy to get subtly wrong, and both fail quietly:

- **Windows.** `ReadFile` with an `OVERLAPPED` offset on a handle opened
  *without* `FILE_FLAG_OVERLAPPED` does read from the offset, but it also moves
  the shared file pointer, and concurrent operations on such a handle are not
  supported. That build would be racy on the primary development platform while
  appearing to work. Open with `FILE_FLAG_OVERLAPPED`; put the `OVERLAPPED` on
  the stack per call with a `thread_local` manual-reset event; finish with
  `GetOverlappedResult(..., TRUE)`. The file pointer is then never used.
- **POSIX.** `pread` is specified not to touch the file offset, so one fd is
  safe across threads — but it may return short. Loop until satisfied.
  `ifstream::read` hid this, so it is a guarantee actively lost in the
  migration, and it gets its own test (§6).

`FileReader` is `const`-correct and holds no mutable state, so a
`shared_ptr<const FileReader>` can be handed to every scene of a slide.

**Memory mapping is rejected.** It would make all of this disappear — reads
become `memcpy`, the page cache does the caching, access is inherently
positional. But a truncated or network-backed file raises `SIGBUS` on POSIX and
an SEH exception on Windows in the middle of a `memcpy`, and handling that
inside a library that must survive arbitrary user files is not worth the
saving.

**Parsing keeps a cursor.** `init()` paths are single-threaded and should not
be contorted into positional reads. A thin, non-owning
`SequentialReader { const FileReader& reader; uint64_t pos; }` with
`read<T>()` / `readBytes()` / `skip()` serves them.

### 4.4 Prerequisite: the process-global libtiff message handlers

`TIFFMessageHandler`'s constructor swaps libtiff's error and warning handlers
and its destructor restores them; `NDPITIFFMessageHandler` is its twin. Those
handlers are **process-global**. `tiffkeeper.hpp` already documents the
non-LIFO hazard and correctly calls today's consequence "lost log routing, not
a crash". Under concurrent reads it stops being that: two threads write the
same global outside any lock.

Separately, and independent of concurrency, NDPI constructs one **per tile** —
in both `NDPIScene::getTileRect` and `NDPIScene::readTile`. Since
`getTileRect` runs once per tile of the whole level under today's
`composeRect` scan, an NDPI read at level 0 of a large slide performs on the
order of 10⁵ global handler swaps.

**Fix:** install both handlers exactly once during library initialisation,
next to logging setup in `ImageDriverManager`, and never swap them again.
Remove the construction from `TIFFKeeper`, `NDPITIFFKeeper`, `TIFFFiles` and
`TiffConverter`, and delete the per-tile constructions in `NDPIScene`. Keep the
classes themselves only if something still needs to scope a handler; otherwise
delete them and keep the handler functions.

This is a precondition for everything in §4.5, and it removes a real
per-tile cost today, so it lands first and separately (§5).

### 4.5 Per-driver conversion

| Driver | Handle today | Context holds | Pool cap | Pool owner |
|---|---|---|---|---|
| SVS, PHTIFF | scene-owned `TIFFKeeper` | one `TIFFKeeper` | `defaultMax()` | scene |
| PKE | scene-owned `TIFFKeeper` | one `TIFFKeeper` | `defaultMax()` | scene |
| SCN | scene-owned `TIFFKeeper` | one `TIFFKeeper` | `defaultMax()` | scene |
| NDPI | shared `NDPIFile::m_tiff` | one `NDPITIFFKeeper` | `defaultMax()` | `NDPIFile` |
| CZI | shared `std::ifstream` | *no context; no pool* | — | — |
| VSI/ETS | shared `VSIStream` + member scratch | tile scratch | `kUnbounded` | `EtsFile` |
| VSI/TIFF | `VsiFileScene::m_tiff` | one `TIFFKeeper` | `defaultMax()` | scene |

#### 4.5.1 The TIFF family (SVS, PHTIFF, PKE, SCN, VSI/TIFF)

libtiff is genuinely not re-entrant on one handle, so each concurrent reader
needs its own. The conversion is cheap for one specific reason:
`TiffTools::setCurrentDirectory` positions with
`TIFFSetSubDirectory(tiff, dirOffset)` when an offset is known. A fresh handle
therefore jumps straight to the right IFD with **no directory walk and no
re-parse** — the cost of a context is one `TIFFOpen` (first IFD only) plus a
seek. The parsed pyramid model stays shared and read-only. This is what makes
a handle pool categorically cheaper than the `Slide` pool the analysis
proposed as an interim.

Every read in these drivers funnels through one accessor —
`SVSScene::getFileHandle`, `PKEScene::getFileHandle`, `SCNScene::getFileHandle`
— so there is one site per driver to change. The accessor is replaced by
reading the handle out of the borrowed context, and the borrow is acquired in
`readResampledBlockChannelsEx` and passed down through userData (§4.2).

`SVSScene::makeSureFileIsOpened()` lazily `reset`s the keeper, which is itself
a race under concurrent reads — two threads can both find the keeper invalid
and both open. Pool acquisition subsumes it: the factory opens, and the pool's
own lock makes that safe. The method goes away.

**SCN's auxiliary scenes.** `SCNSlide` constructs `SVSSmallScene` passing
`m_tiff.getHandle()` into a parameter declared `bool auxiliary` — the
documented TECH_DEBT #3 bug. The handle is silently converted to `true` and
discarded, and the small scene lazily opens its own file, so SCN aux scenes are
*accidentally* safe for concurrency. The spec's requirement is negative but
important: **fixing TECH_DEBT #3 must not repair the call by actually passing
the slide's handle into the scene.** Doing so would introduce a shared handle
into a driver this change has just declared concurrent. If #3 is fixed, the
handle argument must be dropped, not plumbed.

#### 4.5.2 NDPI

Same mechanism, different owner. `NDPIScene::m_pfile` is a raw pointer to a
shared `NDPIFile`, and `readTile` reaches the handle via
`NDPIFile::getTiffHandle()`. The pool therefore belongs to `NDPIFile`, and
`getTiffHandle()` is replaced by `NDPIFile::acquireContext()` returning a
`ContextPool::Borrow`. Scenes of one NDPI file share the pool, so descriptor
use is bounded per *file*, not per scene.

NDPI also carries the largest share of §4.4's benefit: with the message
handlers installed once, the two per-tile `NDPITIFFMessageHandler`
constructions disappear.

#### 4.5.3 CZI

`CZISlide::readBlock(pos, size, data)` is *already* a positional API; the body
merely implements it as seek-then-read on a shared `std::ifstream` that has
`exceptions(failbit | badbit)` set. Under `FileReader` it becomes:

```cpp
void CZISlide::readBlock(uint64_t pos, uint64_t size,
                         std::vector<unsigned char>& data) const
{
    data.resize(size);
    m_reader->readAt(pos, data.data(), size);
}
```

`m_fileStream` has four users; three of them — `readFileHeader`,
`readDirectory`/`readMetadata`, and `readAttachments` — run during `init()` and
migrate to `SequentialReader`. Only `readBlock` is on the read path.

`CZIScene::readTile` needs **no change**: `data` and `rasterData` are locals,
`decodeData` builds per-call OpenJPEG/JXR objects, `unpackChannels` writes into
per-call rasters, and `m_componentToChannelIndex`, `m_sceneParams` and
`m_zoomLevels` are read-only after `init()`.

Three defects are fixed in the same change because the code containing them is
being rewritten:

- The `catch` block does `m_fileStream.clear(); m_fileStream.seekg(0);` —
  error recovery by mutating shared state, which is exactly what is being
  removed. It disappears.
- `throw ex;` in that block slices the exception to `std::exception`. It
  becomes `throw;`. Pre-existing and unrelated, but free here.
- `CZIScene` reads through `m_slide`, a raw `CZISlide*`, on every tile, so a
  scene outliving its slide is already a dangling read. The scene takes a
  `shared_ptr<const FileReader>` directly instead, which removes the
  back-pointer from the read path and fixes the lifetime at the same time.

**CZI has no `ContextPool` and no `ReadContext`** — the row above says so, and
`grep -rn "ContextPool\|ReadContext" src/slideio/drivers/czi/` finds nothing.
An earlier draft of this section specified a `kUnbounded` pool whose context
held the decode scratch buffer; that turned out to buy nothing. Once the shared
`ifstream` is replaced by a `FileReader` that needs no per-thread state, the
only remaining candidate for a context was the scratch buffer, and
`CZIScene::readTile` keeps that as a plain local `std::vector<uint8_t>` — which
is thread-safe by construction, with no pool, no borrow and no userData field
to plumb. So the driver opts in with nothing but the reader swap, which is what
makes it the cheapest validation of the mechanism (§5, commit 4).

The one thing the abandoned pool would have bought is still available as a
future optimisation: the local buffer is allocated per tile, and a pooled
context would let it be reused across tiles of one read. That is a
single-threaded allocation cost, unmeasured, and not worth a pool on its own;
if the tile-read path is ever profiled again it belongs on the same list as the
fused repack (§2).

#### 4.5.4 VSI

Both halves are converted, or the driver ends up concurrent on ETS scenes and
serialised on TIFF ones — which would be a worse contract than either, because
`supportsConcurrentReads()` would differ between two scenes of one file.

`VsiFileScene` holds its own `TIFFKeeper m_tiff` and belongs to §4.5.1.

`EtsFile::readTilePart` has **two** races, and the second is the dangerous one:

```cpp
m_etsStream->setPos(offset);
m_buffer.resize(tileCompressedSize);     // m_buffer is a MEMBER
m_etsStream->readBytes(m_buffer.data(), m_buffer.size());
```

The stream is fixed by `FileReader`. `m_buffer` is not a handle and will not be
found by looking for file state, it survives fixing the stream, and it produces
**corrupted tiles rather than a crash** — the worst failure mode for a dataset
and invisible to any test that only checks for absent exceptions. It becomes
context-owned scratch:

```cpp
class EtsReadContext : public ReadContext
{
public:
    std::vector<uint8_t> buffer;         // pure scratch, no handle
};

void EtsFile::readTilePart(const TileInfo& tileInfo, EtsReadContext& ctx,
                           cv::OutputArray tileRaster) const
{
    ctx.buffer.resize(tileInfo.size);
    m_reader->readAt(tileInfo.offset, ctx.buffer.data(), tileInfo.size);
    ...
}
```

Context-owned rather than `thread_local`: the buffer then dies with the
`EtsFile`'s pool instead of living for the thread's lifetime, and it is
per-scene instead of per-process.

Marking `readTilePart` `const` makes the compiler find any remaining member
mutation, which is the cheapest available check that nothing else was missed.

`VSIStream` stays as the parsing API — `vsifile.cpp` and `EtsFile::init` use it
heavily at open time — reimplemented over `FileReader` with a local cursor
(`SequentialReader`).

### 4.6 Races that survive the file-handle fix

Collected here because they are the half of the work that review is most
likely to skip. Every item below except the message-handler swap fails
**without raising an exception** — as wrong pixels, as corrupted memory, or as
a read of freed state. That is what makes the ThreadSanitizer gate in §6
load-bearing rather than nice to have.

| Item | Where | Symptom if missed |
|---|---|---|
| `EtsFile::m_buffer` member scratch | `etsfile.cpp` `readTilePart` | corrupted tiles |
| `SVSScene::makeSureFileIsOpened` double open | `svsscene.cpp` | leaked handle, or read on a closed one |
| `PKETiledScene::readTile` re-fetching the handle | `pketiledscene.cpp` | mixed handles inside one tile |
| CZI error recovery mutating the shared stream | `CZISlide::readBlock` | wrong data in the *other* thread's read |
| `CZIScene::m_slide` raw back-pointer | `cziscene.cpp` | dangling read on scene outliving slide |
| global libtiff handler swap | §4.4 | torn global; lost or misrouted diagnostics |
| `TIFFFiles::getOrOpen` map insert | OME-TIFF, deferred | container corruption (UB) |

### 4.7 Lifetime and shutdown

Closing a `Slide` or `Scene` while worker threads hold borrows must not free a
handle in use. The rule is placed in one implementation:
`ContextPool::~ContextPool` blocks until every outstanding `Borrow` has been
returned, and refuses new acquisitions once destruction has begun (throwing,
so a read racing a close fails loudly instead of touching freed state).

Because contexts are free-list rather than thread-affine, that is sufficient
*for the contexts*: there is no context reachable only from a thread that has
already exited.

**What the pool does not protect, and why the caller has to.** The pool
guarantees the lifetime of the **contexts** — handles and scratch buffers — and
nothing else. It cannot guarantee the lifetime of the scene's own parsed model,
because the pool is a *member* of the scene, and members are destroyed in
reverse declaration order, derived class before base. A read in flight
dereferences much more than its context: `SVSTiledScene::readTile` reads a
`TiffDirectory*` into `m_directories`, `PKETiledScene::readTiffTile` indexes
`m_directories` and `m_zoomDirectoryIndices`, `NDPIScene::readTile` reads
`m_pfile->directories()`. So:

- Within a class, the pool must be declared **last**, after everything the read
  path reads. It is then destroyed first and blocks before that state is freed.
  That ordering is load-bearing in `SVSTiledScene`, `SVSSmallScene`,
  `PKETiledScene`, `PKESmallScene`, `SCNScene`, `VsiFileScene` and `NDPIFile`,
  and each records it at the member.
- Across a hierarchy, the pool must live in the **most-derived** class that
  owns read state. A pool in a base class is destroyed after the derived
  members an in-flight read is still using. This is why the SVS and PKE pools
  live in the concrete tiled/small scenes rather than in `SVSScene`/`PKEScene`.
- **Shared ownership held across the read is the reference pattern**, and the
  only one that is robust independently of declaration order. `CZIScene` holds
  a `shared_ptr<const FileReader>`, and `EtsFileScene` copies its
  `shared_ptr<EtsFile>` into a local for the whole read
  (`etsfilescene.cpp`) — either keeps the object alive for the duration of the
  read no matter what the owner does. A new driver should prefer it.

**And the caller still owns the top of the chain.** None of the above makes it
safe to destroy a `Slide` or `Scene` while a read on it is in flight: the
`Scene` object itself, and its members, go away at that point. The public
contract in `scene.hpp` therefore states the requirement — keep the scene alive
(or join the reader threads) until every read has returned. The pool makes a
close that races a read fail loudly rather than silently return freed handles;
it does not make it legal.

The failure mode if this is wrong is platform-asymmetric and worth naming:
on POSIX a leaked descriptor is invisible until the process runs out; on
Windows it is a file the user cannot delete after closing the slide. Windows
is the primary development platform, so that is the shape the bug will take
first, and it gets its own test (§6).

### 4.8 `RefCounter` — a note, not a change

`CVScene` derives from `RefCounter` and every read entry point constructs a
`RefCounterGuard`. The counter is an `std::atomic<int>`, so it is safe as it
stands, and no change is needed.

The note is that `increaseCounter`/`decreaseCounter` invoke the virtual hooks
`initializeCounter()` and `cleanCounter()` on the 0→1 and 1→0 transitions.
Nothing in the tree overrides either today. They are, however, exactly where
someone would later put lazy file open and close — and under concurrent reads
the count oscillates through zero, so a driver doing that would open and close
the file repeatedly and race a close against another thread's open. The hooks
must not be used for handle lifecycle; that is what `ContextPool` is for. A
comment in `refcounter.hpp` records this.

---

## 5. Landing strategy

Seven commits. The grouping is chosen so that each one is independently
reviewable and no commit leaves the tree with a driver claiming a guarantee it
does not yet honour.

| # | Commit | Contents |
|---|---|---|
| 1 | libtiff handlers installed once | §4.4. No contract change; a standalone improvement that also removes NDPI's per-tile swap. Ships value even if the rest slips. |
| 2 | the contract | §4.1: `supportsConcurrentReads`, `lockIfSerialised`, the three call sites, the 4D symmetry fix. No driver opts in yet, so behaviour is unchanged. |
| 3 | the mechanisms | §4.2 and §4.3: `ReadContext`, `ContextPool`, `FileReader`, `SequentialReader`, plus their unit tests including the TSan job. No driver uses them yet. |
| 4 | CZI | §4.5.3, opts in. First driver, and the one whose read path needs no change beyond the reader swap — so it validates the mechanisms against the smallest diff. |
| 5 | the TIFF family | §4.5.1: SVS, PHTIFF, PKE, SCN, `VsiFileScene`. One accessor per driver; they share a single pattern, so splitting them costs more review than it saves. |
| 6 | NDPI | §4.5.2, opts in. Separate because the pool lives in `NDPIFile` rather than the scene. |
| 7 | VSI/ETS | §4.5.4, opts in — completing VSI, whose TIFF half landed in 5. `m_buffer` is fixed here. |

Commit 1 must precede 4–7. Commits 2 and 3 must precede 4. Commits 4, 5, 6 and
7 are independent of each other.

`TiffConverter::cloneScene()` becoming unnecessary is deliberately **not** in
this sequence. It is a follow-up, taken only once §6's gates are green on the
drivers the converter uses.

---

## 6. Verification

Parallel reads are a correctness change, and §4.6 establishes that the
characteristic failure is wrong pixels rather than a crash. Gates are therefore
written against that, not against absence of exceptions.

**Byte-exactness, per driver, every platform.** A stress test reads one scene
from 16 threads and compares **every tile byte-for-byte** against the
single-threaded result. Run for each converted scene type: SVS, PHTIFF, PKE,
SCN, NDPI, CZI, VSI/ETS, `VsiFileScene`. Covering ROI edges, partial edge
tiles, `channelIndices` empty and non-empty, and both `readResampledBlock*`
and `readResampledLevelBlock*` paths. Checking only for absent exceptions does
not test this: `EtsFile::m_buffer` would corrupt tiles while every smoke test
passed.

**ThreadSanitizer, split by what CI can reach.** The mechanism-level suites --
`FileReader.*` and `ContextPool.*` -- run under ThreadSanitizer on every push
(`tsan-linux` in `build-validation.yml`); they need no slide corpus and cover
the code where a generic concurrency bug like `m_buffer` would live. The
per-driver byte-exactness tests above need the corpus, which CI does not
carry, and `CLAUDE.md` requires CI to leave `SLIDEIO_SKIP_MISSING_IMAGES`
unset, so a corpus-less run of those tests would fail rather than skip and
cannot run there. They must instead be run under ThreadSanitizer on a machine
with the corpus before each driver's opt-in commit merges, built **debug, not
release** -- `CMakeLists.txt` strips symbols and lets `-O3` win over an
explicit `-O1` in a release build, which would blind TSan's stack traces for
no benefit here:

    CXXFLAGS="-fsanitize=thread -g -O1" CFLAGS="-fsanitize=thread -g -O1" \
    LDFLAGS="-fsanitize=thread" python install.py -a install -c debug -bd build-tsan
    ./build-tsan/debug/bin/slideio_tests --gtest_filter="*concurrentReads*"

This gate has a platform gap that is stated rather than implied: **MSVC has no
ThreadSanitizer**, so Windows — the primary development platform — gets the
byte-exactness test only. Anything found by TSan is found on Linux, on
whichever compiler that runner defaults to -- GCC and clang both implement
ThreadSanitizer, and this document does not pin one. The implication for
sequencing is unchanged: the per-driver ThreadSanitizer run above must be
green before a driver's opt-in commit merges, not after the last driver lands
-- it is a required local/manual step, since CI cannot carry it.

**As practiced, this gate has not been met for any driver.** The run needs a
Linux build with the image corpus, and development has happened on a
Windows-only machine throughout, for ZVI and for every driver converted before
it. `software-docs/TECH_DEBT.md` §20.3 is the first place this is stated
plainly rather than left implicit -- `TECH_DEBT.md` names a TSan gap only in
the ZVI entry, which is a symptom of the same machine limitation rather than
evidence that the other nine were actually run. This document should stop
asserting the run as established practice until a Linux machine with the
corpus is actually in the loop for a driver's opt-in commit; §20.3 is the
standing item that tracks closing it.

**Lifetime.** A test that closes a `Slide` while 16 reader threads are mid-read
and then asserts the file can be deleted (`std::filesystem::remove` succeeds on
Windows) — the §4.7 failure, in the platform-visible form.

**Short reads.** A `FileReader` test that a `pread` returning fewer bytes than
requested is retried to completion, via an injected fake or a filesystem known
to return partial reads. This is the one guarantee actively lost in the
migration off `ifstream::read`, so it is tested rather than assumed.

**Diagnostics still routed.** After §4.4, a check that a libtiff warning
raised during a read still reaches the log — and still does so after
concurrent reads, which is where the old swap-per-keeper design would have
lost it.

**Contract coverage.** A test asserting `supportsConcurrentReads()` is `true`
for the scenes of exactly the eight converted formats and `false` for ZVI,
DCM, GDAL and OME-TIFF, so the §3.2 exemptions cannot silently drift.

**No throughput gate in this specification.** Per §2, a benchmark taken before
the fused repack and `Tiler::getTileIndices` land is not evidence about this
work. The scaling measurement — 1/2/4/8/16 threads over one `Scene`, against
the serialised baseline and against a `Slide` pool of the same width — belongs
to the analysis document's §7 harness and runs after those two items.

Existing suites must stay green throughout: `slideio_tests`,
`slideio_converter_tests`, `slideio_ndpi_tests`, `slideio_vsi_tests`,
`slideio_pke_tests`, `slideio_phtiff_tests`.

---

## 7. Documentation updates

### 7.1 `scene.hpp` and `cvscene.hpp`

The public threading guarantee changes from "thread-safe and serialised" to
"thread-safe; concurrent where the scene reports `supportsConcurrentReads()`".
Both headers say so, and `Scene`'s documentation points a caller at the query
rather than letting them assume either answer.

### 7.2 `TECH_DEBT.md`

- **#4 (`CVScene` serialises every block read, and does so inconsistently)** —
  resolved for the scenes of the eight converted formats; the inconsistency in
  `assemble4DBlock` is resolved outright. The entry is updated, not deleted,
  because four drivers remain serialised.
- **#3 (`SCNSlide` passes a `TIFF*` where `SVSSmallScene` expects a `bool`)** —
  gains the §4.5.1 constraint: the fix must drop the argument, not plumb the
  slide's handle into the scene.
- **New entries, one per deferred driver** — ZVI, DCM, GDAL and OME-TIFF. Each
  records that the work is now "add a `ReadContext` subclass holding the
  per-thread object (`ole::compound_document`, `DCMFile`, GDAL dataset, or the
  `TIFFFiles` handle map) and choose the pool's cap", plus the specific hazard:
  for OME-TIFF that the `getOrOpen` map race is memory corruption rather than
  wrong pixels; for ZVI and DCM that a per-thread replica costs N× parse, and
  that resolving tile extents once at `init()` (OLE sector chains, DICOM
  basic-offset tables) is the alternative if either ever matters for
  throughput.

### 7.3 `BREAKING_CHANGES.md`

Under `v2.10.0`: the documented threading contract for `Scene` reads changes.
No signature changes and no source or binary incompatibility — but a caller
that relied on reads of one scene being mutually exclusive to protect *its own*
state now needs its own lock. That is exactly the kind of silent behavioural
change the file exists to record.

### 7.4 `CLAUDE.md`

The "Key Design Patterns" list gains the concurrency contract and names
`ContextPool` as the one mechanism for per-thread read state, so the next
driver added does not invent a second one.

---

## 8. Out of scope

- **Items 1, 3 and 8 of the analysis's sequencing table** — the fused YCbCr
  repack, `Tiler::getTileIndices`, and the redundant clears and identity
  resize. They should land before this work is benchmarked (§2) but they are
  separate changes with no shared code.
- **ZVI, DCM, GDAL and OME-TIFF** — §3.2, recorded in `TECH_DEBT.md`.
- **`read_batch` itself**, the `out=` read parameter, `Scene.__reduce__`, and a
  C++ batch entry point — `slideio-python` and `slideio-tiling`. This
  specification is the prerequisite they were waiting on, not their design.
- **A `Slide` pool** — the interim workaround this change makes unnecessary.
- **`cv::parallel_for_` inside `composeRect`** — the second axis of
  parallelism, for large-block callers such as a tiled viewer. It needs the
  same prerequisites and is a small follow-up, but a 512² output tile yields
  only 4 to 9 tasks, so it is not what `read_batch` wants.
- **Retiring `TiffConverter::cloneScene()`** — a follow-up once §6 is green.
- **Any public tuning knob** for the pool bound (§3.3).
