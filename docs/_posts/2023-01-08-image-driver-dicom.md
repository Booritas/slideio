---
layout: post
title:  "DCM Image Driver"
date:   2023-01-08 14:32:31 +0100
categories: 
  - Image Drivers
---

DCM driver opens DICOM images and directories. The path parameter in open_slide function can be one of the following:

- path to a DICOM image;
- path to a DICOMDIR file;
- path to a difrectory with DICOM files.

Each Scene object of a slide corresponds to a single series for a study and a patient. If path parameter in the open_slide function references a DICOMDIR file or a directory with DICOM files, all images are sorted by series, study and patient and a collection of scene objects is created.

# Metadata
raw_metadata property of a Slide object always returns an empty string. raw_metadata property of a Scene object returns json representation of DICOM tags for the first image of the scene.

# Auxiliary images
Auxiliary images are not supported by the driver.