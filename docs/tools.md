---
layout: page
title: Tools
sidebar_link: true
sidebar_sort_order: 270
---

## Overview

Two command line utilities ship with every SlideIO package:

- **`slideio-converter`** converts a slide, or a region of one, into a tiled
  pyramidal SVS or OME-TIFF file.
- **`slideio-tiffinspector`** prints the directory structure of a TIFF file,
  which is how you find out what a pyramidal TIFF actually contains.

Neither needs Python, and neither needs the library to be built: they are part
of the binary distributions.

## Installation

The tools are in every package on the [Downloads page]({{ site.baseurl }}/downloads.html).

### Debian and Ubuntu

They are a separate package, so that installing the library does not put
unversioned programs in `/usr/bin`:

```
sudo apt install ./slideio-tools_<version>_amd64.deb
```

That puts both on your `PATH`.

### macOS and Windows

Unpack the archive. The tools are in its `bin` directory and run from there —
they carry an RPATH into the package's `lib`, so nothing has to be set up
first. On Windows, add `bin` to `PATH` if you want to run them from anywhere:

```
set PATH=C:\slideio\bin;%PATH%
```

A note if you build from source: in the build tree the executables are called
`converter` and `tiffinspector`. The distributions rename them at install
time, because `converter` is too generic a name to put in a shared `bin`
directory.

## slideio-converter

Converts a slide to a tiled, pyramidal image. The output carries a zoom
pyramid, so the result stays usable at any magnification rather than being a
single enormous plane.

```
slideio-converter <input> <output> [options]
```

Both paths are required. By default the output is an OME-TIFF with JPEG 2000
compression, 512-pixel tiles, and a pyramid whose depth is chosen from the
image size.

### Options

| Option | Default | Meaning |
|---|---|---|
| `-f, --format` | `OMETIFF` | Target format: `SVS` or `OMETIFF` |
| `-m, --compression-method` | `Jpeg2000` | `Jpeg` or `Jpeg2000` |
| `-q, --quality` | `95` | JPEG quality, 0–100 |
| `-c, --compression-rate` | `5.0` | JPEG 2000 compression rate |
| `-t, --tile-size` | `512` | Tile width and height |
| `-z, --zoom-levels` | `-1` | Number of pyramid levels; `-1` chooses automatically |
| `-n, --scene-index` | `0` | Which scene to convert, for slides holding several |
| `-d, --driver` | `AUTO` | Input driver; `AUTO` detects the format |
| `-r, --rect` | whole image | Region of interest as `x,y,width,height` |
| `--channel-range` | all | Channels as `start,end` |
| `--slice-range` | all | Z-slices as `start,end` |
| `--frame-range` | all | Time frames as `start,end` |
| `-i, --info-only` | off | Print what the conversion would do, and stop |
| `-x, --delete-if-exists` | off | Overwrite the output if it is already there |
| `-s, --silent` | off | No progress bar, no information |
| `-l, --log-level` | `1` | 0 fatal, 1 error, 2 warning, 3 info |
| `-b, --batch-size` | `10` | Tiles read per operation |
| `--reading-threads` | `0` | Reading threads; `0` means half the CPU cores |
| `--encoding-threads` | `0` | Encoding threads; `0` means half the CPU cores |

`--help` lists them all, with the library version in the header.

### Examples

Convert a slide to OME-TIFF with the defaults:

```
slideio-converter image.czi image.ome.tiff
```

Produce an SVS with JPEG compression at quality 90:

```
slideio-converter image.ndpi image.svs -f SVS -m Jpeg -q 90
```

Convert one region of the second scene:

```
slideio-converter image.czi region.ome.tiff -n 1 -r 10000,8000,4096,4096
```

Check what a conversion would produce without doing it:

```
slideio-converter image.vsi out.ome.tiff --info-only
```

Reading and encoding run on separate thread pools, and reads of one scene run
in parallel on most formats, so a conversion uses the machine it is given. Cap
either pool with `--reading-threads` or `--encoding-threads` when converting
several files at once.

## slideio-tiffinspector

Prints every TIFF directory in a file, one after another:

```
slideio-tiffinspector <input>
```

It takes exactly one argument and writes to standard output, so it pipes and
redirects like any other filter.

For each directory it reports the geometry and encoding — width and height,
whether the directory is tiled and with what tile size, channels, bits per
sample, photometric interpretation, compression, data type, rows per strip,
strip size, interleaving, subfile type — along with the directory index, its
offset in the file, the description, resolution, position, and the `software`
and `dateTime` tags. Where the file carries an **ICC colour profile**, the
profile is summarised too: its size, version, data and connection spaces,
rendering intent, manufacturer, model and white point.

This is the tool for answering questions that the image itself does not: how
many pyramid levels a slide really has and at what sizes, whether levels are
tiled or stripped, what compression each one uses, which directory holds the
thumbnail or label, and whether a slide carries a colour profile at all.

```
slideio-tiffinspector slide.svs | head -40
slideio-tiffinspector slide.svs > structure.txt
```

It works on any TIFF, including the formats SlideIO reads that are TIFF
underneath — SVS, Philips TIFF, NDPI, QPTIFF and OME-TIFF — and on output
written by `slideio-converter`, which makes it the quickest way to confirm a
conversion produced the pyramid you expected.
