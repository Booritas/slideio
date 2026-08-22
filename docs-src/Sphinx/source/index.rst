Module SlideIO
===================================

.. image:: images/slideio.png

If you have any question about the library or want to report a bug, visit our new `forum <http://slideio.com/forum/viewforum.php?f=2>`_ .

What is new in version 2.9.0
-----------------------------
- Support of Philips TIFF whole slide images through the new PHTIFF driver.
- Reading from an explicitly selected zoom level with the new *Scene* method :py:meth:`~slideio.Scene.read_block_from_level`. The rectangle is given in the coordinate system of the level, so a tiled viewer does not have to convert its coordinates and no implicit level selection happens inside the library.
- New level tile helpers: properties *tile_count* and method *get_tile_rect* of the level info object return the tile grid of a zoom level in level coordinates.
- CZI: fixed a defect where a single pyramid level was split into two zoom levels, which could make *read_block* return a partially blank image.
- Bug fixing and small improvements.

Earlier releases added the structured metadata tree (*metadata* property of *Slide* and *Scene* objects),
multithreaded conversion, and support for OME-TIFF files.


Overview
------------------
Slideio is a python module for the reading of medical images. It allows reading whole slides as well as any region of a slide.
Large slides can be effectively scaled to a smaller size.
The module uses internal zoom pyramids of images to make the scaling process as fast as possible.
Slideio supports 2D slides as well as 3D data sets and time series.

The module delivers a raster as a numpy array and compatible with the popular computer vision library `OpenCV <https://opencv.org/>`_.

The module builds accesses images through a system of image drivers that implement specifics of different image formats. Currently following drivers are implemented:

- CZI - driver for the reading of `Zeiss CZI <https://www.zeiss.com/microscopy/int/products/microscope-software/zen/czi.html>`_ images.
- SVS - driver for the reading of `Aperio SVS <https://tmalab.jhmi.edu/imagescope.html>`_ images.
- AFI - driver for the reading of Aperio fluorescent images.
- SCN - driver for the reading of `Leica SCN images <https://www.leica-microsystems.com/company/news/news-details/article/fast-efficient-and-reliable-slide-scanner-leica-scn400-for-optimal-histological-examinations/>`_.
- GDAL - driver for the reading of generic formats like jpeg, png, tiff, etc. It uses a popular c++ image library `GDAL <https://gdal.org>`_.
- ZVI - driver for reading of images produced by Carl Zeiss `AxioVision microscope <https://microscopy-news.com/download-center/software/carl-zeiss-axiovision-digital-image-processing-software-for-your-microscope/>`_.
- DCM - driver for reading of DICOM images, including whole slide images (WSI).
- NDPI - driver for reading of Hamamatsu NDPI images.
- VSI  - driver for reading of `Olympus VSI images <https://www.olympus-lifescience.com>`_.
- QPTIFF - driver for reading of `PerkinElmer Vectra QPTIFF images <https://www.akoyabio.com/phenoimager/instruments/vectra-3-0/>`_.
- OMETIFF - driver for reading of `OME-TIFF images <https://docs.openmicroscopy.org/ome-model/5.6.3/ome-tiff/>`_.
- PHTIFF - driver for reading of `Philips TIFF whole slide images <https://www.usa.philips.com/healthcare/resources/feature-detail/intellisite-pathology-solution>`_.

The module provides 2 python classes: *Slide* and *Scene*. *Slide* is a container object returned by the module function *open_slide*. In the simplest case, a *Slide* object contains a single *Scene* object. Some slides can contain multiple scenes. For example, a czi file can contain several scanned regions, each of them is represented as a *Scene* object. *Scene* class provides methods to access image pixel values and metadata. 


Contents
----------

.. toctree::
   :maxdepth: 2
   :caption: Table of contents:

   functions
   slide
   scene
   drivers
   tutorial
   license
   software

Installation
------------------
Installation of the modile available through pip.

.. code-block::

   pip install slideio

Quick Start
-----------------

Here is an example of a reading of a czi file:

.. code-block:: python

 import slideio
 slide = slideio.open_slide(file_path="/data/a.czi",driver_id="CZI")
 scene = slide.get_scene(0)
 block = scene.read_block()

Source code
------------
Souce code is located in the `gitlab repository <https://gitlab.com/bioslide/slideio>`_ and mirror `github repository <https://github.com/Booritas/slideio>`_.

Indices and tables
------------------

* :ref:`genindex`
* :ref:`modindex`
* :ref:`search`
