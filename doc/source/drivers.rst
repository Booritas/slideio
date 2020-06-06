Image Drivers
=================

The slideio module accesses images through a system of image drivers. A driver implements an image format specifics. The driver's capabilities depend on the image format. The table below contains information about driver capabilities.

+--------+-----------------+----------+------------+
| Driver | Multiple Scenes | Z Stacks | Time Frames|
+========+=================+==========+============+
| CZI    |     yes         |   yes    |  yes       |
+--------+-----------------+----------+------------+
|  SVS   | yes             |  no      |    no      |
+--------+-----------------+----------+------------+
| AFI    |     yes         |   no     |    no      |
+--------+-----------------+----------+------------+
| GDAL   | no              |  no      |     no     |
+--------+-----------------+----------+------------+

CZI driver
------------------

Overview
********************

A CZI image is a collection of multi-dimensional pixel regions (sub-blocks) that are aquired with different parameters.
As of version 1.2.2 the following parameters (dimensions) are supported:

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

*Slideio* combines the sub-blocks in 2D-4D(depending if Z slices and time-frames are present) continuous scenes.
Each scene corresponds to different sequence of dimensions:

- Rotation (R).
- Region (S).
- Illumination (I).
- Acquisition (B).
- Mosaic tile index (I).
- Phase index (H).
- View index (V).

Scene naming
**************

The actual values of the dimension parameters are coded in the scene name. 
For example, scene with name: "s:1 i:0 v:2 h:1 r:0 b:1" corresponds to a region with index 1, illumination index 0,
view index 2, phase index 1, rotation angle index 0 and block index 1.

Metadata
*************

*Slide* property *raw_metadata* returns a textual representation of embedded in the slide xml document with image metadata.

SVS driver
------------------
SVS driver exposes images extracted from Aperion SVS slides. Each *Slide* objects contains at least one *Scene* object - image itself.
Another *Scene* objects represent thumbnail, label, and macro.

Scene naming
**************

The following scene names are used:

- Image: main image;
- Macro: macro image;
- Thumbnail: thumbnail image;
- Label: label image;

Main image is always the first in the scene collection.

Metadata
***********

*Slide* property *raw_metadata* returns a string extracted from "Image Description" tiff tag of the main image tag directory.


AFI driver
------------------

AFI images is a collection of SVS images boundled together by an xml file with extention "afi". 
Each SVS image is represented by a separated *Scene* object.  *Slide* property raw_metadata always returns an empty string.


GDAL driver
------------------

GDAL driver opens generic formats like jpeg, png, tiff, etc. *Slide* object always contains a single *Scene* object.
 *Slide* property raw_metadata always returns an empty string.
