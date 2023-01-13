---
layout: post
title:  "CZI Image Driver"
date:   2023-01-05 10:00:00 +0100
categories: 
  - Image Drivers
---
# Overview
A CZI image is a collection of multi-dimensional pixel regions (sub-blocks) that are aquired with different parameters. As of version 1.2.2 the following parameters (dimensions) are supported:

- Channel (C) - index of a channel in a multi-channel data set
- Slice index (Z) - index of a slice in Z direction.
- Time point (T) - index of a time frame in a sequentially acquired series of data.
- Rotation (R) – used in acquisition modes where the data is recorded from various angles.
- Region (S) – for clustering items in X/Y direction (data belonging to contiguous regions of interests in a mosaic image).
- Illumination (I) - illumination direction index (e.g. from left=0, from right=1).
- Acquisition (B) Block index in segmented experiments.
- Mosaic tile index (I) – this index uniquely identifies all tiles in a specific plane.
- Phase index (H) – for specific acquisition methods.
- View index (V) - for multi – view images, e.g. SPIM.

Slideio combines the sub-blocks in 2D-4D(depending if Z slices and time-frames are present) continuous scenes. Each scene corresponds to different sequence of dimensions:

- Rotation (R).
- Region (S).
- Illumination (I).
- Acquisition (B).
- Mosaic tile index (I).
- Phase index (H).
- View index (V).

# Scene naming
The actual values of the dimension parameters are coded in the scene name. For example, scene with name: “s:1 i:0 v:2 h:1 r:0 b:1” corresponds to a region with index 1, illumination index 0, view index 2, phase index 1, rotation angle index 0 and block index 1.

# Metadata
Slide property raw_metadata returns a textual representation of embedded in the slide xml document with image metadata.

# Auxiliary images
The driver supports auxiliary images for Slide and Scene objects. Images are extracted from corresponded “attachment” sections of the file.