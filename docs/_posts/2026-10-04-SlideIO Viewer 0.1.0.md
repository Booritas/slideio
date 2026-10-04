---
layout: post
title:  "SlideIO Viewer 0.1.0 – A Desktop Application for Whole-Slide Images"
date:   2026-10-04 10:00:00 +0100
categories: 
  - News
---

We are pleased to announce the first release of **SlideIO Viewer**, a desktop
application for opening and navigating whole-slide images. It reads every format
the SlideIO library supports, runs on Windows, macOS and Linux, and requires no
programming at all.

<!--more-->

![SlideIO Viewer]({{ site.baseurl }}/assets/viewer.png)

## What it does

Until now, looking at a slide with SlideIO meant writing code. The viewer closes
that gap. Open a file and the whole slide is there, rendered through the GPU and
read from the zoom pyramid stored inside it, so panning and zooming stay smooth
on images far larger than memory. Tiles load asynchronously: the picture stays
responsive while the next level of detail arrives.

Beyond the image itself, the viewer surfaces what the file carries around it:

- **Associated images** — the thumbnail, slide overview and label scanned
  alongside the tissue.
- **Slide properties and metadata** — magnification, resolution, pixel format,
  and the raw format-specific metadata.
- **Channels** — per-channel visibility, colour and intensity with a histogram,
  for fluorescent and multiplexed slides.
- **Scenes** — slides holding several scanned regions list them as thumbnails.
- **Z-stacks and time series** — navigation through the Z and T dimensions of 3D
  and time-lapse data sets.
- **A minimap and a scale bar**, with the slide coordinates under the cursor.

## Formats

The viewer inherits the library's format support: Aperio SVS and AFI, Leica SCN,
Zeiss CZI and ZVI, Hamamatsu NDPI, Olympus VSI, DICOM whole-slide images,
PerkinElmer QPTIFF, OME-TIFF, Philips TIFF, and ordinary TIFF, PNG and JPEG
images.

## Getting it

Packages are available for Windows, macOS and Linux. Everything the application
needs, Qt included, ships inside the package — there is no separate SlideIO
installation and nothing to compile. A GPU capable of OpenGL 3.3 is required,
which covers any graphics hardware of the last decade.

- [Download SlideIO Viewer](https://github.com/Booritas/slideio-view/releases/latest)
- [Viewer page]({{ site.baseurl }}/viewer.html) — features, keyboard shortcuts and
  the full list of packages
- [All SlideIO downloads]({{ site.baseurl }}/downloads.html)

The Windows installer and the macOS disk image are not signed by a registered
developer, so both systems will warn on first run. The release page explains how
to proceed on each.

## A first release

This is version 0.1.0, and the version number is meant honestly: the viewer is
new, and we expect to find rough edges in it that no amount of testing at home
would have shown. Please report them on the
[issue tracker](https://github.com/Booritas/slideio-view/issues) — a file that
fails to open, or opens wrongly, is the most useful bug report of all.

The viewer is built on SlideIO 2.10.0, whose
[parallel block reads]({{ site.baseurl }}/news/2026/09/29/Version-2.10.0.html) are
what let it decode several tiles at once. It is open source under the same
BSD 3-clause license as the library, at
[github.com/Booritas/slideio-view](https://github.com/Booritas/slideio-view).
