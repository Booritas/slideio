## Manual installation
#### Linux
#### Configuration of 3rd party libraries with conan package manager
From the slideio directory execute:
```
python install.py -a conan
```
#### Configuration of the release configuration. 
```
mkdir build_release && cd build_release
cmake .. -DCMAKE_BUILD_TYPE=Release
```
#### Build of the release version.
```
cmake --build . --config Release
```
#### Run tests
