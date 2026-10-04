---
layout: page
title: Python API
sidebar_link: true
sidebar_sort_order: 100
---
## Overview
The python module provides 2 python classes: Slide and Scene. Slide is a container object returned by the module function open_slide. In the simplest case, a Slide object contains a single Scene object. Some slides can contain multiple scenes. For example, a czi file can contain several scanned regions, each of them is represented as a Scene object. Scene class provides methods to access image pixel values and metadata.
See [Sphinx generated SlideIO python API]({{ site.baseurl }}/sphinx/)

## Installation

The module is installed from PyPI:

```
pip install slideio
```

Wheels are published for Python 3.9 through 3.14 on Windows x86-64, macOS
(Apple Silicon and Intel) and Linux x86-64. The wheel carries the SlideIO
library inside it, so no compiler, no Conan and no separate library
installation is needed. See the [Downloads page]({{ site.baseurl }}/downloads.html)
for every way to get SlideIO.

### Building the module from the source

The Python module is developed in its own repository,
[Booritas/slideio-python](https://github.com/Booritas/slideio-python), where
the build instructions are kept current with the code: see
[Building from source](https://github.com/Booritas/slideio-python#building-from-source)
in its README.

## Quick Start

Here is an example of a reading of a czi file:

{% gist 89c29934ebb371a60afdfd7821b9741f %}

For a tutorial check [Sphinx generated SlideIO python documentation]({{ site.baseurl }}/sphinx/).