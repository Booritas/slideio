# SLIDEIO - Open source python/c++ library for reading of medical images (version 2.0.0)
## Overview
Slideio is a c++ library and a python module for the reading of medical images. It allows reading whole slides as well as any region of a slide. Large slides can be effectively scaled to a smaller size. The module uses internal zoom pyramids of images to make the scaling process as fast as possible. Slideio supports 2D slides as well as 3D data sets and time series.

The module delivers a raster as a numpy array and compatible with the popular computer vision library OpenCV.

The module builds accesses images through a system of image drivers that implement specifics of different image formats. Currently following drivers are implemented:

| **Driver** | **File format** | **File extensions** | **Format developer** | **Scanners** |
|---|---|---|---|---|
| **SVS** | [Aperio SVS](https://www.leicabiosystems.com/en-de/digital-pathology/manage/aperio-imagescope/) | *.svs | [Leica Microsystems](https://www.leicabiosystems.com/) | [Aperio GT 450 and Aperio GT 450 DX](https://www.leicabiosystems.com/en-de/digital-pathology/scan/) |
| **AFI** | [Aperio AFI - Fluorescent images](https://www.pathologynews.com/fileformats/leica-afi/) | *.afi | [Leica Microsystems](https://www.leicabiosystems.com/) |  |
| **SCN** | [Leica](https://www.leica-microsystems.com/) SCN images | *.scn | [Leica Microsystems](https://www.leicabiosystems.com/) | [Leica SCN400](https://www.leicabiosystems.com/en-de/news-events/leica-microsystems-launches-scn400-f-combined-fluorescence-and-brightfield-slide/) |
| **CZI** | [Zeiss CZI](https://www.zeiss.com/microscopy/en/products/software/zeiss-zen/czi-image-file-format.html) images | *.czi | [Zeiss Microscopy](https://www.zeiss.com/microscopy/en/home.html?vaURL=www.zeiss.com/microscopy) | [ZEISS Axioscan 7](https://www.zeiss.com/microscopy/en/products/imaging-systems/axioscan-for-biology.html) |
| **ZVI** | Zeiss ZVI image format | *.zvi | [Zeiss Microscopy](https://www.zeiss.com/microscopy/en/home.html?vaURL=www.zeiss.com/microscopy) |  |
| **DCM** | DICOM images | *.dcm, no extension |  |  |
| **NDPI** | [Hamamatsu NDPI image format](https://www.hamamatsu.com/eu/en/product/life-science-and-medical-systems/digital-slide-scanner/U12388-01.html) | *.ndpi | [Hamamatsu](https://www.hamamatsu.com/eu/en.html) |  |
| **GDAL** | General image formates | *.jpeg,*.jpg,*.tiff,*.tiff,*.png | - | - |

The library is built as a c++ python extension and provides c++ and python interfaces.
## Python Interface
### Overview
The python module provides 2 python classes: Slide and Scene. Slide is a container object returned by the module function open_slide. In the simplest case, a Slide object contains a single Scene object. Some slides can contain multiple scenes. For example, a czi file can contain several scanned regions, each of them is represented as a Scene object. Scene class provides methods to access image pixel values and metadata.
### Installation
The python package can be installed with the pip utility:
```
pip install slideio
```
## C++ interface
## Overview
The software is cross-platform and should work on Windows10/11, MacOS version 10.14 and above, most of the Linux systems. It is tested on Window 10/10, [Ubuntu] 22.04, MacOS 11.
The library provides 2 main c++ calsses: 
### Installation the library from the source
System requrements:

- Python v3.6 and above

- [Conan package manager](https://conan.io/).

### Used 3rd party libraries:

- [boost](https://boost.org)
- [dcmtk](https://dicom.offis.de/)
- [gdal](https://gdal.org)
- [gtest](https://github.com/google/googletest)
- [json-c](https://github.com/json-c/json-c)
- [JPEG XR Reference Codec](https://jpeg.org/jpegxr/software.html)
- [libjpeg](https://libjpeg.sourceforge.net/)
- [libpng](http://libpng.org)
- [libtiff](http://libtiff.org)
- [NDPITools](https://www.imnc.in2p3.fr/pagesperso/deroulers/software/ndpitools/)
- [opencv](https://opencv.org)
- [openjpeg](https://openjpeg.org)
- [pole](https://www.dimin.net/software/pole/)
- [tinyxml2](https://github.com/leethomason/tinyxml2)

