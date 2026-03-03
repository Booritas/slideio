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
| SCN    |     yes         |   no     |    no      |
+--------+-----------------+----------+------------+
| GDAL   | no              |  no      |     no     |
+--------+-----------------+----------+------------+
| ZVI    | no              |  yes     |     no     |
+--------+-----------------+----------+------------+
| DCM    | yes             |  yes     |     no     |
+--------+-----------------+----------+------------+
| NDPI   | no              |  no      |     no     |
+--------+-----------------+----------+------------+
| VSI    |     yes         |   yes    |    yes     |
+--------+-----------------+----------+------------+
| QPTIFF |     yes         |   yes    |    yes     |
+--------+-----------------+----------+------------+
| OMETIFF|     yes         |   yes    |    yes     |
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

Auxiliary images
******************
The driver supports auxiliary images for *Slide* and *Scene* objects. Images are extracted from corresponded "attachment" sections
of the file.

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

Auxiliary images
******************
The driver supports auxiliary images for *Slide* objects.


AFI driver
------------------

AFI images is a collection of SVS images boundled together by an xml file with extention "afi". 
Each SVS image is represented by a separated *Scene* object.  *Slide* property raw_metadata always returns an empty string.

SCN driver
------------------
SCN driver allows reading of images produced by a popular `Leica SCN400 scanner <https://www.leica-microsystems.com/company/news/news-details/article/fast-efficient-and-reliable-slide-scanner-leica-scn400-for-optimal-histological-examinations/>`_.
Files produced by the scanner have extension .scn and normally contain muliple images such as high resolution scanes of different regions of tissue,
thumbnails, lables and barcodes. Each of such internal images exposed as a separated scene objects. Scene property "rect" descripes 
size of the image and offset of the region.

Scene naming
******************
Names of scenes are taken from an xml metadata document located in the "Image Description" tiff tag.

Metadata
******************
*Slide* property *raw_metadata* returns a string extracted from "Image Description" tiff tag of the main image tag directory.


Auxiliary images
******************
The driver supports auxiliary images for *Slide* objects.


GDAL driver
------------------

GDAL driver opens generic formats like jpeg, png, tiff, etc. *Slide* object always contains a single *Scene* object.

Metadata
******************
*Slide* property raw_metadata always returns an empty string.

ZVI driver
------------------

ZVI driver opens images produced by Carl Zeiss `AxioVision microscope <https://microscopy-news.com/download-center/software/carl-zeiss-axiovision-digital-image-processing-software-for-your-microscope/>`_. The files can containe 2D or 3D images. *Slide* object always contains a single *Scene* object.

Metadata
******************
raw_metadata property of a *Slide* object always returns an empty string.

DCM driver
------------------

DCM driver opens DICOM images and directories. The path parameter in open_slide function can be one of the following:

 - path to a DICOM image;
 - path to a DICOMDIR file;
 - path to a difrectory with DICOM files.

Each *Scene* object of a slide corresponds to a single series for a study and a patient. If path parameter in the open_slide function references a DICOMDIR file or a directory with DICOM files, all images are sorted by series, study and patient and a collection of scene objects is created.

Metadata
******************
raw_metadata property of a *Slide* object always returns an empty string.
raw_metadata property of a *Scene* object returns json representation of DICOM tags for the first image of the scene.

Auxiliary images
******************
Auxiliary images are not supported by the driver.

NDPI driver
------------------

NDPI driver opens NDPI pathology images created by a Hamamatsu slide scanner, such as the Hamamatsu NanoZoomer.

Metadata
******************
raw_metadata property of a *Slide* and *Scene* objects always returns an empty string.

Auxiliary images
******************
The driver supports auxiliary images for Slide objects. An slide may contain the following auxiliary images:
- map
- macro

VSI driver
------------------

VSI driver opens Olympus VSI slides.

Metadata
******************
raw_metadata property of a *Slide* returns JSON representation of metadata
extracted from images.

Auxiliary images
******************
The driver supports auxiliary images for Slide objects. An slide may contain auxiliary images such as:
- overview
- map

QPTIFF driver
------------------

QPTIFF driver opens PerkingElmer Vectra slides.

Metadata
******************
raw_metadata property of a *Slide* returns XML representation of metadata
extracted from images.

Auxiliary images
******************
The driver supports auxiliary images for Slide objects.

OMETIFF driver
------------------

The driver opens OME-TIFF slides.

Metadata
******************
raw_metadata property of a *Scene* returns JSON representation of metadata
extracted from images.

Auxiliary images
******************
The driver does not support auxiliary images.