# SLIDEIO - Open source python/c++ library for reading of medical images
## Overview
Check **SlideIO** [tutorial](https://github.com/Booritas/slideio-tutorial)


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
| **VSI** | Olympus VSI images | *.vsi |  |  |
| **GDAL** | General image formates | *.jpeg,*.jpg,*.tiff,*.tiff,*.png | - | - |

The library is built as a c++ python extension and provides c++ and python interfaces.
For details visit [the library WEB site](https://booritas.github.io/slideio/).
