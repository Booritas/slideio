---
layout: post
title:  "SCN Image Driver"
date:   2023-01-04 10:00:00 +0100
categories: 
  - Image Drivers
---

SCN driver allows reading of images produced by a popular Leica SCN400 scanner. Files produced by the scanner have extension .scn and normally contain muliple images such as high resolution scanes of different regions of tissue, thumbnails, lables and barcodes. Each of such internal images exposed as a separated scene objects. Scene property “rect” descripes size of the image and offset of the region.

# Scene naming
Names of scenes are taken from an xml metadata document located in the “Image Description” tiff tag.

# Metadata
Slide property raw_metadata returns a string extracted from “Image Description” tiff tag of the main image tag directory.

# Auxiliary images
The driver supports auxiliary images for Slide objects.