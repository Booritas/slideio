---
layout: page
title: C++ API
sidebar_link: true
sidebar_sort_order: 200
---

## Overview

SlideIO is a cross-platform C++ library. It is built and tested on Windows 10/11,
Ubuntu 22.04, macOS 14 on Apple Silicon, and a `manylinux_2_28` container. The
macOS build targets macOS 12 and above; the Linux packages need glibc 2.28 or
newer, and the Debian packages target Debian 12+ and Ubuntu 22.04+.

The library is reached through the global functions `slideio::openSlide()` and
`slideio::getDriverIDs()`, and provides two main classes: `slideio::Slide`, a
slide container, and `slideio::Scene`, a single raster image within it.

## Installation

### Installing a release package

Prebuilt packages for Windows, macOS and Linux are on the
[Downloads page]({{ site.baseurl }}/downloads.html). They are self-contained:
no Conan, no CMake toolchain file and no build of SlideIO is needed to use
one. A package carries the headers, the shared libraries, the CMake package
configuration, and the `slideio-converter` and `slideio-tiffinspector`
command line tools.

Whichever platform you are on, a program then finds the library with:

```cmake
find_package(slideio REQUIRED)
target_link_libraries(myapp PRIVATE slideio::slideio)
```

The package publishes the components `slideio`, `core`, `imagetools`,
`converter` and `transformer`; linking `slideio::slideio` is enough for most
uses.

#### Debian and Ubuntu

Download the two packages from the release page and install them together.
The leading `./` matters -- without it `apt` looks for a package of that name
in your configured repositories rather than installing the file:

```
sudo apt install ./libslideio<version>_<version>_amd64.deb \
                 ./libslideio-dev_<version>_amd64.deb
```

`libslideio<version>` is the runtime and `libslideio-dev` adds the headers and
the CMake configuration; install both to build against the library. The
version is part of the runtime package name so that two minor releases can be
installed side by side. The command line tools are a third package,
`slideio-tools`, installed the same way.

Everything lands under `/usr`, which CMake and the dynamic loader already
search, so nothing further is needed:

```
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
```

#### macOS

Unpack the archive. Its layout is flat -- `bin`, `lib` and `include` sit at
the root with no enclosing directory -- so the directory you unpack into is
the prefix:

```
mkdir -p ~/slideio && tar -xzf slideio-<version>-macos-arm64.tar.gz -C ~/slideio
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH=~/slideio
```

No `DYLD_LIBRARY_PATH` is required: the imported CMake target carries an
absolute location, and your executable gets an RPATH into the unpacked tree.
The tools in `bin` find the dylibs in `lib` the same way.

The archive is built for Apple Silicon against a macOS 12 deployment target.
It is not signed by a registered developer, so macOS may quarantine the
download; `xattr -d com.apple.quarantine <file>` clears it.

#### Windows

Unpack the zip. The layout is flat here too, so the directory you unpack into
is the prefix:

```
cmake -S . -B build -DCMAKE_PREFIX_PATH=C:/slideio
cmake --build build --config Release
```

Windows has no RPATH, so the DLLs are found on `PATH` at run time. Add the
package's `bin` directory to it, or copy the DLLs beside your executable:

```
set PATH=C:\slideio\bin;%PATH%
```

The separate `-pdb.zip` holds the matching MSVC debug symbols, should you need
to step into the library.

### Building the library from the source

Build instructions live with the code, where they are kept current:
see [Build instructions](https://github.com/Booritas/slideio#build-instructions)
in the SlideIO README for the prerequisites, the dependencies, and the
commands for Linux, macOS and Windows.

## C++ API

`slideio::openSlide()` opens a slide and returns an object of class
`slideio::Slide`. That class exposes methods for the slide's properties,
including its metadata and associated images. A single `slideio::Slide` can
hold several raster images, each represented by a `slideio::Scene`. For example,
a CZI file can contain several scanned regions, each of them a separate scene.
`slideio::Scene` exposes the methods for reading raster data and per-scene
metadata.

The API uses only standard C++ types, so a program that includes SlideIO does
not need OpenCV on its include path.

See the
[SlideIO C++ API reference]({{ site.baseurl }}/doxygen/html/), generated with
Doxygen, for the full documentation.

{% gist 83df5998e83a737661374aa3515a84d8 %}

## Used 3rd party libraries

From [conan center](https://conan.io/center):

- [OpenCV](https://opencv.org)
- [DCMTK](https://dicom.offis.de/dcmtk)
- [FreeImage](https://freeimage.sourceforge.io/)
- [libtiff](http://libtiff.org)
- [libjpeg](https://libjpeg.sourceforge.net/)
- [libpng](http://libpng.org)
- [OpenJPEG](https://www.openjpeg.org)
- [WebP](https://developers.google.com/speed/webp)
- [zlib](https://zlib.net)
- [Zstandard](https://facebook.github.io/zstd/)
- [SQLite](https://sqlite.org)
- [tinyxml2](https://github.com/leethomason/tinyxml2)
- [ICU](https://icu.unicode.org/)
- [libiconv](https://www.gnu.org/software/libiconv/)
- [Expat](https://libexpat.github.io/)
- [nlohmann/json](https://github.com/nlohmann/json)
- [spdlog](https://github.com/gabime/spdlog)
- [Little CMS](https://littlecms.com/)
- [CLI11](https://github.com/CLIUtils/CLI11)

As git submodules, because they are not on conan center:

- [JPEG XR codec](https://github.com/Booritas/jpegxrcodec)
- [pole](https://github.com/Booritas/pole), an OLE compound file reader, used by
  the ZVI driver
- [ndpi-libjpeg-turbo](https://github.com/Booritas/ndpi-libjpeg-turbo) and
  [ndpi-tiff](https://github.com/Booritas/ndpi-tiff), forks the NDPI driver
  needs
