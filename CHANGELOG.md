# Changelog

Notable changes to the SlideIO C++ library. Exported-API changes that break
out-of-tree callers are described in full in
[software-docs/BREAKING_CHANGES.md](software-docs/BREAKING_CHANGES.md); the entries
marked **Breaking** below summarise them.

## [2.11.0] - 2026-11-14

### Added

- **Four new drivers**, each detected automatically, each reporting concurrent reads:
  - **MRXS** (3DHISTECH MIRAX): stitched rendering from camera positions, scene
    metadata and associated images, decoded-image and file-handle caches, bounded
    resampling of large regions. A bare `Slidedat.ini` path opens too.
  - **BIF** (Roche Ventana): one scene per area of interest, per-boundary stitching
    geometry, stitched level reads with per-tile decode, iScan and EncodeInfo XML
    metadata.
  - **LIF** (Leica): container header, XML and memory-block index, stride-based
    layout, plane timestamps, magnification, and a `Color` channel attribute derived
    from single-hue LUT names.
  - **ND2** (Nikon NIS-Elements): chunked files (Ver2.x and Ver3.x) and legacy
    JPEG 2000 files, including 9- to 15-bit codestreams; experiment loops, plane
    timestamps, T fitted to the frames present, magnification from the objective
    name, and recovery of a file whose chunk map trailer is missing.
- **Computed scenes**: `makeScene` builds a `Scene` from a caller-implemented
  `ISceneSource` and a `SceneMetadata` declaration, validated at construction. A
  source with a real pyramid serves level reads directly, and such a scene can be
  converted like any other. New public headers `slideio/slideio/scenesource.hpp`
  and `slideio/slideio/scenemetadata.hpp`.
- `slideio/core/channelattributes.hpp` names the standard channel attributes
  (`ChannelAttribute::Color`, `Name`, `EmissionWavelength`, `ExcitationWavelength`);
  the drivers and the converter use them instead of literal keys.
- `CVScene::getChannelBackground(channel)`: the per-channel value of pixels a scene
  has no data for. The default keeps the previous rule (255 for byte channels, 0
  otherwise).
- A shared bounded-memory area downscale for reads far below native size, used by
  LIF and ND2. It matches `cv::resize(INTER_AREA)` bit for bit when a source row
  fits in the budget on x86; on arm64 a non-integer ratio may differ by 1.
- JPEG 2000 *striped* TIFF directories are decoded, not only tiled ones (for
  example a VSI "FocusMap").
- In-memory JPEG, PNG and BMP stream decoding in `slideio-imagetools`, and reading
  of the `XMLPacket` and `ImageDepth` TIFF tags.

### Changed

- **Breaking:** the scenes of a multi-file VSI slide are ordered by stack id, then
  path, rather than by file-system listing order, so the order is the same on every
  platform. A scene may sit at a different index than before; select scenes by
  `Scene::getName()` where it matters.
- VSI volumes whose metadata names no C dimension infer it from their tiles, and ETS
  files are paired with volumes by stack id (#50).
- NDPI files share one striped-raster cache budget across all open files, and
  concurrent reads of an NDPI scene no longer serialise on its decode.
- Scenes of the NDPI, CZI and GDAL drivers stay readable after their slide is
  released.

### Fixed

- **Breaking:** a block read of channels with different data types raises
  `slideio::RuntimeError` instead of returning a block typed by the first channel
  (#90).
- **Breaking:** a negative block size raises `slideio::RuntimeError` from every read
  path, before any allocation (#88).
- **Breaking:** an invalid scene index raises `slideio::RuntimeError` from every
  driver, and a negative index no longer reads out of bounds (#87).
- **Breaking:** `Transformation` has a virtual destructor, and a `Transformation`
  subclass defined outside SlideIO is rejected instead of being misread (#89). Code
  compiled against an earlier `transformation.hpp` must be rebuilt.
- **Breaking:** converting channels of mixed data types to OME-TIFF raises
  `slideio::RuntimeError` instead of writing a file that contradicts its own OME-XML.
- A block read whose rectangle crosses the scene edge fills the overhang with
  background in every driver (#45). GDAL, single-frame DCM, striped SVS, PKE and VSI
  scenes used to fail with an OpenCV assertion, and small scenes stretched the
  clipped part over the whole block.
- The converter no longer writes the untransformed pixels of a transformed scene: a
  transform is shared across reader threads instead of being re-opened by path.
- Indexed (palette) images decoded through FreeImage are expanded.
- `scaleRect` computes the far edge in 64-bit, avoiding overflow on very large
  slides.
- ND2: `getPlaneTimestamp` returns 0 for an out-of-range index (#98).

### Build and packaging

- The macOS release package is built on the `macos-26` runner (still with a
  macOS 12 deployment target); `macos-14` is retired.
- The release job also runs the `TiledAreaDownscale` unit tests on every platform.

[2.11.0]: https://github.com/Booritas/slideio/compare/v2.10.0...v2.11.0
