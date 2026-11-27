# GDAL driver: pyramidal TIFFs and concurrent reads

Status: draft, 2026-11-27

## Intent

The GDAL driver is what opens a `.tif` that no vendor driver claims. Today it
has two shortcomings, and both matter for large files:

1. **Every read is serialised.** All scenes of a file share one mutex
   (`gdalslide.cpp`), because every page decodes through the one libtiff
   handle owned by `SmallTiffWrapper`. A tiled TIFF read from N threads runs at
   the speed of one.
2. **Pyramids are invisible.** Every top-level IFD is its own scene with a
   single level, and SubIFDs are never surfaced at all. A pyramidal TIFF
   written by libvips, GDAL (COG), bfconvert and the like opens as a pile of
   unrelated scenes, and a downscaled read of the base decodes full-resolution
   tiles when a reduced level sits in the same file.

The goal is for such a file to open as one scene with a zoom pyramid that reads
concurrently, the way an SVS does, without introducing a new driver.

## Goals

- A generic TIFF whose pages form a pyramid opens as one scene per image, with
  the pyramid exposed as `LevelInfo` levels and addressable by
  `readResampledLevelBlockChannelsEx`.
- Reads of TIFF scenes run concurrently: `supportsConcurrentReads()` is `true`
  and no mutex is shared between scenes or reads.
- FreeImage-read scenes (PNG, JPEG, BMP, GIF, JP2...) also report concurrent
  reads, since their read path is already a read-only copy out of a bitmap
  decoded at open.
- Level 0 of every file that opens today reads bit-identically to today.

## Non-goals

- A new driver, a new driver id, or a new library. The driver stays `GDAL`.
- Strip-by-strip reading. A striped level is still decoded whole per block, as
  today (see Constraints).
- Size-based guessing of levels. Only pages the file itself marks as reduced
  resolution, or that sit in SubIFDs, become levels.
- Exposing masks, labels or thumbnails as anything. See grouping rules.
- Changing anything about which driver `findDriver` picks; GDAL stays last.

## Anti-goals

- Merging pages that are genuinely different images into one scene. A wrong
  merge loses data silently; a missed merge only costs a scene. When in doubt
  the grouping keeps a page as its own scene.
- Reintroducing a handle whose lifetime is tied to a thread (`thread_local`) --
  see the concurrency contract in `CLAUDE.md`.

## Constraints

- `CVScene` concurrency contract: per-read state goes through `ContextPool`;
  the pool is declared last among the members its in-flight reads touch.
- `ImageTools::openSmallImage` picks `SmallTiffWrapper` for TIFFs and
  `FIWrapper` for everything else; FreeImage never reads a TIFF.
- `TiffTools::scanFile` already scans top-level IFDs and their SubIFDs
  (`TiffDirectory::subdirectories`, each carrying its own `offset`), and
  `subFileType`. `TiffTools::setCurrentDirectory` handles a SubIFD by offset.
- Every piece of per-page metadata the TIFF path reports today
  (`SmallTiffPage`: size, type, channels, compression, resolution, ICC profile,
  datetime, the metadata JSON) is derived from `TiffDirectory` alone. No handle
  is needed after the scan.
- The corpus (`images/gdal/*.tif`) has no SubIFDs and no reduced-resolution
  pages; all six TIFFs are striped with `subFileType` 0. Existing tests are
  therefore unaffected by grouping. It also means the corpus cannot test the
  new behaviour.

## Approaches considered

| Approach | Trade-off |
|---|---|
| **New `TIFF` driver** before GDAL in `findDriver` | Honest about the change in meaning, but a 16th library (packaging, `NAME_TOOL_LIB_LIST`, smoke test, CI lists), a new id for files that report `GDAL` today, and two drivers claiming the same extension. |
| **Concurrency only in `GDALScene`**, no pyramids | Smallest change; leaves pyramidal TIFFs as piles of scenes. |
| **Split the classes inside the GDAL driver** (chosen) | `GDAL` keeps its id and library; a new `GDALTiffScene` owns the TIFF path with pyramids and a `ContextPool`, and `GDALScene` keeps the FreeImage path. Same pattern the svs library uses for PHTIFF. The cost is a behaviour change -- fewer scenes for pyramidal files -- recorded as breaking. |

Chosen by the user: split the classes, keep the driver.

## Grouping rules (decided: option B)

Applied to the top-level IFDs in file order, by `GDALSlide` for a TIFF:

1. A top-level IFD with `subFileType & 4` (transparency mask) is skipped: it
   is not a scene and not a level.
2. A top-level IFD with `(subFileType & 1) == 0` starts a new scene; it is
   that scene's level 0 (the *base*).
3. The base's candidate levels are its SubIFDs, plus every following top-level
   IFD with `subFileType & 1` up to the next IFD that starts a scene.
4. A candidate is accepted as a level only if all of these hold:
   - it has the same `channels`, `dataType` and `bitsPerSample` as the base;
   - it is strictly smaller than the base in both width and height;
   - it keeps the base's aspect ratio: its height is within
     `max(1, ceil(baseHeight / baseWidth))` pixels of
     `baseHeight * width / baseWidth` (rounded), and its width within
     `max(1, ceil(baseWidth / baseHeight))` pixels of
     `baseWidth * height / baseHeight` (rounded). Writers round each level's
     size, so the check cannot be exact, and one pixel of rounding in one
     dimension moves the prediction of the other by the base's ratio -- a fixed
     tolerance of 1 rejected 98x33 as a level of 100001x33000, whose predicted
     width is 100. A thumbnail or label with its own proportions still fails it.
     *(Amended by the PR review.)*
     *(Design call, from the builder check: without it a thumbnail flagged
     reduced-resolution would be read as a level and return wrong pixels.)*
   - it is not a mask (`subFileType & 4`). This applies to SubIFDs too, not only
     top-level IFDs.
5. Accepted levels are ordered by width, descending. A level whose size equals
   one already accepted is dropped. Candidates are considered SubIFDs first,
   then top-level IFDs in file order, so on a tie the SubIFD is kept.
   *(Design call.)*
6. A top-level reduced-resolution IFD that fails rule 4, or that comes before
   any base, becomes a scene of its own (single level). It does **not** end the
   base's run: in `A, r1, x, r2`, where x fails rule 4, r1 and r2 are both
   levels of A. Scenes are ordered by the file position of their first IFD, so
   x's scene comes right after A's. *(Design call.)* A SubIFD that fails rule 4
   is ignored, which is exactly how SubIFDs are treated today.
7. If skipping masks would leave no scenes at all (a file of nothing but mask
   pages), the masks are kept as scenes, one level each. A file that opens
   today keeps opening. *(Design call, which follows from the level-0 goal.)*

A file with no SubIFDs and no reduced pages therefore opens exactly as today:
one scene per top-level IFD (minus masks), one level each.

## Design

### Classes

- **`GDALSlide`** branches on the file type. For a TIFF it scans with
  `TiffTools::scanFile`, applies the grouping rules and builds one
  `GDALTiffScene` per group. Otherwise it opens `FIWrapper` as today and builds
  one `GDALScene` per page. The shared `readMutex` goes away. For a TIFF it
  also creates the file's one `std::shared_ptr<ContextPool>` and hands it to
  every `GDALTiffScene` it builds.
- **`GDALTiffScene`** (new; `CVScene` + `Tiler`). Holds the base and level
  `TiffDirectory` copies, the file path, metadata derived from the base, and a
  `std::shared_ptr` to the file's `ContextPool`, declared last. It does not hold
  a `SmallImage` or any open handle outside the pool. *Amended by the final
  review:* the pool is per file, shared by all its scenes, not per scene. A pool
  per scene made open handles scale with the page count -- a plain 1000-page
  TIFF read page by page would hold up to one handle per page, a regression from
  the one handle per file the old driver kept. A shared pool bounds the file at
  `ContextPool::defaultMax()` handles. Each read still borrows once. Because the
  pool is shared, a scene being destroyed blocks in `~ContextPool` only if it
  holds the last reference; otherwise dropping it returns at once, and borrows
  still out belong to reads of other scenes, whose directories are alive.
- **`GDALScene`** keeps only the FreeImage path: the tiled-TIFF branch,
  `m_tiff`, `m_tiledDirectory`, the `Tiler` methods and the shared mutex leave
  it. It overrides `supportsConcurrentReads()` to `true`.
- **`GDALReadContext : ReadContext`** owns one `TIFFKeeper` opened from the
  path, as `SVSReadContext` does. The pool factory captures the path by value.
  A context is not tied to a scene -- every read sets its own directory -- so
  the file's scenes can share one pool of them.

### Read path of `GDALTiffScene`

- `readResampledBlockChannelsEx` picks the level with `Tools::findZoomLevel`
  (as `SVSTiledScene` does), scales the rect into that level and calls the
  level read.
- `readResampledLevelBlockChannelsEx` borrows one context for the whole call
  and passes `{directory, context}` as `userData`; nothing below re-acquires.
  - A tiled level goes through `TileComposer::composeRect`. `readTile` keeps
    the current GDAL behaviour: edge tiles cropped to the valid area, a tile
    that decodes to the wrong size or type fails the read (no zero-fill, unlike
    SVS).
  - A striped level is decoded whole with `TiffTools::readStripedDir` through
    the borrowed handle, then cropped and resized as `readWholePageBlock` does
    today. *Amended by the PR review:* that decode, crop and resize run under a
    per-file `std::mutex`, which `GDALSlide` creates next to the shared
    `ContextPool` and passes to every `GDALTiffScene` as a `shared_ptr`. Without
    it, concurrent reads could hold up to `ContextPool::defaultMax()` whole
    decoded levels at once. The lock is taken before the borrow, so a read
    waiting for it holds no handle. Tiled levels never take it, and
    `supportsConcurrentReads()` stays `true`: the mutex bounds memory, it does
    not guard shared state. It goes once striped levels are read strip by strip
    (`TECH_DEBT.md` §31).
- Level 0 of a single-level scene must give the same pixels as today for both
  the tiled and striped case.

### Scene properties

- Levels: `LevelInfo` per level, scale = level width / base width, tile size
  = the directory's tiles, or the whole level for a striped one.
  Magnification stays 0.
- Size, channels, data type, compression, resolution, ICC profile,
  acquisition time and raw metadata all come from the base directory and must
  equal what `SmallTiffPage` reports for that directory today. The metadata
  JSON is built by the same code `SmallTiffPage::extractMetadata` uses -- move
  that into a shared function rather than copying it.
- `GDALSlide` raw metadata stays as today: the single scene's metadata when
  there is one scene (now counted in scenes, not pages), none otherwise.

## Testing

- **New corpus-free suite** in `slideio_tests`, named `GDALTiffPyramid.*`. The
  tests write their fixtures at run time into a temporary directory with raw
  libtiff calls -- not through `TiffTools`' write path, so a mistake shared by
  writer and reader cannot cancel itself out. The fixtures:
  - SubIFD pyramid (3 levels, tiled);
  - reduced-IFD pyramid (COG layout, tiled) with a mask page;
  - two independent pyramids in one file;
  - a reduced page with a different channel count (must stay its own scene);
  - a striped base with tiled reduced levels;
  - a plain multi-page file (scene count unchanged).
  Each checks scene count, level count and sizes, and that a level read
  matches a reference decoded independently.
- **Concurrency**: N threads reading random blocks at random levels, compared
  with the same reads done serially. Both TIFF and FreeImage scenes.
- **Grouping unit tests** run the grouping function on hand-built
  `std::vector<TiffDirectory>` values. They cover every rule above, including
  the aspect-ratio tolerance, a masked SubIFD, the `A, r1, x, r2` ordering, a
  size tie, and a file of nothing but masks.
- **Existing `GDALDriver.*` corpus tests** pass, with two deliberate
  exceptions:
  - `ConcurrencyContract.gdalIsStillSerialised` flips to asserting `true`, and
    is renamed to match;
  - the comment in `GDALDriver.twoPagesOfOneTiffReadConcurrently` that
    describes the shared mutex is rewritten.
- CI: add `GDALTiffPyramid.*` to the corpus-free filter in
  `build-validation.yml` and `release.yml`.

## Documentation

- `BREAKING_CHANGES.md`: a TIFF with SubIFDs or reduced-resolution pages opens
  as fewer scenes; mask pages no longer appear as scenes.
- `CHANGELOG.md`: the pyramid support and concurrent reads.
- `CLAUDE.md`: add GDAL to the "Concurrent today" list.
- `TECH_DEBT.md`: striped levels are decoded whole per block.

## Left to the builder

- Naming of files and helpers, and where the grouping function lives (a free
  function in the gdal driver is fine; it should be testable on a
  `std::vector<TiffDirectory>` without a file).
- Whether `GDALSlide` sniffs the format itself or keeps going through
  `ImageTools::isTiff`.
- The exact bound of the context pool (default is fine).
