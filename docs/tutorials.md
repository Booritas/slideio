---
layout: page
title: Tutorials
sidebar_link: true
sidebar_sort_order: 260
---

## Overview

The tutorials are Jupyter notebooks, each working through one part of the
library with runnable code and sample images. They use the Python module, but
most of what they cover — zoom pyramids, scenes, metadata, colour profiles —
applies equally to the C++ API.

They live in their own repository,
[Booritas/slideio-tutorial](https://github.com/Booritas/slideio-tutorial).
GitHub renders notebooks in the browser, so you can read any of them without
installing anything.

## Running them

```
git clone https://github.com/Booritas/slideio-tutorial.git
cd slideio-tutorial
pip install -r requirements.txt
jupyter notebook
```

The notebooks download the slides they use, so no test corpus is needed.

## The notebooks

### Getting started

- [general.ipynb](https://github.com/Booritas/slideio-tutorial/blob/master/general.ipynb)
  — an introduction to the library: opening a slide, reading regions,
  extracting metadata, and the other everyday tasks.
- [zoom-levels.ipynb](https://github.com/Booritas/slideio-tutorial/blob/master/zoom-levels.ipynb)
  — reading from an explicitly chosen pyramid level: inspecting a scene's
  levels, reading a region in a level's own coordinates with
  `read_block_from_level`, walking a level tile by tile the way a viewer
  would, and the assumptions about level geometry that real files break.

### Formats

- [dicom.ipynb](https://github.com/Booritas/slideio-tutorial/blob/master/dicom.ipynb)
  — reading DICOM files.
- [dicom-xd.ipynb](https://github.com/Booritas/slideio-tutorial/blob/master/dicom-xd.ipynb)
  — multidimensional DICOM images.
- [dicom-wsi.ipynb](https://github.com/Booritas/slideio-tutorial/blob/master/dicom-wsi.ipynb)
  — DICOM whole-slide images.
- [olympus.ipynb](https://github.com/Booritas/slideio-tutorial/blob/master/olympus.ipynb)
  — Olympus VSI files.
- [ometiff.ipynb](https://github.com/Booritas/slideio-tutorial/blob/master/ometiff.ipynb)
  — OME-TIFF images.

### Transforming and converting

- [converter.ipynb](https://github.com/Booritas/slideio-tutorial/blob/master/converter.ipynb)
  — converting slides to the Aperio SVS format, step by step.
- [color-transformations.ipynb](https://github.com/Booritas/slideio-tutorial/blob/master/color-transformations.ipynb)
  — colour transformations: manipulating channels, adjusting brightness and
  contrast, and related operations.
- [filter-transformation.ipynb](https://github.com/Booritas/slideio-tutorial/blob/master/filter-transformation.ipynb)
  — applying filters such as blurring, sharpening and edge detection.

## Reference documentation

For the API itself rather than worked examples, see the
[Python API]({{ site.baseurl }}/python.html) and
[C++ API]({{ site.baseurl }}/cpp.html) pages, and the
[Sphinx reference]({{ site.baseurl }}/sphinx/).
