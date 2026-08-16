# Explicit Zoom-Level Reading — Design

**Status:** Proposed 2026-08-16
**Addresses:** [issue #69](https://github.com/Booritas/slideio/issues/69), milestone 2.9.0
**Repositories:** `Booritas/slideio` (C++), `Booritas/slideio-python` (bindings)
**Scope options chosen:** separate method rather than a `level=` keyword; one new
virtual plus two public `Scene` methods; overrides in every pyramid driver plus
the missing GDAL level table; out-of-range rects clamp and background-fill.

## 1. Goal

Let a caller read a rectangle from a zoom level it names, with the rectangle
expressed in that level's own pixel coordinates.

- No conversion to full-resolution coordinates, so no rounding and no
  sub-pixel drift between levels.
- No second level selection inside the library, so no surprise read from a
  finer level followed by a resize.
- Additive: `read_block` and every existing `Scene` read method keep their
  current signatures and their current behaviour, bit for bit.

## 2. The request, and what it is really asking for

Issue #69 comes from a tiled whole-slide viewer. Such a viewer addresses image
data as `(level, tile_col, tile_row)` and already knows the rectangle it wants
in level coordinates. Today it must convert that rectangle to level-0
coordinates, in integers, and hand it to `Scene.read_block()`, which converts
it back. The reporter observes two consequences: visible geometric shifts when
a coarse tile is replaced by a fine one, and reads that are more expensive than
necessary.

Both are real, and both are visible in the code rather than merely plausible.

**Drift.** `Tools::scaleRect` (`src/slideio/core/tools/tools.cpp:190-200`)
floors the origin and ceils the far corner. A rectangle that leaves level
coordinates by one rounding rule and re-enters by another does not in general
come back to where it started. Two levels of the same pyramid can therefore
disagree about where a given tile boundary falls.

**Cost.** `Tools::findZoomLevel` (`src/slideio/core/tools/tools.hpp:51-85`)
picks a level from the requested zoom ratio. When the caller's rounding
perturbs that ratio away from an exact level scale, the search can settle on
the next finer level, and `TileComposer` then decodes more tiles than were
needed and downsamples them.

The important observation is that the requested API is not new machinery. It
is an entry point below two steps the drivers already perform. Every pyramid
driver's `readResampledBlockChannelsEx` has the same three-part shape:

```
zoom      = max(blockSize.w / blockRect.w, blockSize.h / blockRect.h)
level     = Tools::findZoomLevel(zoom, ...)
levelRect = Tools::scaleRect(blockRect, levelScale)
            TileComposer::composeRect(this, channels, levelRect, blockSize, out, userData)
```

Verified in `svstiledscene.cpp:119`, `ndpiscene.cpp:261`, `scnscene.cpp:46`,
`cziscene.cpp:114`, `otscene.cpp:367`, `pketiledscene.cpp:181`,
`wsiscene.cpp:141` and `etsfilescene.cpp:151`. The reporter is asking to be
allowed to supply `level` and `levelRect` directly — the two values the driver
computes and then discards.

## 3. Current state of the level API

`CVScene` already carries a level table: `std::vector<LevelInfo> m_levels`
(`cvscene.hpp:238`), exposed as `getNumZoomLevels()` and `getZoomLevelInfo(int)`
(`cvscene.cpp:168-178`), surfaced on `Scene` as `getNumZoomLevels()` and
`getLevelInfo(int)`, and in Python as `scene.num_zoom_levels` and
`scene.get_zoom_level_info(index)` (`pybind.cpp:77-78`). `LevelInfo` reports
level index, size, scale, magnification and tile size.

Two gaps matter here.

- `LevelInfo::getTileCount()` and `LevelInfo::getTileRect(int)`
  (`levelinfo.hpp:89-117`) exist in C++ but are not bound in Python
  (`pybind.cpp:255-261` binds only size, tile size, level, scale and
  magnification). These are exactly the helpers a tiled viewer needs, and the
  issue's example code reimplements `getTileRect` by hand.
- `GDALScene` and `PKESmallScene` never populate `m_levels`, so
  `getNumZoomLevels()` returns `0` for them. Every other scene class registers
  at least one level.

## 4. Scope

In scope: the new read path from `CVScene` through `Scene` to the Python
`Scene` wrapper, overrides in the pyramid drivers, the two missing level
tables, the two missing `LevelInfo` bindings, and tests.

Out of scope, deliberately, each recorded in §10:

- The per-scene read mutex that serialises all block reads.
- Any form of tile caching.
- A `read_tile(level, col, row)` convenience method.
- `NDPIScene`'s separate `Striped` and `SingleStripe` directory types beyond
  making them work; no restructuring of that branch.

## 5. Design

### 5.1 The core contract

One new virtual on `CVScene`:

```cpp
/**@brief reads a raster rectangle from an explicitly selected zoom level.
 *
 * @param level : zoom level index, in [0, getNumZoomLevels()).
 * @param levelRect : rectangle in the pixel coordinate system of @p level,
 *   not in scene coordinates.
 * @param blockSize : size of the returned block. Resampling is performed
 *   from @p level only; the method never selects a different level.
 * ...
 */
virtual void readResampledLevelBlockChannelsEx(
    int level,
    const cv::Rect& levelRect,
    const cv::Size& blockSize,
    const std::vector<int>& channelIndices,
    int zSliceIndex,
    int tFrameIndex,
    cv::OutputArray output);
```

The contract:

1. `level` outside `[0, getNumZoomLevels())` raises. `getNumZoomLevels() == 0`
   raises with a message naming the driver's lack of a level table — after
   §5.6 no in-tree driver is in that state.
2. `levelRect` is interpreted in `level`'s coordinates. No scaling is applied
   to it, ever.
3. `blockSize` is the output size. `blockSize == levelRect.size()` means no
   resampling at all: `TileComposer` copies tile data verbatim. A different
   `blockSize` resamples within the level and does not change which level is
   read.
4. The part of `levelRect` outside the level's bounds is background-filled,
   the same treatment `read_block` already gives via `TileComposer`'s
   tile-intersection loop and `initializeSceneBlock`. A viewer may therefore
   request a full-size edge tile and receive a full-size array.
5. Callers reach this through the locking wrappers of §5.3, not directly, so
   the existing `m_readBlockMutex` discipline is unchanged.

**The base-class default.** `CVScene` provides an implementation so that no
driver is obliged to override:

1. Validate `level`.
2. Intersect `levelRect` with `{0, 0, levelSize.width, levelSize.height}`. If
   the intersection is empty, initialise the output to background and return.
3. Map the intersection to base coordinates by dividing by
   `LevelInfo::getScale()`, compute the corresponding sub-rectangle of
   `blockSize`, and delegate to `readResampledBlockChannelsEx`.
4. Copy the result into the correct sub-rectangle of a background-initialised
   output of size `blockSize`.

For a scene with a single level, `scale == 1.0`, so steps 3 and 4 reduce to a
clamp, a delegation and a paste — exact, and no worse than `read_block`. This
is the path GDAL, ZVI, the non-WSI DICOM scenes, `VsiFileScene` and the
`*SmallScene` classes take. The scaled branch is insurance for a future
multi-level driver that does not override; it is correct but may re-select a
level, which is precisely what an override exists to avoid.

The clamp in step 2 is not optional politeness.
`GDALScene::readResampledBlockChannelsEx` does `sceneRaster(blockRect)`
unguarded (`gdalscene.cpp:97`), which throws on an out-of-range rectangle.
Without the clamp, GDAL would violate contract point 4.

### 5.2 Driver overrides

Each pyramid driver's existing method splits at the seam identified in §2. The
old entry point keeps computing exactly the `level` and `levelRect` it computes
today and then calls the new one; the new one holds the body that already
exists. Taking `SVSTiledScene` as the model:

```cpp
void SVSTiledScene::readResampledBlockChannelsEx(
    const cv::Rect& blockRect, const cv::Size& blockSize,
    const std::vector<int>& channelIndices, int zSliceIndex, int tFrameIndex,
    cv::OutputArray output)
{
    const double zoomX = static_cast<double>(blockSize.width)  / blockRect.width;
    const double zoomY = static_cast<double>(blockSize.height) / blockRect.height;
    const int level = findZoomLevelIndex(std::max(zoomX, zoomY));

    const TiffDirectory& dir = m_directories[level];
    const double zoomDirX = static_cast<double>(dir.width)  / m_directories[0].width;
    const double zoomDirY = static_cast<double>(dir.height) / m_directories[0].height;
    cv::Rect levelRect;
    Tools::scaleRect(blockRect, zoomDirX, zoomDirY, levelRect);

    readResampledLevelBlockChannelsEx(level, levelRect, blockSize,
                                      channelIndices, zSliceIndex, tFrameIndex, output);
}

void SVSTiledScene::readResampledLevelBlockChannelsEx(
    int level, const cv::Rect& levelRect, const cv::Size& blockSize,
    const std::vector<int>& channelIndices, int zSliceIndex, int tFrameIndex,
    cv::OutputArray output)
{
    if (zSliceIndex != 0 || tFrameIndex != 0) {
        RAISE_RUNTIME_ERROR << "SVSDriver: 3D and 4D images are not supported";
    }
    validateLevel(level);
    if (getFileHandle() == nullptr) {
        RAISE_RUNTIME_ERROR << "SVSDriver: Invalid file header by raster reading operation";
    }
    const TiffDirectory& dir = m_directories[level];
    std::vector<int> channels = Tools::completeChannelList(channelIndices, dir.channels);
    TileComposer::composeRect(this, channels, levelRect, blockSize, output, (void*)&dir);
}
```

`findZoomDirectory(double)` becomes `findZoomLevelIndex(double)` returning the
index rather than the directory; it is private and has no external callers.

This is an extraction, not a rewrite. Nothing on the old path changes value, so
the existing suite is the regression proof — see §7.

Per-driver notes, all verified against the tree at `fd7b6371`:

- **`SVSTiledScene`** (`svstiledscene.cpp:119`) — as above. Serves SVS, PHTIFF
  and, through `AFISlide`, AFI.
- **`NDPIScene`** (`ndpiscene.cpp:261`) — same shape, but the level body
  branches on `NDPITiffDirectory::Type`. The `Tiled`, `SingleStripeMCU` and
  `Striped` branches use `TileComposer` and move unchanged. The `SingleStripe`
  branch reads the whole directory and crops with `cv::Mat block(raster,
  dirBlockRect)`, which throws on an out-of-range rectangle; the override must
  clamp `levelRect` there and pad, as the base default does.
- **`SCNScene`** (`scnscene.cpp:46`) — the one genuinely different driver. It
  resolves a directory **per channel** via `findZoomDirectory(channelIndex,
  zIndex, zoom)` (`scnscene.cpp:270-282`) and takes the level geometry from the
  first channel that resolves. `m_levels` is built from channel 0 / z 0's
  directory list (`scnscene.cpp:252-267`). The override therefore indexes each
  channel's directory list by `level` directly instead of searching it by zoom.
  If a channel's list is shorter than `level`, that channel resolves to
  `nullptr`, which `SCNTilingInfo` already tolerates — the same outcome the
  zoom search produces today for such a channel.
- **`CZIScene`** (`cziscene.cpp:114`) and **`WSIScene`** (`wsiscene.cpp:141`) —
  both pass `userData.relativeZoom = levelZoom / zoom` to the composer. Since
  `levelRect.width ≈ blockRect.width * levelZoom` and
  `blockSize.width = blockRect.width * zoom`, that quantity is
  `levelRect.width / blockSize.width`, computable from the level-space
  arguments alone. It survives the split unchanged.
- **`OTScene`** (`otscene.cpp:367`) — already indexes `m_levels[zoomIndex]`
  and derives the scale from `LevelInfo::getSize()`. The split is mechanical.
- **`PKETiledScene`** (`pketiledscene.cpp:181`) — level index and directory
  index differ; `m_zoomDirectoryIndices[level]` maps between them, and the
  composer's `userData` is already the level index.
- **`EtsFileScene`** (`etsfilescene.cpp:151`) — already validates the level
  index and passes it in `TileComposerUserData`. The split is mechanical.

Each override derives its level geometry from `LevelInfo::getScale()` where
the surrounding code allows, so that the scale used to convert on the old path
is provably the scale reported by `get_zoom_level_info`.

### 5.3 Public C++ API

Non-virtual on `CVScene`, mirroring the existing locking wrappers
(`cvscene.cpp:42-48` and `73-160`):

```cpp
void readResampledLevelBlockChannels(int level, const cv::Rect& levelRect,
    const cv::Size& blockSize, const std::vector<int>& channelIndices,
    cv::OutputArray output);

void readResampledLevel4DBlockChannels(int level, const cv::Rect& levelRect,
    const cv::Size& blockSize, const std::vector<int>& channelIndices,
    const cv::Range& zSliceRange, const cv::Range& timeFrameRange,
    cv::OutputArray output);
```

The first takes `m_readBlockMutex` and calls the virtual with
`zSliceIndex = tFrameIndex = 0`. The second reuses the slice/frame assembly
loop of `readResampled4DBlockChannels` verbatim; that loop is factored into a
private helper templated on the per-plane read so the two share it rather than
duplicating forty lines of `cv::Range` bookkeeping.

On `slideio::Scene`, the buffer-based public API:

```cpp
void readResampledLevelBlockChannels(int level,
    const std::tuple<int,int,int,int>& levelRect,
    const std::tuple<int,int>& blockSize,
    const std::vector<int>& channelIndices,
    void* buffer, size_t bufferSize);

void readResampledLevel4DBlockChannels(int level,
    const std::tuple<int,int,int,int>& levelRect,
    const std::tuple<int,int>& blockSize,
    const std::vector<int>& channelIndices,
    const std::tuple<int,int>& zSliceRange,
    const std::tuple<int,int>& timeFrameRange,
    void* buffer, size_t bufferSize);
```

Two methods rather than a full eight-way mirror of the existing read family.
`channelIndices = {}` covers the all-channels case and
`blockSize = levelRect.size()` covers the no-resampling case, so the simpler
overloads would carry no information. `getBlockSize()` already computes the
required buffer size and needs no level-aware variant, since it depends only on
the output block size and the channel selection.

### 5.4 Python API

```python
scene.read_block_from_level(level, rect=(0, 0, 0, 0), size=(0, 0),
                            channel_indices=None, slices=(0, 1), frames=(0, 1))
```

Semantics, chosen to echo `read_block` wherever the meaning is genuinely the
same and to differ visibly where it is not:

- `level` is required and positional. There is no default, because a defaulted
  level is the ambiguity this method exists to remove.
- `rect` is **always** in `level`'s coordinates. A width or height of `0`
  extends to the **level's** edge. This is the one place the existing
  `PyScene::adjustSourceRect` (`pyscene.cpp:187-200`) cannot be reused as
  written: it clamps against `m_scene->getRect()`, the scene rectangle. It gets
  a bounds parameter so both call sites share it.
- `size=(0, 0)` returns native level pixels with no resampling. A single
  non-zero component preserves aspect ratio, as `adjustTargetSize`
  (`pyscene.cpp:202-229`) already does; that function is reused unchanged.
- `channel_indices`, `slices` and `frames` behave exactly as in `read_block`.
- The returned array's shape follows the same rules as `read_block`:
  `(height, width)`, with a channel axis when more than one channel is
  requested and slice/frame axes prepended when those ranges span more than
  one.
- The dtype-homogeneity check currently inlined in `PyScene::readBlock`
  (`pyscene.cpp:128-144`) is factored into a private helper and called from
  both methods; a heterogeneous channel selection raises the same error.
- The GIL is released around the read.

Files touched: `src/pyscene.hpp`, `src/pyscene.cpp`, `src/pybind.cpp`,
`slideio/wrappers/py_slideio.py`. `slideio/core/__init__.py` and
`slideio/__init__.py` need no change: `Scene` is already re-exported and this
adds a method to it, not a new type.

### 5.5 The two missing `LevelInfo` bindings

```python
level_info.tile_count            # LevelInfo::getTileCount()
level_info.get_tile_rect(index)  # LevelInfo::getTileRect(int) -> Rectangle
```

`Rectangle` is already bound (`pybind.cpp:137-141`). With these, the loop the
issue asks for is direct, and no caller has to reimplement the tile grid:

```python
info = scene.get_zoom_level_info(level)
for i in range(info.tile_count):
    r = info.get_tile_rect(i)
    tile = scene.read_block_from_level(level, (r.x, r.y, r.width, r.height))
```

Note that `getTileRect` returns full-size rectangles that overhang the level at
the right and bottom edges. Under contract point 4 that is exactly right: the
overhang comes back as background and every tile in the grid has the same
shape, which is what a tile cache wants.

### 5.6 Level tables for GDAL and PKESmallScene

Both classes build their state in their constructors — `gdalscene.cpp:15-21`
and `pkesmallscene.cpp:14-22` — rather than in an `init()`. Each gains a
single-level table there:

```cpp
m_levels.resize(1);
LevelInfo& level = m_levels[0];
level.setLevel(0);
level.setScale(1.0);
level.setSize({rect.width, rect.height});
level.setTileSize({rect.width, rect.height});
level.setMagnification(getMagnification());
```

This matches what `ZVIScene` (`zviscene.cpp:359-364`), `DCMScene`
(`dcmscene.cpp:190-194`), `VsiFileScene` (`vsifilescene.cpp:75-79`) and
`SVSSmallScene` (`svssmallscene.cpp:46-52`) already do. It removes a
zero-levels special case from the new API and, independently of this feature,
makes `num_zoom_levels` meaningful for every format the library reads.

## 6. Worked example

The code from the issue, before and after. Before:

```python
info = scene.get_zoom_level_info(level)
tile_w, tile_h = info.tile_size.width, info.tile_size.height
ds = 1.0 / info.scale
x0 = int(round(col * tile_w * ds))          # rounding #1
y0 = int(round(row * tile_h * ds))
w0 = int(round(tile_w * ds))                # rounding #2
h0 = int(round(tile_h * ds))
tile = scene.read_block(rect=(x0, y0, w0, h0), size=(tile_w, tile_h))
#                                    ^ slideio now converts back, rounding again,
#                                      and re-derives the level from the ratio
```

After:

```python
info = scene.get_zoom_level_info(level)
tw, th = info.tile_size.width, info.tile_size.height
tile = scene.read_block_from_level(level, (col * tw, row * th, tw, th))
```

No rounding, one level, and `size` omitted means the tile is returned at native
level resolution with no resampling pass at all. A viewer that walks the whole
grid instead of a single `(col, row)` can iterate `get_tile_rect` as in §5.5.

## 7. Testing

**The regression argument comes first.** §5.2 is an extraction: the old entry
point computes the same `level` and `levelRect` it computes today and passes
them on. So every existing test of `read_block`, across all suites, must pass
unchanged and without tolerance adjustments. If any existing test needs its
expected values touched, the extraction was not faithful and the change stops
there.

New tests:

- **Geometric alignment**, the reported defect. Read the same physical region
  from level *N* and level *N+1* through `read_block_from_level`, downsample the
  finer result to the coarser size, and assert the two agree within resampling
  tolerance with no integer offset. Written so that it fails against the
  current `read_block` path and passes against the new one, so it documents
  the bug as well as the fix.
- **Tile-grid coverage.** For each level, read every `get_tile_rect` tile,
  stitch them, and compare against a single full-level read. Catches any
  off-by-one in the level rectangle interpretation.
- **Edge tiles.** A rectangle overhanging the right and bottom edges returns an
  array of the requested size whose in-bounds part matches a clamped read and
  whose remainder is background.
- **Equivalence with the implicit path.** `read_block_from_level(L, full level
  rect)` against `read_block(scene rect, size=level size)`, within resampling
  tolerance. Confirms the two entry points agree about what a level contains.
- **No level escalation.** Read with `blockSize` slightly smaller than
  `levelRect` and confirm the result matches a same-level resample rather than
  a finer-level one — the distinction is visible in the interpolation of
  high-frequency detail near tile seams.
- **Base-class fallback.** The same alignment and edge tests against a
  single-level scene (GDAL, ZVI) to exercise the default implementation.
- **Errors.** `level = -1`, `level = num_zoom_levels`, and a non-zero slice or
  frame index on a driver that does not support them.
- **Bindings.** `tile_count` and `get_tile_rect` agree with the C++ values and
  with a hand-computed tile grid.

Gate: `slideio_tests`, `slideio_ndpi_tests`, `slideio_vsi_tests`,
`slideio_pke_tests`, `slideio_ometiff_tests`, `slideio_phtiff_tests`,
`slideio_converter_tests`, `slideio_transformer_tests`. The last two because
the converter and transformer both read through `CVScene`, and the 4D helper
of §5.3 refactors a loop they depend on.

## 8. Compatibility

Source-compatible and purely additive at the API level. No existing signature,
default argument or documented behaviour changes.

**Not ABI-compatible.** Adding a virtual to `CVScene` changes the vtable
layout. Any out-of-tree driver or application linked against 2.8.x must be
recompiled against 2.9.0. This is acceptable for the minor version bump the
issue is milestoned to, but it belongs in `software-docs/BREAKING_CHANGES.md`
and in the release notes.

The Python wheels must be rebuilt against slideio 2.9.0. Per
`slideio-python/CLAUDE.md`, the version appears in `conanfile.txt`,
`build-dependencies.ps1` and `conan.sh`, and all three must move together.

The public documentation on the Jekyll site (`docs/`) needs a section on the
new method and the level coordinate system; that is a separate commit from the
code.

## 9. Isolation and boundaries

The new virtual is the only widening of the driver interface, and it has a
working default, so a driver author can ignore it. Each driver override is
self-contained: it takes a level index and a level-space rectangle and reads,
with no knowledge of how the level was chosen. Level *selection* stays where it
is today, in the old entry point, called by exactly one caller per driver.

This is a net simplification of the drivers rather than an addition. Each one
currently interleaves "which level" with "read from it" in a single function;
afterwards the two are separately readable and separately testable, and the
second is directly reachable from a test.

## 10. Out of scope, and why

**The read mutex.** `CVScene::readResampledBlockChannels` takes
`m_readBlockMutex` (`cvscene.cpp:46`), and `readResampled4DBlockChannels` takes
it per plane (`cvscene.cpp:149`). Every block read of a scene is therefore
serialised. A tiled viewer fetching tiles from a thread pool — the exact
workload issue #69 describes — gets no concurrency from slideio at all, and
this will very likely dominate whatever throughput the level API saves. It
deserves its own issue and its own thread-safety audit of the driver state each
`readTile` touches; folding it into this change would make an extraction that
is provably behaviour-preserving into one that is not.

**Tile caching.** Correctly the viewer's job, not the library's. The library's
part is making a cache key cheap to satisfy, which is what this change does.

**`read_tile(level, col, row)`.** `get_tile_rect` plus `read_block_from_level`
covers it without introducing a second coordinate convention, and a tile-index
API would have to take a position on levels whose tile size is the whole level.

## 11. Risks

| Risk | Handling |
|---|---|
| The extraction changes behaviour on the existing path | The existing suite is the check, and it must pass with no expected-value edits (§7). Any edit needed means the extraction was wrong. |
| SCN's per-channel directory lists are not index-aligned | The override indexes each channel's own list and tolerates a short list by resolving that channel to `nullptr`, the same outcome the current zoom search produces. Covered by the tile-grid test on a multi-channel SCN file. |
| NDPI `SingleStripe` and GDAL crop with an unguarded `cv::Mat(rect)` | Both clamp before cropping and pad afterwards, per contract point 4. Covered by the edge-tile test on both drivers. |
| A driver override diverges from the scale reported by `get_zoom_level_info` | Overrides derive geometry from `LevelInfo::getScale()` where the surrounding code allows; the equivalence test compares the two entry points level by level. |
| ABI break surprises a downstream consumer | Documented in `BREAKING_CHANGES.md` and the release notes; the version bump to 2.9.0 signals it. |
| The new API disappoints on throughput because of the read mutex | Named explicitly in §10 and in the issue reply, so the reporter is not left to discover it. |
| The 4D helper refactor (§5.3) touches a loop the converter depends on | `slideio_converter_tests` and `slideio_transformer_tests` are in the gate. |
