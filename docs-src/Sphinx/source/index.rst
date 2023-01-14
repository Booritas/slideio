Module slideio (version 0.6.1)
===================================

.. image:: images/mrt.png

If you have any question about the library or want to report a bug, visit our new `forum <http://slideio.com/forum/viewforum.php?f=2>`_ .

What is new
-------------------
- Support of DICOM files
- New functionality for retriving of auxiliary images of scene and slide objects such as thumbnails and labels.
- Bug fixing and small improvements


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
- DCM - driver for reading of DICOM images.

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
 slide = slideio.open_slidei(file_path="/data/a.czi",driver_id="CZI")
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
