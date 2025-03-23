# SLIDEIO - Open source python/c++ library for reading of medical images
## Overview
Relevant resources:
- [Core C++ **SlideIO** library](https://github.com/Booritas/slideio)
- [**SlideIO** Python bindings](https://github.com/Booritas/slideio-python)
- [**SlideIO** Python tutorial](https://github.com/Booritas/slideio-tutorial)
- [**SlideIO** Home page](https://www.slideio.com/)
- [**SlideIO** Python API documentation](https://www.slideio.com/sphinx)

**Note: if you are interested in the python bindings of the library use [**SlideIO** Python bindings](https://github.com/Booritas/slideio-python).**

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
| **QPTIFF** | PerkinElmer Vectra QPTIFF | *.qptiff | [Akoya Biosciences](https://www.akoyabio.com/software-data-analysis/) | [Perkin Elmer Vectra scanner](https://www.akoyabio.com/phenoimager/instruments/vectra-3-0/) |
| **GDAL** | General image formates | *.jpeg,*.jpg,*.tiff,*.tiff,*.png | - | - |

The library is built as a c++ python extension and provides c++ and python interfaces.
For details visit [the library WEB site](https://booritas.github.io/slideio/).
## Build instructions
### Linux build using manylinux docker containers
#### Prerequisites:
- Docker
- Python 3.6 or higher
- git
For manylinux slideio provides 2 docker containers:
- x86_64 Linux: booritas/slideio-manylinux_2_28_x86_64:2.7.1
- s390x Linux: booritas/slideio-manylinux_2_28_s390x:2.7.1
#### Build instructions
1. Clone the repository:
```bash
git clone https://github.com/Booritas/slideio
```
2. Pull docker image from the docker hub
For x86_64 processor use:
```bash
docker pull booritas/slideio-manylinux_2_28_x86_64:2.7.1
```
For s390x processor use:
```bash
docker pull booritas/slideio-manylinux_2_28_x86_64:2.7.1
```
3. Start the docker container
```bash
docker run -it -v $(PWD)/slideio:/slideio  booritas/slideio-manylinux_2_28_x86_64:2.7.1 bash
```
4. Inside the container
```bash
cd /slideio
python3 install.py -a install
```
After the build process you can find installed files in the install subfolder of the slideio folder.

### Build for Linux and Mac
#### Prerequisites
- Docker
- Python 3.6 or higher
- conan package manager version 2 or more
- git
#### Build instructions
1. Clone the repository:
```bash
git clone https://github.com/Booritas/slideio
```
2. Build custom dependencies
```bash
bash ./conan.sh
```
3. Build the SlideIO library
```bash
cd /slideio
python3 install.py -a install
```
After the build process you can find installed files in the install subfolder of the slideio folder.

### Build for Windows
#### Prerequisites
- Docker
- Python 3.6 or higher
- conan package manager version 2 or more
- git
#### Build instructions
1. Clone the repository:
```bash
git clone https://github.com/Booritas/slideio
```
2. Build custom dependencies
```powershell
powershell ./conan.ps1
```
3. Build the SlideIO library
```powershell
cd /slideio
python3 install.py -a install
```
After the build process you can find installed files in the install subfolder of the slideio folder.
