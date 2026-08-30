# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

SlideIO is a C++ library (with Python bindings in a separate repo) for reading medical/microscopy slide images. It supports 12 formats (SVS, AFI, SCN, CZI, ZVI, NDPI, VSI, DCM, QPTIFF, OME-TIFF, PHTIFF, GDAL) through a pluggable driver architecture. Cross-platform: Linux, macOS, Windows.

The 12 formats are served by 11 driver libraries: PHTIFF (Philips TIFF) has no library of its own — it ships inside the `slideio-svs` library, as `PHTIFFImageDriver`/`PHTIFFSlide`/`PHTIFFTiledScene` (`phtiff*.cpp` in `src/slideio/drivers/svs/`). `PHTIFFTiledScene` derives from `SVSTiledScene` to reuse the tiled-tiff reading path; everything format specific (detection, metadata parsing in `phtdescription.cpp`/`phtmetadata.cpp`, level-to-directory matching, tile padding crop) lives in the PHTIFF classes. Both driver ids are declared in `svsdriverids.hpp`.

## Build Commands

Prerequisites: Conan v2+, CMake 3.10+, C++17 compiler, Python 3.6+.

```bash
# Full build (conan + configure + build), release only:
python3 install.py -a install -c release

# Individual steps:
python3 install.py -a conan              # Install dependencies via Conan
python3 install.py -a configure-only     # CMake configure only
python3 install.py -a build-only         # Build only (after configure)
python3 install.py -a clean --clean      # Clean build directories

# Build specific config:
python3 install.py -a build -c debug     # Debug build
python3 install.py -a build -c release   # Release build
python3 install.py -a build -c all       # Both (default)

# Custom build/install directories:
python3 install.py -a install -bd /path/to/build -pr /path/to/install
```

Build output: `build/release/bin/` and `build/debug/bin/` on Linux/macOS; `build/` on Windows.

## Running Tests

Tests use Google Test. Test executables (release build):

```bash
./build/release/bin/slideio_tests              # Main test suite
./build/release/bin/slideio_converter_tests    # Converter tests
./build/release/bin/slideio_transformer_tests  # Transformer tests
./build/release/bin/slideio_ndpi_tests         # NDPI driver tests
./build/release/bin/slideio_vsi_tests          # VSI driver tests
./build/release/bin/slideio_pke_tests          # PKE driver tests
./build/release/bin/slideio_ometiff_tests      # OME-TIFF tests
./build/release/bin/slideio_phtiff_tests       # Philips TIFF (PHTIFF) tests

# Run a single test:
./build/release/bin/slideio_tests --gtest_filter="TestSuiteName.TestName"

# Run all tests matching a pattern:
./build/release/bin/slideio_tests --gtest_filter="*SVS*"
```

There are also single tests for memory/perf in `src/single_tests/`.

### Test images

Tests read from three roots, named by `SLIDEIO_TEST_DATA_PATH`,
`SLIDEIO_TEST_DATA_PRIV_PATH` and `SLIDEIO_IMAGES_PATH`. The corpus under the last
one is far larger than most machines hold, so it tends to be rotated in and out.

Set `SLIDEIO_SKIP_MISSING_IMAGES=1` and a test whose image is absent **skips**
instead of failing, naming the file it wanted. Leave it unset -- as CI must -- and a
missing image fails exactly as before, so coverage cannot quietly disappear. This
matters for reading a run at all: without it a rotated-out directory turns a suite
entirely red and a real regression is indistinguishable from an absent file.

`software-docs/TEST_IMAGES.md` lists every image the tests name, with its size and
how many tests would be lost by deleting it. Regenerate it with
`python3 auxfiles/list-test-images.py`.

## Architecture

### Module Hierarchy (each is a shared library, prefixed `slideio-`):

- **core** — Bottom layer. Fundamental types (Rect, Size, Range, Resolution, enums, exceptions, the logging seam) and the abstract base classes: `CVScene` (raster image), `CVSlide` (slide container), `ImageDriver`, `LevelInfo` (zoom pyramid)
- **imagetools** — Image I/O utilities: TIFF handling, JPEG2000 codec, FreeImage wrapper, color processing, tile management
- **slideio** (main) — Public API: `Slide`, `Scene`, `ImageDriverManager` (loads drivers dynamically)
- **converter** — Format conversion (mainly TIFF output), multithreaded encoding
- **transformer** — Image filters and color transformations (Gaussian, Canny, Sobel, etc.)
- **drivers** — 11 format-specific shared libraries, each implementing `ImageDriver`/`CVScene`/`CVSlide` (the svs library serves both SVS and PHTIFF)

### Driver Plugin Pattern

Each driver in `src/slideio/drivers/<format>/` is an independent shared library that:
1. Subclasses `ImageDriver` (registration/detection)
2. Subclasses `CVSlide` (file-level metadata, scene enumeration)
3. Subclasses `CVScene` (raster data access, zoom pyramid navigation)

### Key Design Patterns

- **Zoom pyramid support**: Slides contain multi-resolution levels accessed via `LevelInfo`
- **Multidimensional images**: 2D, 3D (Z-slices), and 4D (time-series) via CVScene
- **Block-based reading**: Efficient region extraction with arbitrary scaling
- **Level-addressed reading**: `CVScene::readResampledLevelBlockChannelsEx` reads a rect given in the coordinates of a named zoom level, bypassing level selection. Pyramid drivers override it; the base class has a working default. Public API: `Scene::readResampledLevelBlockChannels` / `readResampledLevel4DBlockChannels`, exposed to Python as `read_block_from_level`
- **Library naming**: `slideio-<module>` with `_d` suffix for debug builds

### Source Layout

```
src/
├── slideio/
│   ├── core/           # slideio-core
│   ├── slideio/        # slideio (main API)
│   ├── imagetools/     # slideio-imagetools
│   ├── converter/      # slideio-converter
│   ├── transformer/    # slideio-transformer
│   └── drivers/        # 11 driver libraries, serving 12 formats
│       ├── svs/  afi/  scn/  czi/  zvi/     # svs/ also serves PHTIFF
│       ├── ndpi/ vsi/  dcm/  pke/
│       └── ome-tiff/  gdal/
├── tests/              # Test suites (one per module/driver)
├── single_tests/       # Memory leak and performance tests
└── tools/              # CLI tools
```

## Documentation

- `docs/` is the published Jekyll site (GitHub Pages); release announcements live in `docs/_posts/`. Anything added here is public.
- `docs-src/` holds the sources of the published API reference: `Sphinx/source/` (Python API, `*.rst`) and `Doxygen/` (C++ API). The Sphinx pages are also public.
- `software-docs/` holds internal engineering documentation: `specs/` (designs), `plans/` (implementation plans), `TECH_DEBT.md`, `BREAKING_CHANGES.md`, `review.md`. Internal docs go here, never under `docs/`.
- Record exported-API changes that break out-of-tree callers in `software-docs/BREAKING_CHANGES.md`, grouped by branch. There is no changelog in the repository.

## Dependencies (managed via Conan)

spdlog, SQLite3, OpenCV, ZLIB, tinyxml2, ICU, libtiff, libjpeg, WebP, OpenJPEG, Iconv, nlohmann_json

Every one of them resolves from **conan center**. There is no private remote and
no conan-center-index fork to bootstrap: nothing has to be `conan create`d
before a build, `conan install -b missing` is all a fresh machine needs, and a
CI job or container needs no credentials. Anything that could not come from
conan center is a git submodule under `extern/` instead -- see below. Keep it
that way: a new dependency belongs on conan center, in `extern/`, or nowhere.

The JPEG XR codec is *not* a Conan package. It is the `extern/jpegxrcodec` git
submodule (github.com/Booritas/jpegxrcodec, pinned at v1.0.3), added to the build
with `add_subdirectory` from the root `CMakeLists.txt`. It builds a static
`jxrcodec` target that slideio-imagetools, slideio-czi and slideio-ndpi link
against; there is no `find_package(jpegxrcodec)` any more. A clone without
`--recurse-submodules` needs `git submodule update --init` before configuring, or
CMake stops with a FATAL_ERROR naming the empty directory. Plain `--init` is
enough: jpegxrcodec's own googletest submodule is only needed for its tests,
which the slideio build forces off.

pole, the OLE compound-file reader the zvi driver reads ZVI storages with, is
the same arrangement: the `extern/pole` submodule (github.com/Booritas/pole,
pinned just past v1.0.4) in place of `pole/1.0.4@slideio/stable`, added from the root
`CMakeLists.txt` because it needs nothing but the standard library. It builds a
static `pole` target that slideio-zvi and the main test suite link directly --
no `find_package(pole)`, no `pole::pole`. It spells its tests option
`PACKAGE_TESTS`, the same name jpegxrcodec uses, so the one cache entry the
root sets turns both off.

pole publishes no include directory of its own, and everything that consumes it
says `<pole/...>` while its headers sit in `includes/`. The root `CMakeLists.txt`
stages that prefix in the build tree -- `file(COPY)` of `includes/` into
`${CMAKE_BINARY_DIR}/extern/pole/include/pole` -- which is the layout the Conan
recipe produced by copying the same directory into the package as
`include/pole`. It also redirects the `pole` target's archive output: pole sets
`CMAKE_ARCHIVE_OUTPUT_DIRECTORY` to `${CMAKE_BINARY_DIR}/install/lib`, which in
this build tree is the directory `install.py` installs into.

The NDPI driver's two forks are also submodules rather than Conan packages:
`extern/ndpi-libjpeg-turbo` (github.com/Booritas/ndpi-libjpeg-turbo, v2.1.2) and
`extern/ndpi-tiff` (github.com/Booritas/ndpi-tiff, v4.3.0), replacing the
`ndpi-libjpeg-turbo/2.1.2@slideio/stable` and `ndpi-libtiff/4.3.0@slideio/stable`
packages. Unlike jpegxrcodec they are added from
`src/slideio/drivers/ndpi/CMakeLists.txt`, not the root: ndpi-tiff needs zlib,
libdeflate, xz_utils, jbig, zstd and libwebp, which are on `CMAKE_PREFIX_PATH`
only inside the directory where that driver's Conan files are generated, and the
ndpi driver is their only consumer.

The two have to move together. libtiff calls libjpeg, the driver calls it too
(`ndpitifftools.cpp` includes `jpeglib.h`), and the fork is built `WITH_JPEG8`
and `WITH_MEM_SRCDST` -- both change the size of `jpeg_decompress_struct`. Two
libjpeg builds that disagree show up at runtime as "JPEG parameter struct
mismatch", not as a link error. One in-tree build removes the whole class of
problem, and with it the `-b ndpi-libtiff/*` force-build `install.py` used to
need.

ndpi-tiff is a pristine submodule, so the `ndpi-libtiff` recipe's
`4.3.0-0001-cmake-dependencies.patch` cannot be applied to it. Shim find modules
in `cmake-scripts/ndpi-tiff-deps/` do the same job from outside, publishing the
imported-target names libtiff links (`Deflate::Deflate`, `JBIG::JBIG`,
`ZSTD::ZSTD`, `WebP::WebP`) from the ones Conan actually provides, and resolving
`find_package(JPEG)` to the in-tree `jpeg-static`. They are deliberately not
`GLOBAL`: the vsi, ome-tiff and phtiff modules have their own `JPEG::JPEG` from
the regular libjpeg.

Two things about that arrangement are easy to break. The ndpi-tiff subdirectory
sets `CMAKE_FIND_PACKAGE_PREFER_CONFIG OFF`, because the Conan toolchain turns
it on and config mode would match `jbig-config.cmake` for `find_package(JBIG)`
on a case-insensitive filesystem -- reporting success while creating
`jbig::jbig` instead of the `JBIG::JBIG` libtiff links. And the shims are handed
their include directories explicitly, derived from `<pkg>_PACKAGE_FOLDER_<CONFIG>`:
for libdeflate, jbig and zstd, Conan's `<pkg>_INCLUDE_DIRS_<CONFIG>` arrives
empty in this graph even though the libraries and link interface are intact, so
relying on it silently loses the headers and libtiff fails on `libdeflate.h`.

spdlog is a static library linked `PRIVATE` into `slideio-core` alone. That is a
link-time-singleton constraint, not an ordinary dependency: the logging
threshold and sink must exist in exactly one shared library.

Conan profiles are in `conan/<Platform>/` with variants per distro/arch.

## Build System Notes

- `install.py` auto-detects platform, architecture, and Linux distro to select the correct Conan profile
- CMake toolchain file: `./cmake/conan_toolchain.cmake` (generated by Conan)
- Windows uses Visual Studio 17 2022 generator; Linux/macOS use Unix Makefiles
- On CentOS/manylinux: adds `-D_GLIBCXX_USE_CXX11_ABI=0`
- Docker builds for manylinux wheels: see `docker/` directory and `docker-build-linux.sh`
