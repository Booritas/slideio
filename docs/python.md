---
layout: page
title: Python API
sidebar_link: true
sidebar_sort_order: 100
---
# Overview
The python module provides 2 python classes: Slide and Scene. Slide is a container object returned by the module function open_slide. In the simplest case, a Slide object contains a single Scene object. Some slides can contain multiple scenes. For example, a czi file can contain several scanned regions, each of them is represented as a Scene object. Scene class provides methods to access image pixel values and metadata.
See [Sphinx generated SlideIO python API](https://booritas.github.io/slideio/sphinx/)

# Installation

Installation
Installation of the modile available through pip.

{% gist 0c71e368e849d707a2834f8e2905bc9e %}

### Building python extension from the source code

You need a python distribution to compile the extension. You can find the distribution(s) on the [Python Web Site](https://www.python.org/downloads/). 
Note: the distribution should include header files and static libraries. 

You can build the extension for multiple distributions. For this purpose, create a text file with a list of paths to the python executables. Here is an example of the file for Windows:
```
D:\conan\python\3.10\slideio\stable\package\ca33edce272a279b24f87dc0d4cf5bbdcffbc187\Python310\python.exe
D:\conan\python\3.6\slideio\stable\package\ca33edce272a279b24f87dc0d4cf5bbdcffbc187\Python36\python.exe
D:\conan\python\3.7\slideio\stable\package\ca33edce272a279b24f87dc0d4cf5bbdcffbc187\Python37\python.exe
D:\conan\python\3.8\slideio\stable\package\ca33edce272a279b24f87dc0d4cf5bbdcffbc187\Python38\python.exe
D:\conan\python\3.9\slideio\stable\package\ca33edce272a279b24f87dc0d4cf5bbdcffbc187\Python39\python.exe
```

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
#### 3. Download/Build library dependencies
```
python install.py -a conan
```
#### 4. Build the library
```
cd ./src/py-bind
python build_py_dists.py path-to-the-file-with-python-distributons
```
After successful build, the wheel files of the extension can be found in the *SLIDEIO-ROOT/src/py-bin/dist* folder.

# Quick Start

Here is an example of a reading of a czi file:

{% gist 89c29934ebb371a60afdfd7821b9741f %}

For a tutorial check [Sphinx generated SlideioIO python documentation](https://booritas.github.io/slideio/sphinx/).