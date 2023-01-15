---
layout: page
title: C++ API
sidebar_link: true
sidebar_sort_order: 200
---

## Overview
The software is cross-platform and should work on Windows10/11, MacOS version 10.14 and above, most of the Linux systems. It is tested on Window 10/10, [Ubuntu] 22.04, MacOS 11.
The library provides 2 main c++ calsses: 
### Installation the library from the source
System requrements:

- Python v3.6 and above
- [Conan package manager](https://conan.io/).

Execute the following steps to build the library.
#### 1. Install conan package manager
```
pip install conan
```
#### 2. Setup SlideIO conan repository
```
export CONAN_REVISIONS_ENABLED=1
conan remote add slideio-conan-local https://bioslide.jfrog.io/artifactory/api/conan/slideio-conan-local
```
#### 3. Build the library
```
python ./install.py -a build
```
After the successful build you can find all shared libraries in the directory ./build/<OSName>/Release|Debug/bin

## C++ API 

SlideIO library provides two c++ interfaces: generic interface and OpenCV based interface. Both of them implement the same functionality. The only difference that OpenCV API expose objects of OpenCV library, generic interface uses only standard c++ classes.
See [SlideIO c++ API doxygen documentation](https://booritas.github.io/slideio/doxygen/html/)

## Generic c++ Interface
SlideIO generic c++ interface provides 2 global functions slideio::openSlide(), slideio::getDriverIDs() and 2 classes: slideio::Slide and slideio::Scene. Function slideio::openSlide() opens a slide and returns object of class slideio::Slide. The class slideio::Slide exposes methods for accessing of the slide properties including metadata and images. A single instance of slideio::Slide can contain multiple raster images that are represented by slideio::Scene class. Class slideio::Scene exposes methods for working with a single raster image of a slide. The class provides method for accessing to raster data and metadata.
{% gist 83df5998e83a737661374aa3515a84d8 %}

## OpenCV Interface
SlideIO OpenCV interface exposes methods for extraction information from medical slides and intensively uses classes of the OpenCV library. The interface exposes the following classes: slideio::CVSlide, slideio::CVScene

{% gist 1ec5e35da0097e8df6b6ad25791d406c %}

## Used 3rd party libraries:

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
