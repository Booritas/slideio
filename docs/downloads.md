---
layout: page
title: Downloads
sidebar_link: true
sidebar_sort_order: 275
---

## Overview

SlideIO comes in three pieces, each released separately. The Python module is
what most users want; the C++ library is for building SlideIO into your own
application; the viewer is a desktop program that needs no programming at all.

Every link below points at the current release, so it stays correct as new
versions appear. All three are free and open source under the BSD 3-clause
license.

## SlideIO for Python

The Python module is installed from PyPI:

```
pip install slideio
```

Wheels are published for **Python 3.9 through 3.14** on Windows x86-64, macOS
(Apple Silicon and Intel) and Linux x86-64 (`manylinux_2_28`). The wheel carries
the library inside it, so no compiler, no Conan and no separate SlideIO
installation is needed.

- [slideio on PyPI](https://pypi.org/project/slideio/)
- [Release notes and wheels on GitHub](https://github.com/Booritas/slideio-python/releases/latest)
- [Python API documentation]({{ site.baseurl }}/python.html)

## SlideIO library and Tools

Prebuilt packages of the C++ library — headers, shared libraries, and the
`slideio-converter` and `slideio-tiffinspector` command line tools. The tools
need no programming and no Python; see the
[Tools page]({{ site.baseurl }}/tools.html) for what they do and how to run
them.

[Download the SlideIO library](https://github.com/Booritas/slideio/releases/latest)

| Platform | Package |
|---|---|
| Windows x86-64 | `.zip` with headers, import libraries and DLLs, built with MSVC 2022. A matching `-pdb.zip` carries the debug symbols. |
| macOS (Apple Silicon) | `.tar.gz` with headers and dylibs, built against a macOS 12 deployment target. |
| Linux x86-64 | `.tar.gz` built in a `manylinux_2_28` image, so it runs anywhere with glibc 2.28 or newer. |
| Debian 12+ / Ubuntu 22.04+ | Three packages: `libslideio<version>` (the runtime, named so that two minor releases can be installed side by side), `libslideio-dev` (headers and the CMake package config), and `slideio-tools` (the two command line tools). Install the first two for development. |

To build from source instead, see the [C++ API page]({{ site.baseurl }}/cpp.html).

## SlideIO Viewer

A desktop application for opening and navigating whole-slide images, with no
code to write. See the [Viewer page]({{ site.baseurl }}/viewer.html) for a
screenshot, the feature list and the keyboard shortcuts.

[Download SlideIO Viewer](https://github.com/Booritas/slideio-view/releases/latest)

| Platform | Package |
|---|---|
| Windows 10/11 (x86-64) | Installer (`.exe`) or portable archive (`.zip`) |
| macOS (Apple Silicon) | Disk image (`.dmg`) |
| Debian 12+ / Ubuntu 22.04+ | Package (`.deb`) |
| Other Linux (x86-64) | Archive (`.tar.gz`) |

Everything the viewer needs, Qt included, ships inside the package. An OpenGL
3.3 capable GPU is required.

## Source code

| Project | Repository |
|---|---|
| C++ library | [github.com/Booritas/slideio](https://github.com/Booritas/slideio) |
| Python module | [github.com/Booritas/slideio-python](https://github.com/Booritas/slideio-python) |
| Viewer | [github.com/Booritas/slideio-view](https://github.com/Booritas/slideio-view) |

Questions and bug reports are welcome on the
[issue tracker](https://github.com/Booritas/slideio/issues).
