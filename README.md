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

The module accesses images through a system of image drivers, each implementing the specifics of one image format. The following drivers are implemented:

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
| **OME-TIFF** | [OME-TIFF](https://ome-model.readthedocs.io/en/stable/ome-tiff/) | *.ome.tif, *.ome.tiff, *.ome.tf2, *.ome.tf8, *.ome.btf | [Open Microscopy Environment](https://www.openmicroscopy.org/) | |
| **PHTIFF** | Philips TIFF | *.tif, *.tiff | [Philips](https://www.philips.com/) | [Philips IntelliSite Ultra Fast Scanner](https://www.usa.philips.com/healthcare/resources/landing/philips-intellisite-pathology-solution) |
| **GDAL** | General image formats | *.png, *.jpeg, *.jpg, *.tif, *.tiff, *.bmp, *.gif, *.jp2 | - | - |

The library is built as a c++ python extension and provides c++ and python interfaces.
For details visit [the library WEB site](https://booritas.github.io/slideio/).
## Build instructions

### Dependencies

All third-party packages come from **conan center**. `conan install` needs no
extra remote, no credentials, and nothing built in advance.

Four dependencies are git submodules under `extern/` rather than conan packages:
the JPEG XR codec (`jpegxrcodec`), the pole OLE compound-file reader (`pole`),
and the NDPI forks of libjpeg-turbo and libtiff (`ndpi-libjpeg-turbo`,
`ndpi-tiff`). Clone with `--recurse-submodules`, or run
`git submodule update --init` before configuring -- CMake stops with an error
naming any directory it finds empty.

Build with the profiles in `conan/<Platform>/`; `install.py` picks the right one
for your platform. They carry more than settings: the Linux and macOS profiles
also hold a `[conf]` entry that jxrlib needs in order to compile from source on
current compilers, so a hand-written profile is likely to fail where these
succeed.

### Syncing the toolchain (Conan profiles + CMake generator)

Before building, sync the Conan profiles with your installed compiler version:

```bash
python sync-toolchain.py
```

The script auto-detects your compiler and updates ``compiler.version`` in the
Conan profiles under ``conan/<Platform>/``:

| Platform  | Compiler        | What is synced                                                    |
|-----------|-----------------|-------------------------------------------------------------------|
| Windows   | MSVC (`cl.exe`) | Conan profiles **and** CMake generator in ``install.py``          |
| Linux     | GCC             | Conan profiles only (CMake generator is static)                   |
| macOS     | Apple Clang     | Conan profiles only (CMake generator is static)                   |

On Windows, run the script from a **Visual Studio developer command prompt**
so ``cl.exe`` is on PATH.  On Linux and macOS the standard compiler is
detected automatically.

### Linux build using manylinux docker containers
#### Prerequisites:
- Docker
- git

The manylinux images carry the build toolchain -- compilers, CMake, conan,
python and the system libraries the dependencies need -- and no slideio sources.
You mount a working copy and build in it, so the same image serves any branch or
version:
- x86_64 Linux: booritas/slideio-manylinux_2_28_x86_64:2.8.0
- s390x Linux: booritas/slideio-manylinux_2_28_s390x:2.8.0

Each image also ships a conan cache with every dependency already built, so a
build inside it goes straight to compiling slideio itself.

To build the image yourself rather than pull it, run this from the repository
root -- the context has to be the root, because the image copies the conanfiles
out of it:
```bash
docker build -f docker/manylinux_2_28_x86_64/Dockerfile -t slideio-manylinux_2_28_x86_64:local .
```
#### Build instructions
1. Clone the repository:
```bash
git clone --recurse-submodules https://github.com/Booritas/slideio
```
In a clone made without `--recurse-submodules`, fetch the submodules before
configuring (see [Dependencies](#dependencies)):
```bash
git submodule update --init
```
2. Pull docker image from the docker hub
For x86_64 processor use:
```bash
docker pull booritas/slideio-manylinux_2_28_x86_64:2.8.0
```
For s390x processor use:
```bash
docker pull booritas/slideio-manylinux_2_28_s390x:2.8.0
```
3. Start the docker container
```bash
docker run -it -v $(pwd)/slideio:/slideio booritas/slideio-manylinux_2_28_x86_64:2.8.0 bash
```
4. Inside the container
```bash
cd /slideio
python3 install.py -a install -c release
```
After the build process you can find installed files in the install subfolder of the slideio folder.

### Build for Linux and Mac
#### Prerequisites
- Python 3.6 or higher
- conan package manager version 2 or more
- CMake 3.10 or higher
- a C++17 compiler
- git
#### Build instructions
1. Clone the repository:
```bash
git clone --recurse-submodules https://github.com/Booritas/slideio
```
In a clone made without `--recurse-submodules`, fetch the submodules before
configuring (see [Dependencies](#dependencies)):
```bash
git submodule update --init
```
2. Build the SlideIO library
```bash
cd /slideio
python3 install.py -a install
```
After the build process you can find installed files in the install subfolder of the slideio folder.

### Build for Windows
#### Prerequisites
- Python 3.6 or higher
- conan package manager version 2 or more
- CMake 3.10 or higher
- Visual Studio 2022 (C++17)
- git
#### Build instructions
1. Clone the repository:
```bash
git clone --recurse-submodules https://github.com/Booritas/slideio
```
In a clone made without `--recurse-submodules`, fetch the submodules before
configuring (see [Dependencies](#dependencies)):
```bash
git submodule update --init
```
2. Build the SlideIO library
```powershell
cd /slideio
python3 install.py -a install
```
After the build process you can find installed files in the install subfolder of the slideio folder.
