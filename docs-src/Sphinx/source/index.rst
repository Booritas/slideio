Module SlideIO
===================================

.. image:: images/slideio.png

If you have any question about the library or want to report a bug, visit our new `forum <http://slideio.com/forum/viewforum.php?f=2>`_ .

What is new in version 2.10.0
-----------------------------
- Colour management: :py:meth:`~slideio.Scene.get_color_profile` returns a scene's embedded ICC profile as raw bytes (or *None* if it carries none), and :py:meth:`~slideio.Scene.get_color_profile_info` returns a parsed summary of it. The new :py:class:`~slideio.ColorManagement` transformation converts scene pixels into a device-independent colour space -- sRGB, linear RGB, CIE Lab or XYZ. See `Colour management`_ below.
- Bug fixing and small improvements.

Earlier releases added support for Philips TIFF whole slide images, reading from an
explicitly selected zoom level with :py:meth:`~slideio.Scene.read_block_from_level`,
the structured metadata tree (*metadata* property of *Slide* and *Scene* objects),
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

Colour management
------------------
Some image formats embed an ICC colour profile that describes how a scene's raw pixel values map to a real colour space. *Scene* exposes it two ways: :py:meth:`~slideio.Scene.get_color_profile` returns the raw profile bytes, or *None* if the scene carries none, and :py:meth:`~slideio.Scene.get_color_profile_info` returns a parsed summary as a :py:class:`~slideio.ColorProfileInfo` object, including where the profile came from (:py:class:`~slideio.ColorProfileSource`: a profile embedded in the file, one you supplied yourself, an assumed one, or none at all).

Those four values are worth keeping apart when you record what a pipeline did. ``EMBEDDED`` means the slide carried the colorimetry; ``SUPPLIED`` means you provided it through ``source_profile_override``, so the claim about the scanner is yours rather than the file's; ``ASSUMED`` means nothing was found and sRGB was assumed, so no real correction took place.

To convert a scene's pixels into a device-independent colour space, transform it with :py:class:`~slideio.ColorManagement`:

.. code-block:: python

 import slideio

 slide = slideio.open_slide(file_path="/data/a.svs", driver_id="SVS")
 scene = slide.get_scene(0)

 cm = slideio.ColorManagement()
 cm.target = slideio.ColorTarget.LAB
 lab_scene = slideio.transform_scene(scene, [cm])
 block = lab_scene.read_block()

*ColorManagement* accepts three-channel scenes only, and converts to one of four targets (:py:class:`~slideio.ColorTarget`): *SRGB*, *LINEAR_RGB*, *LAB* or *XYZ*. When a scene carries no embedded profile it assumes sRGB by default; set *missing_profile_policy* (:py:class:`~slideio.MissingProfilePolicy`) to change that -- for example to *FAIL*, to reject scenes without a real embedded profile, or to *PASS_THROUGH*, to leave the pixels untouched when the target is sRGB.

If you have characterised the scanner yourself, assign the profile to *source_profile_override* as raw ICC bytes; it is used in place of whatever the scene embeds, so even a slide with no profile converts colorimetrically and *missing_profile_policy* never applies. Assign *None* to clear it.

.. code-block:: python

 with open("/data/scanner.icc", "rb") as icc:
     cm.source_profile_override = icc.read()



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
