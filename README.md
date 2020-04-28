## Manual installation
#### Get source code
- clone slideio repository
```
git clone https://booritas@bitbucket.org/bioslide/slideio.git
```
- clone slideio-extra repository with test images
```
git clone https://booritas@bitbucket.org/bioslide/slideio_extra.git
```
#### Add slideio conan repository to your remotes
```
conan remote add slideio https://api.bintray.com/conan/booritas/slideio
```
### Windows
#### Requirements:
- Visual Studio 2019, Community edition
- cmake version 3.2 or later
- python 3.5 or later with packages:
- - conan package manager version 1.20 or later
- - numpy
- - twine
- - setuptools
- - wheel
#### Build library
- switch to the slideio root directory
- execute install.ps1 script with powershell
```
python install.py -a build 
```
#### Run tests
- set enviroment variable SLIDEIO_TEST_DATA_PATH to path to test images. For example:
```
SLIDEIO_TEST_DATA_PATH=c:\Projects\slideio_extra\testdata\cv\slideio
```
- switch to slidio/build directory and run command:
```
ctest
```

#### Create python wheel packages
- switch to the slideio/pybind directory
- execute install.ps1 script with the powershell
#### Upload packages to PyPI
- switch to the dist subdirectory of pybind
- execute
```
twine upload *
```

### Linux
#### Requirements:
For build for Linux you can use docker containers. Following containers are prepared for the project:
- Ubuntu 18.04: booritas/slideio-ubuntu-clang-9:latest . This image is used for the development of the slideio library.
- Manylinux2010: booritas/slideio-manylinux2010:latest . This image is used for praparing of python distributives.
 
The docker images have everything needed preinstalled.
For a manual installation the following software is required:
- clang compiler v.9
- python 3.5 or later with packages:
- - conan package manager version 1.20 or later
- - numpy
- - twine
- - setuptools
- - wheel

Please note, for preparing of PyPi compatible python packages you need a version of manylinux operating system. 
#### Build the slideio library:
From the slideio directory execute:
```
python install.py -a build
```
if cmake cannot find path to python interpreter or find a wrong version of the python, you can supply build utilities with the correct path.
```
python install.py -a conan
mkdir build_release && cd build_release
cmake -DCMAKE_BUILD_TYPE=Release --DPYTHON_EXECUTABLE:FILEPATH=<path-to-python-executable> ..
cmake --build . --config Release
``` 
#### Run tests
- set enviroment variable SLIDEIO_TEST_DATA_PATH to path to test images. For example:
```
export SLIDEIO_TEST_DATA_PATH=/projects/slideio_extra/testdata/cv/slideio
```
- switch to slidio/build directory and run command:
```
ctest
```
#### Create python wheel files for Linux
To creates python wheel files that can work on most of Linux systems, we use manylinux2010 docker container. Docker image **booritas/slideio-manylinux2010:latest** is inherited from the manylinux2010 and contains all dependencies needed for the slideio project. It can be downloaded from the docker hub:
```
docker pull booritas/slideio-manylinux2010:latest
```
Run docker container from the image:
```
docker run -it -v path_to_the_library_on_the_host_machine:/projects  booritas/slideio-manylinux2010:latest /bin/bash
```
In the container, switch to the pybind directory:
```
cd /projects/slideio/pybind
```
Build wheel files for all supported python versions:
```
/opt/python/cp35-cp35m/bin/python setup.py sdist bdist_wheel
/opt/python/cp36-cp36m/bin/python setup.py sdist bdist_wheel
/opt/python/cp37-cp37m/bin/python setup.py sdist bdist_wheel
/opt/python/cp38-cp38/bin/python setup.py sdist bdist_wheel
```
switch to the location of wheel files and execute for each wheel file
```
auditwheel repair <file>
```
Auditwheel producess new whl files that can be used on many Linux systems (not older thanfrom year 2010). The files can be uploaded to PyPi repository:
```
twine upload *
```