# ndpi-tiff dependency shims

`extern/ndpi-tiff` is a pristine submodule, so it cannot be patched at build
time the way the `ndpi-libtiff` conan recipe used to patch it. Its codec checks
call `find_package(Deflate)`, `find_package(JBIG)`, `find_package(ZSTD)`,
`find_package(WebP)`, `find_package(ZLIB)`, `find_package(LibLZMA)` and
`find_package(JPEG)`, and `libtiff/CMakeLists.txt` then links the imported
targets `Deflate::Deflate`, `JBIG::JBIG`, `ZSTD::ZSTD`, `WebP::WebP`,
`ZLIB::ZLIB`, `LibLZMA::LibLZMA` and `JPEG::JPEG`.

Conan spells four of those differently -- `libdeflate::libdeflate_static`,
`jbig::jbig`, `zstd::libzstd_static`, `WebP::webp` -- which is exactly what the
recipe's `4.3.0-0001-cmake-dependencies.patch` rewrote. These modules do the
same job from the outside: each finds the conan package in CONFIG mode and
publishes an imported target under the name libtiff expects. `JPEG::JPEG` is
different in kind -- it resolves to the in-tree `jpeg-static` from
`extern/ndpi-libjpeg-turbo`, which is the whole point of the exercise: libtiff
and the ndpi driver must share one libjpeg-turbo build, not two.

`src/slideio/drivers/ndpi/CMakeLists.txt` prepends this directory to
`CMAKE_MODULE_PATH` before adding the submodule. ndpi-tiff appends its own
`cmake/` directory, so these win.
