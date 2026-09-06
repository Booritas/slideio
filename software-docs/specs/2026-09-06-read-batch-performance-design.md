# `read_batch` performance — source analysis and proposal

**Status:** analysis / for discussion
**Date:** 2026-09-06 (rev. 3 — adds §3.1.1, the per-driver stateless-I/O design for CZI, VSI and ZVI)
**Baseline read:** `slideio` @ main, `slideio-python` @ main (2026-09-06)
**Companion to:** the ML tiling design doc, `slideio-tiling/docs/ml-tiling-design.md` (§5.2 defines `grid.read_batch`)
**Scope:** what makes tile reads slow today, and what `read_batch` has to do — and what the core has to do — to be fast.

---

## 1. Summary

`read_batch` is where the ML tiling workflow spends essentially all of its time, so it is worth being precise about where that time goes. Reading the sources plus a set of micro-benchmarks says the per-tile cost is dominated by four things, in this order:

| # | Cost | Where | Measured / estimated |
|---|---|---|---|
| 1 | Reads on one `Scene` are fully serialised | `CVScene::m_readBlockMutex` | caps throughput at 1 core |
| 2 | RGBA repack on the YCbCr-JPEG path | `TiffTools::readNotRGBTile` | **0.214 ms/tile**, vs 0.031 ms fused |
| 3 | `composeRect` walks *every tile of the level* per read | `TileComposer::composeRect` | **0.235 ms** per read at level 0 of a 100k×80k slide |
| 4 | Grid misalignment against codec tiles | caller / `TileGrid` | **2.25×** more tile decodes than necessary |

For reference, decoding one 256×256 YCbCr JPEG tile is **0.195 ms**. Items 2–4 together cost *more than the decoding they exist to support*.

Arithmetic for one 512×512 output tile at level 0 of a 100k×80k SVS, unaligned grid, YCbCr JPEG (summing measured components, not an end-to-end measurement):

```
today     scan 0.235 + 9 tiles x (decode 0.195 + repack 0.214 + identity resize 0.017)
                                                          + 2 block clears 0.031   =  4.2 ms   ~240 tiles/s
after 2-4 indexed lookup + 4 tiles x (decode 0.195 + fused repack 0.031)
                                                          + 1 block clear 0.016    =  0.93 ms  ~1075 tiles/s
```

**~4.4× single-threaded, before any concurrency**, and every one of those changes is local. Item 1 then multiplies on top of it — and only after 2–4 are fixed is it multiplying something worth multiplying.

**On item 1, rev. 2 changes the recommendation.** Rev. 1 proposed working around the read lock with a pool of open `Slide`s, and deferring any core change. Having read what the lock actually protects, the better answer is to remove the serialisation itself: make `read_block` genuinely parallel. It is a bounded change — one positional-I/O change, one handle-pool mechanism, one process-global fix — and it is *not* the "audit eleven drivers for re-entrancy" project it sounds like, because the decode path is already thread-clean. §3.1 has the evidence. A `Slide` pool remains the right interim step, but it is now the interim, not the destination: it is a workaround that only `slideio_tiling` gets, whereas parallel `read_block` benefits every caller — including the converter, which pays `cloneScene()` today for exactly this reason.

---

## 2. Method, and what these numbers are not

Micro-benchmarks run in this session's Linux container (x86-64, libtiff 4.5.1, OpenCV 4.x via `opencv-python-headless`), against a **synthetic** 8192×8192 tiled TIFF: 256×256 tiles, `COMPRESSION_JPEG`, `PHOTOMETRIC_YCBCR`, 2×2 subsampling, q80, gradient+noise content, warm page cache.

That is deliberately the shape of an Aperio SVS level, but it is not an SVS, there is no cold-cache or network I/O in it, and there is no CZI/NDPI equivalent here. **Treat every number below as an indication of relative cost, not as a benchmark result.** §7 is the plan for measuring the real thing.

Two things were tested and are reported as-is even though one contradicts the obvious hypothesis:

- **Rejected:** replacing `TIFFReadRGBATile` with `TIFFSetField(JPEGCOLORMODE_RGB)` + `TIFFReadEncodedTile` is **not** faster here — 0.245 ms/tile vs 0.195 ms/tile for the RGBA decode. libtiff's RGBA path is doing fine; it is what slideio does *after* it that costs. Worth re-testing on real files before discarding entirely, but it should not be the first move.
- **Confirmed:** the repack after the decode is 0.214 ms/tile and can be 0.031 ms.

---

## 3. Findings

### 3.1 Reads on one `Scene` are serialised — and the serialisation is removable

`CVScene::readResampledBlockChannels` (`core/cvscene.cpp:48`) and `readResampledLevelBlockChannels` (`:285`) both take `std::lock_guard<std::mutex> lock(m_readBlockMutex)` (`core/cvscene.hpp:290`) around the whole read. The bindings already release the GIL on all four read paths (`src/pyscene.cpp:146,159,212,226`) and read straight into the numpy buffer, so the Python layer is not the problem: a worker thread drops the GIL and immediately blocks on the C++ mutex. Threading over one `Scene` yields exactly zero parallelism — silently, which is its own problem, because threading a `Scene` is the natural thing for a user to try.

#### What the lock actually protects

Two things, and only two:

1. **Stateful I/O.** The libtiff `TIFF*` (current directory, `tif_rawdata`/`tif_rawcc`, JPEG codec state), `CZISlide::m_fileStream`, `VSIStream`, the POLE storage behind ZVI.
2. **The process-global libtiff message handler swap** — see §3.2.

Everything else on the read path is already thread-clean, which is the finding that makes this tractable:

- Scene metadata (`m_levels`, `m_directories`, `m_zoomLevels`, `m_componentToChannelIndex`, `m_sceneParams`) is written in `initialize()` and read-only thereafter.
- `CZIScene::readTile` (`cziscene.cpp:551`) works entirely on locals — the encoded buffer, the decoded vector, the channel rasters.
- The codecs have no mutable globals: `jp2kcodec.cpp`, `jpegcodec.cpp`, `jxrcodec.cpp` and `jp2kmem.cpp` contain only file-scope *functions* plus per-call OpenJPEG objects.
- `getMetadata()` / `getChannelAttributes()` are already `std::call_once`.

So there is no re-entrancy audit of eleven drivers to do. There are two mechanisms to replace.

#### Why a naive scene pool does not work

Worth recording, because it answers design-doc open question 10.2 #1, and the answer is no — scenes do **not** all own their file handles:

| Driver | File handle | Two scenes of one slide read concurrently? |
|---|---|---|
| SVS / PHTIFF | `SVSScene::m_tiffKeeper`, ownership transferred by `keeper.release()` (`svsslide.cpp:95`) | yes — scene owns its handle |
| PKE | same pattern (`pkeslide.cpp`) | yes |
| **CZI** | `CZIScene::readTile` → `m_slide->readBlock` → **shared unsynchronised `std::ifstream`**, `seekg`+`read` (`czislide.cpp:67-79`) | **no** |
| **NDPI** | `NDPIScene::m_pfile` is a raw pointer to a shared `NDPIFile`; `readTile` uses `m_pfile->getTiffHandle()` | **no** |
| **VSI/ETS** | `EtsFile::m_etsStream` (`unique_ptr<VSIStream>`) shared by the scenes of that ETS | **no** |
| ZVI | shared POLE storage | **no** |

A pool therefore has to be a pool of `Slide`s — one open file per worker — which is exactly what `TiffConverter::cloneScene()` (`converter/tiffconverter.cpp:530`) already does: `openSlide(filePath, driverId)` then `getScene(sceneIndex)`, per reader thread, `hardware_concurrency()/2` of them. It works, and it makes the `fork()` question (10.2 #2) moot because each worker opens its own. Its cost is N× metadata parse and N× the parsed model in memory — cheap for SVS, not cheap for a CZI with a large sub-block directory.

#### Making `read_block` parallel instead

**CZI and VSI — nearly free; ZVI is not.** `CZISlide::readBlock(pos, size, data)` is *already* a positional API; the body just happens to implement it as seek-then-read on a shared stream. Replace it with a positional read and it becomes thread-safe with no pool, no extra descriptors and no extra memory. VSI is the same shape plus one hidden scratch-buffer race. ZVI's shared state is inside vendored OLE code and is a different problem. §3.1.1 works all three through in detail.

**TIFF family — a per-scene handle pool.** libtiff genuinely is not re-entrant on one handle, so each thread needs its own. But every read funnels through a single accessor — `SVSScene::getFileHandle()` (`svsscene.cpp:46`), `NDPIFile::getTiffHandle()`, the PKE equivalent — so the change is to turn that accessor into an RAII borrow from a small pool. The reason this is much cheaper than a `Slide` pool: `TiffTools::setCurrentDirectory` positions via `TIFFSetSubDirectory(hFile, dir.byteOffset)` (`tifftools.cpp:1040`), so a fresh handle jumps straight to the right IFD with no directory walk and no re-parse. You duplicate the file descriptor, not the parsed model.

**Then the lock.** `m_readBlockMutex` shrinks to guarding pool acquisition — a short critical section — or disappears entirely where the path has become stateless. The public contract changes from "thread-safe and serialised" to "thread-safe and concurrent", which is worth stating explicitly in the docs and testing for (§7).

#### Two axes of parallelism, and which one `read_batch` wants

- **Across `read_block` calls** (batch level): N independent tiles, disjoint outputs, no contention. This is the right axis for `read_batch` and for the converter.
- **Inside one `read_block`**: a `cv::parallel_for_` over the intersecting tiles in `composeRect`, each writing a disjoint sub-rect of the output — no locking needed on the destination. Right for a tiled viewer and for large-region pre-extraction, but a 512² output tile only yields 4–9 tasks, so the scaling is limited.

Both need the same prerequisite. Build the prerequisite once, do the batch axis first, and leave the in-read axis as a later option for the large-block callers.

The converter is worth mining for two other things while in here: it already reads **several tiles wide in one call** (`batchWidth`, `tiffconverter.cpp:515-525`), and it already carries idle-time instrumentation (`m_readersIdleTimeNs`, `m_encodersIdleTimeNs`) that can serve as the measurement harness for §7.

### 3.1.1 Stateless I/O for CZI, VSI and ZVI

These three are grouped in §3.1 as "the stream drivers", but they are three different amounts of work. Summary first:

| Driver | Change | Extra descriptors | Extra parse | Concurrent after? |
|---|---|---|---|---|
| CZI | one function + reader plumbing | 0 | 0 | yes |
| VSI/ETS | one function + thread-local scratch | 0 | 0 | yes |
| ZVI | declared serialised (option A below) | 0 | 0 | no, by decision |

For comparison, a `Slide` pool at 8 workers costs 8× the descriptors and 8× the sub-block directory parse for CZI — the case where that hurts most.

#### The shared piece: a positional reader

One class in `slideio-core`, used by all three, so the platform subtleties are written and tested once:

```cpp
class FileReader {                       // core/tools/filereader.hpp
public:
    explicit FileReader(const std::string& path);
    // Thread-safe. No shared cursor. Fills `size` bytes or throws.
    void readAt(uint64_t offset, void* dst, size_t size) const;
    uint64_t size() const;
private:
#ifdef _WIN32
    HANDLE m_handle;   // CreateFileW(..., FILE_FLAG_OVERLAPPED | FILE_FLAG_RANDOM_ACCESS, ...)
#else
    int m_fd;          // open(path, O_RDONLY | O_CLOEXEC); posix_fadvise(POSIX_FADV_RANDOM)
#endif
    uint64_t m_size;
};
```

Two things that are easy to get subtly wrong:

- **Windows.** `ReadFile` with an `OVERLAPPED` offset on a handle opened *without* `FILE_FLAG_OVERLAPPED` does read at the offset, but it also moves the file pointer, and concurrent operations on such a handle are not supported. That build is silently racy on the primary development platform. Open with `FILE_FLAG_OVERLAPPED`, put the `OVERLAPPED` on the stack per call with a `thread_local` manual-reset event, and finish with `GetOverlappedResult(..., TRUE)`. The file pointer is then not used at all and concurrent reads on one handle are supported.
- **POSIX.** `pread` is specified not to touch the file offset, so one fd is safe across threads — but it may return short. Loop until satisfied; `ifstream::read` hid that.

Memory-mapping would make all of this disappear (reads become `memcpy`, the page cache does the caching, inherently positional). Rejected: a truncated or network-backed file raises `SIGBUS` / an SEH exception in the middle of a `memcpy`, and handling that inside a library that has to survive arbitrary user files is not worth the saving.

The key property is that **one descriptor per file serves every thread** — no pool, no per-thread handle, no descriptor growth.

Parsing keeps a cursor. A thin `SequentialReader { reader, pos }` with `read<T>()` / `skip()` over `FileReader` serves the `init()` paths, which are single-threaded and should not be contorted.

#### CZI — one function

`m_fileStream` has four users; three (`readFileHeader`, `readDirectory`/`readMetadata`, `readAttachments`) run during `init()`. Only `readBlock` is on the read path.

```cpp
void CZISlide::readBlock(uint64_t pos, uint64_t size, std::vector<unsigned char>& data) {
    data.resize(size);
    m_reader->readAt(pos, data.data(), size);
}
```

`CZIScene::readTile` then needs **no change**: `data` and `rasterData` are locals, `decodeData` builds per-call OpenJPEG/JXR objects, `unpackChannels` writes into per-call rasters, and `m_componentToChannelIndex` / `m_sceneParams` / `m_zoomLevels` are read-only after `init()`.

Three details to pick up in the same change:

- The current `catch` does `m_fileStream.clear(); m_fileStream.seekg(0); throw ex;` — error recovery that mutates shared state, which is exactly what is being removed. It disappears.
- `throw ex;` slices to `std::exception`. Should be `throw;`. Unrelated pre-existing bug, free to fix here.
- Have `CZIScene` hold `shared_ptr<FileReader>` directly rather than reading through `m_slide`. `CZIScene::m_slide` is a raw `CZISlide*`, so a scene outliving its slide is already a dangling read; this removes the back-pointer from the read path and fixes that at the same time.

#### VSI — the stream, plus a scratch buffer that is easy to miss

`EtsFile::readTilePart` (`etsfile.cpp:141-147`) has **two** races, not one:

```cpp
m_etsStream->setPos(offset);
m_buffer.resize(tileCompressedSize);          // m_buffer is a MEMBER (etsfile.hpp:112)
m_etsStream->readBytes(m_buffer.data(), m_buffer.size());
```

The second one survives fixing the stream and is not a handle, so it will not be found by looking for file state. It produces **corrupted tiles, not a crash** — the worst failure mode for a dataset, and invisible to any test that only checks for exceptions.

```cpp
void EtsFile::readTilePart(const TileInfo& tileInfo, cv::OutputArray tileRaster) const {
    static thread_local std::vector<uint8_t> buffer;   // pure scratch: nothing to close
    buffer.resize(tileInfo.size);
    m_reader->readAt(tileInfo.offset, buffer.data(), tileInfo.size);
    ...
}
```

Marking the method `const` makes the compiler find any remaining member mutation. Note this is the *benign* use of thread-local storage — scratch memory with no handle and no lifetime coupling to the `Scene`, so none of the ownership problems of a thread-local I/O context apply.

`VSIStream` stays as the parsing API (`vsifile.cpp` and `etsfile.cpp:74-125` use it heavily at open time), reimplemented over `FileReader` with a local cursor.

Consistency point: `VsiFileScene` holds its own `TIFFKeeper m_tiff` and belongs to the libtiff fix, not this one. Convert both halves of the driver or it ends up concurrent on ETS scenes and serialised on TIFF ones.

#### ZVI — a different problem

The shared state is not a cursor slideio owns. `ZVIScene::m_Doc` is an `ole::compound_document`; `ZVIUtils::StreamKeeper` holds an iterator into the document's own vector of `stream_path` and returns a reference to a shared `ole::basic_stream`; `ZVIImageItem::readRaster` (`zviimageitem.cpp:163-198`) then calls `stream->seek()` / `stream->read()` on it, and in the JPEG branch seeks to the end to measure the stream. Shared mutable cursors, three layers down, in vendored code.

- **A — serialise ZVI, deliberately.** Keep `m_readBlockMutex` for this driver alone. ZVI is a legacy Zeiss format with small images and no real pyramid; it is not in the ML tiling path. Model it as a declared property on the base class — `bool supportsConcurrentReads()` — rather than leaving it looking like an oversight. **This is the recommendation.**
- **B — per-thread `compound_document`.** This is where the thread-local I/O context earns its keep: the OLE FAT/directory parse for a ZVI is small, so N× parse is cheap here in a way it never is for CZI. The right fallback for drivers whose I/O layer cannot be made stateless.
- **C — resolve extents once.** At `init()`, walk the compound document and record each `Image/Item(n)/Contents` stream as a list of `(fileOffset, length)` extents from its sector chain; a tile read then becomes `readAt` calls with no OLE involved, and ZVI takes the same shape as CZI — and gets faster single-threaded, since per-read stream navigation disappears. Cost: taking ownership of the OLE sector-chain logic including the mini-FAT for streams under 4096 bytes. A couple of days, self-contained, worth it only if ZVI ever matters for throughput.

### 3.2 The global libtiff message handler is a blocker for any of this

`TIFFMessageHandler` (`imagetools/tiffmessagehandler.cpp:79`) and its NDPI twin swap libtiff's **process-global** error and warning handlers in the constructor and restore them in the destructor. `tiffkeeper.hpp` already documents the non-LIFO hazard and correctly calls today's consequence "lost log routing, not a crash" — under concurrent reads it stops being that and becomes two threads writing the same global, outside any lock.

Separately, and independent of concurrency: NDPI constructs one **per tile**, in both `getTileRect` and `readTile` (`ndpiscene.cpp:369,418`). Since `getTileRect` is called once per tile of the whole level by the scan in §3.4, an NDPI read at level 0 of a large slide performs on the order of 10⁵ global handler swaps.

**Fix:** install the handlers once at library init — next to `initLogging()` in `ImageDriverManager` — and never swap them again. Small, removes a per-tile cost today, and is a precondition for anything in §3.1.

### 3.3 The YCbCr repack is the single largest local win

`TiffTools::readTile` (`imagetools/tifftools.cpp:736`) dispatches on photometric. **Photometric 6 is YCbCr, which is what JPEG-compressed SVS, NDPI and Philips TIFF normally are**, so `readNotRGBTile` (`:826`) is the hot path for most brightfield WSIs. It:

1. allocates a 4-channel tile raster and calls `TIFFReadRGBATile`,
2. runs `cv::extractChannel` three times,
3. `cv::merge`s the three back together,
4. `cv::flip`s the result vertically,

five passes over a 256 KB buffer, plus 33% extra memory traffic from carrying an alpha channel nobody wants.

```
current   3x extractChannel + merge + flip : 0.2141 ms   (256x256, 4->3 ch)
fused     cvtColor(RGBA2RGB) + flip        : 0.0313 ms
          mixChannels + flip               : 0.0647 ms
```

**0.214 ms → 0.031 ms, a 6.8× reduction on a step that currently costs more than the JPEG decode it follows.** The `channelIndices.empty()` case — which is what every ML read does — is a plain `cvtColor(..., COLOR_RGBA2RGB)`. The flip can be folded in too by writing rows in reverse into the destination, and the general `channelIndices` case is one `cv::mixChannels` with a computed `fromTo` list rather than a loop of `extractChannel`.

This is a change to one function, with no API surface. It should be the first thing done.

`SLIDEIO_LOG(INFO)` at `tifftools.cpp:842` also fires once per tile. `FLAGS_minloglevel` defaults to `GLOG_FATAL` (`imagedrivermanager.cpp:31`), but glog still constructs the `LogMessage` and formats its header before deciding to discard it. Small next to the above, but it is in the innermost loop and it is free to remove.

### 3.4 `composeRect` scans every tile of the level, on every read

`TileComposer::composeRect` (`core/tools/tilecomposer.cpp:28`):

```cpp
const int tileCount = tiler->getTileCount(userData);
for (int tileIndex = 0; tileIndex < tileCount; tileIndex++) {
    cv::Rect tileRect;
    tiler->getTileRect(tileIndex, tileRect, userData);   // virtual call
    cv::Rect intersection = blockRect & tileRect;
    if (intersection.area() > 0) { ... }
}
```

`tileCount` is the tile count of the **whole level**, not of the requested block. Reading a 512×512 tile from level 0 tests every tile in the slide to find the four to nine it needs. Every driver in the tree goes through this — SVS, PHTIFF, PKE, CZI, NDPI, VSI, SCN, OME-TIFF, ZVI, DCM.

Measured (tight loop, statically dispatched — the real one makes a virtual call per iteration, for PKE/OME-TIFF re-derives `getTileCount()` inside `getTileRect`, and for NDPI swaps a global handler per iteration, so this is a floor):

```
20k x 15k    4,661 tiles  ->  0.010 ms per 512x512 read
50k x 40k   30,772 tiles  ->  0.064 ms
100k x 80k 122,383 tiles  ->  0.235 ms
200k x100k 305,762 tiles  ->  0.577 ms
```

Against ~1.8 ms of useful decode for that block, 0.235 ms is ~13% of the read on a 40× slide — and it is O(slide area), so it scales the wrong way: the bigger the slide, the larger the share. Extracting a million tiles from a large slide spends minutes purely on this scan.

**Fix:** give `Tiler` a way to name the tiles that intersect a rect.

```cpp
// core/tools/tilecomposer.hpp
virtual void getTileIndices(const cv::Rect& blockRect, std::vector<int>& indices, void* userData) {
    // default: current linear scan, so no driver is forced to change at once
}
```

`composeRect` calls it and iterates the result. Regular-grid drivers (SVS, PHTIFF, PKE, NDPI-tiled, OME-TIFF, VSI/ETS, SCN) override it with the row/column arithmetic they already have in `getTileRect`, inverted — O(k) in the tiles actually touched, no virtual call per level tile. CZI's tiles are an irregular mosaic, so it gets a coarse uniform-grid bucket index built once in `computeSceneTiles()` and queried per read.

Backwards compatible, driver-by-driver, and it removes an unbounded cost.

### 3.5 Copies and clears that nothing needs

Per read, in order:

1. `Scene::readResampledBlockChannels` (`slideio/scene.cpp:172`) wraps the caller's buffer and does `raster = cv::Scalar(0)` — a full clear.
2. `initializeSceneBlock` (`core/cvscene.cpp:312`) then does `output.setTo(255)` (or 0) — **a second full clear of the same buffer**.
3. Per tile, `composeRect` calls `Tools::resize(tileRaster, scaledTileRaster, scaledTileRect.size())` **unconditionally**. `Tools::resize` (`core/tools/tools.cpp:121`) ends in `cv::resize`, which does not short-circuit on `dsize == src.size()`. The native-level ML read — `read_block_from_level` with `size == rect size`, exactly the `TileGrid` fast path — therefore pays a full interpolation pass plus an allocation per tile purely to copy.
4. `tilePartRaster.copyTo(blockPartRaster)` — the one copy that is actually needed.
5. Inside `readRegularTile`, the interleaved all-channels case decodes into a temp and then `tileRaster.copyTo(output)` — one more.

Measured, for a 512×512×3 block and 256×256 tiles:

```
raster = cv::Scalar(0)                  : 0.015 ms   (redundant)
identity cv2.resize, 256x256x3          : 0.017 ms   per tile
identity cv2.resize, 512x512x3          : 0.048 ms
```

Fixes, all small:

- Drop the clear in `Scene::readResampledBlockChannels`; `initializeSceneBlock` already covers it.
- Skip `initializeSceneBlock`'s fill when the tiles fully cover the block — track coverage in `composeRect` and fill only the uncovered remainder. Interior tiles are the common case and never need it.
- In `composeRect`, when `scaledTileRect.size() == tileRect.size()`, alias instead of resizing.
- Let the tile decode write into the destination sub-rect where the geometry allows, rather than into a temp that is then copied.

### 3.6 Grid alignment is worth more than any of the above, and costs nothing

With 256×256 codec tiles and a 512×512 output grid: an aligned grid spans 2×2 = **4** codec tiles per output tile; any non-zero offset spans 3×3 = **9**. **2.25× the decodes, for a geometry choice.**

`LevelInfo::getTileSize()` is already exposed to Python as `LevelInfo.tile_size` (`pybind.cpp:284`), so `align="codec"` from ml-tiling-design §5.1 is buildable today with no core change — and on these numbers it is not a micro-optimisation, it is the largest single factor in `read_batch` throughput after the lock. It should move up the priority list within `slideio_tiling`.

The complement is a **decoded-tile cache** (design doc §6.8) keyed by `(scene, level, codec tile index)`. With `overlap > 0`, or output tiles smaller than codec tiles, or misalignment that cannot be avoided because the ROI dictates the origin, neighbouring output tiles re-decode the same codec tiles at 0.195 ms each. Combined with `iter_native_order()`, a cache of a couple of tile rows turns most of those into hits. Note that once §3.1 lands the cache becomes shared mutable state on the read path — it needs its own lock, or a sharded/thread-local design, and it is the one new component here that does.

### 3.7 The binding layer

Not a bottleneck today, but `read_batch` is where it changes shape.

- Each `read_block*` call allocates a fresh numpy array (`pyscene.cpp:142,208`). For N tiles that is N allocations feeding a `np.stack` the caller then has to do — plus a copy. An `out=` parameter, or a batch entry point writing into one preallocated `(N, H, W, C)`, removes both. `Scene::readResampledLevelBlockChannels` already takes `void* buffer, size_t bufferSize`, so the plumbing exists.
- `std::vector<int> channelIndices` is taken by value in `PyScene::readBlock` and copied again in each layer below (`std::vector<int> channels(channelIndices)` in every driver, `Tools::completeChannelList` per tile in `CZIScene::readTile`). Noise per read, real per tile.
- `Scene` has `close()`, `__del__` and context-manager support but no `__reduce__` (`slideio/wrappers/py_slideio.py`), so it does not pickle and `spawn`-based `DataLoader`s (Windows, macOS) cannot carry one into a worker. Making it reconstructible from `(file_path, driver_id, scene_index)` is small and worth doing regardless — and it is the same triple `cloneScene()` uses.

Once `read_block` is genuinely parallel, a C++ batch entry point becomes the natural home for the fan-out: one GIL release for the whole batch, an internal thread pool over the handle pool, writing into the caller's `(N,H,W,C)` array. It is still not the starting point — a Python `ThreadPoolExecutor` gets most of it, because the GIL is already released — but it stops being an odd special case and becomes the obvious place for the pool to live.

---

## 4. What `read_batch` should be

```python
grid.read_batch(indices, workers=None, out=None) -> ndarray  # (N, H, W, C)
```

The API is the same whichever way the concurrency is obtained; only the pool behind it changes.

- **Interim: per-worker `Slide`.** A pool that opens `openSlide(path, driver_id)` once per worker thread and holds `slide.get_scene(scene_index)` in thread-local state. Not `get_scene()` N times on one `Slide` — see §3.1. This ships against 2.9.0 unchanged.
- **Destination: one `Scene`, parallel reads.** Once §3.1 and §3.2 land, the same `read_batch` drops the pool and threads a single `Scene`, with no duplicated parse and no N× memory. The pool becomes a compatibility path for older cores, selected at import time on `slideio.get_version()`.
- **One output allocation.** Allocate `(N, H, W, C)` up front, or accept the caller's `out=`; each worker fills its slice. No per-tile numpy array, no `np.stack`.
- **Codec-order dispatch.** Sort `indices` by codec tile locality before handing them out, not by grid index. Sequential decode order beats random access substantially on JPEG-in-TIFF, and only the library knows the native order.
- **Coalesce runs.** Contiguous tiles in a row read as one wider block, the way the converter's `batchWidth` already does. This mostly disappears as a concern once §3.4 lands, but it also halves call overhead.
- **`workers=None` means a sensible default**, not "all cores": decode is CPU-bound at ~0.2 ms/tile, so `min(8, os.cpu_count())` with the value recorded in the manifest is a better starting point than oversubscribing against a disk.
- **Exceptions are per-tile.** One unreadable tile in a batch of 512 must not lose the batch; return the background block and record it, the way `SVSTiledScene::readTile` already does per tile.

---

## 5. Sequencing

| Order | Change | Repo | Effort | Expected |
|---|---|---|---|---|
| 1 | Fuse the RGBA repack in `readNotRGBTile`; drop the per-tile `LOG(INFO)` | `slideio` | one function | −0.18 ms/tile on YCbCr JPEG |
| 2 | Install the libtiff message handlers once at init; stop swapping them | `slideio` | small | removes a per-tile cost; unblocks 5–6 |
| 3 | `align="codec"` in `TileGrid` | tiling | geometry only | 2.25× fewer decodes |
| 4 | `Tiler::getTileIndices` + per-driver overrides | `slideio` | ~1 day + per driver | removes an O(slide area) cost |
| 5 | `FileReader` + positional I/O for CZI and VSI; ZVI declared serialised (§3.1.1) | `slideio` | one shared class + ~1 function each | CZI and VSI become concurrent |
| 6 | Per-scene TIFF handle pool behind `getFileHandle()`; shrink `m_readBlockMutex` | `slideio` | medium | TIFF family becomes concurrent |
| 7 | `read_batch` — `Slide` pool now, single `Scene` once 5–6 ship | tiling | small | N× |
| 8 | Drop the double clear; skip identity resize; fill only uncovered area | `slideio` | small | ~5–10% |
| 9 | Decoded-tile cache (needs its own locking, see §3.6) | tiling | medium | large with overlap |
| 10 | `Scene.__reduce__`, `out=` on reads | `slideio-python` | small | enables spawn DataLoaders |
| 11 | C++ batch entry point over the handle pool | `slideio-python` (+core) | medium | call overhead, one GIL release |
| 12 | `cv::parallel_for_` inside `composeRect` for large blocks | `slideio` | small, after 5–6 | viewer / pre-extraction path |

1, 2, 4, 5, 6 and 8 are core changes; none touches a public signature, so they can ride any 2.9.x — though 5 and 6 change the *documented threading contract*, which is a release-note item, not a silent change. 3, 7 and 9 need no core release at all, so `read_batch` can exist and be benchmarked before any of the core work lands.

Two ordering points worth keeping:

- **`align="codec"` moves from a refinement of `TileGrid` to a Tier 1 performance feature** — 2.25× for pure geometry beats most of the code changes here on payoff-per-effort.
- **Do 1 and 4 before measuring the value of 5–6.** Otherwise the concurrency benchmark measures a lock wrapped around a read path that is spending more than half its time on avoidable work, and overstates the case.

---

## 6. Corrections to the ML tiling design doc (`slideio-tiling/docs/ml-tiling-design.md`)

- **§5.2 / §10.2 #1 — answered, and it changes the design.** Scenes do *not* all own their file handles. CZI, NDPI, VSI/ETS and ZVI share a stream or handle across the scenes of one slide, unsynchronised. A pool must therefore be a **`Slide` pool**, one open file per worker, as `TiffConverter::cloneScene()` already does — "N file handles" in §5.2's table should read "N file handles **and N slide parses**".
- **§5.2's recommendation — superseded.** It framed the choice as "small, safe scene pool" versus "large, risky lock narrowing across eleven drivers", and recommended the pool. The second column was mis-priced: the work is not per-driver re-entrancy but *per-thread I/O context*, and the decode path is already thread-clean (§3.1). The pool is the right first step, but it should be planned as an interim, not as the answer.
- **§10.2 #2 (`fork()`) — mostly dissolved.** With a per-worker `Slide` nothing is inherited across a fork; with parallel `read_block` and threads it does not arise. Still worth documenting.
- **§10.2 #8 (what the read lock costs) — still the right experiment**, but run it *after* §5 items 1 and 4, per the ordering note above.
- **§6.8 (tile cache) gains a constraint:** once reads are concurrent it is shared mutable state and needs its own locking or a per-thread design.
- **§6.4 (ICC)** is unaffected, and the observation stands: there is no ICC handling in the read path.

---

## 7. Validation

Everything above needs to be re-measured on real files before anything is merged. The harness:

- **Files:** one Aperio SVS (JPEG/YCbCr), one SVS (JPEG2000), one Hamamatsu NDPI, one Zeiss CZI, one Philips TIFF. Small and large of each where possible — §3.4 only bites at scale.
- **Metric:** tiles/sec at 512×512, level 0 and at 0.5 µm/px, cold and warm cache.
- **Ablation, in this order**, so each change is attributable: baseline → fused repack → codec alignment → indexed tile lookup → clears/resize → 1/2/4/8/16 workers → with tile cache → overlap 0 vs 64 → random vs native order.
- **The concurrency comparison** (design doc §10.2 #8): threads over one `Scene` before the change (the serialised baseline), a `Slide` pool of N, N processes, and threads over one `Scene` after §3.1. The gap between the pool and the last one is what the pool costs in handles, parse time and memory — the number that justifies doing 5–6 at all.
- **Thread-safety gates, not just speed gates.** Parallel reads are a correctness change, and the characteristic failure is *wrong pixels*, not a crash — `EtsFile::m_buffer` (§3.1.1) would corrupt tiles while every smoke test passes. So:
  - a stress test reading one scene from 16 threads and comparing **every tile byte-for-byte** against the single-threaded result, on every driver. Checking only for absent exceptions does not test this;
  - that test under ThreadSanitizer, and under Helgrind/DRD at least once per driver. TSan finds `m_buffer` immediately; review may not;
  - a short-read test for `pread` (a filesystem that returns partial reads, or an injected fake) — the one guarantee lost in the migration off `ifstream::read`;
  - a check that libtiff warnings still reach the log after concurrent reads (the §3.2 regression);
  - a check that closing a `Slide` while worker threads are alive releases every descriptor — the lifetime failure from the thread-local design, which on Windows shows up as a file the user cannot delete.

  None of these exist today because none of them could have failed today.
- **Correctness gate, non-negotiable:** tiles from `read_batch` must be pixel-identical to the equivalent single `read_block_from_level` call, on every driver, including ROI edges, `align="codec"`, and the fused repack path. The repack change in §3.3 alters channel handling on the most common code path in the library — it needs a byte-exact regression test against the current output before it lands, not after.

---

## 8. Caveats

All timings here are micro-benchmarks on synthetic data in a cloud container, chosen to isolate individual code paths; they establish *relative* cost, and the 4.4× in §1 is arithmetic over measured components, not an end-to-end result. The code sketches are illustrative — they have not been compiled, and any implementation needs review and its own benchmarks before merge. The concurrency work in particular changes a documented threading guarantee and touches every driver's I/O path; it wants the §7 sanitizer gates in place before the first driver is converted, not after the last one.
