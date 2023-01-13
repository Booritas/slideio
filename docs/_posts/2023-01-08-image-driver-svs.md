---
layout: post
title:  "SVS Image Driver"
date:   2023-01-01 10:00:00 +0100
categories: 
  - Image Drivers
---

SVS driver exposes images extracted from Aperion SVS slides. Each Slide objects contains at least one Scene object - image itself. Another Scene objects represent thumbnail, label, and macro.

# Scene naming
The following scene names are used:

- Image: main image;
- Macro: macro image;
- Thumbnail: thumbnail image;
- Label: label image;

Main image is always the first in the scene collection.

# Metadata
Slide property raw_metadata returns a string extracted from “Image Description” tiff tag of the main image tag directory.

# Auxiliary images
The driver supports auxiliary images for Slide objects.