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

### Building the library from the source

Build instructions live with the code, where they are kept current:
see [Build instructions](https://github.com/Booritas/slideio#build-instructions)
in the SlideIO README for the prerequisites, the dependencies, and the
commands for Linux, macOS and Windows.

If you do not need to build from source, prebuilt packages for all three
platforms are on the [Downloads page]({{ site.baseurl }}/downloads.html).

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
