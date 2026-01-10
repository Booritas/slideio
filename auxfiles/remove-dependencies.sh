#!/bin/bash
set -e

conan remove -c slideio/2.8.0@slideio/stable -r slideio
conan remove -c dcmtk/3.6.8@slideio/stable -r slideio
conan remove -c opencv/4.10.0@slideio/stable -r slideio
conan remove -c jpegxrcodec/1.0.3@slideio/stable -r slideio
conan remove -c ndpi-libjpeg-turbo/2.1.2@slideio/stable -r slideio
conan remove -c ndpi-libtiff/4.3.0@slideio/stable -r slideio
conan remove -c pole/1.0.4@slideio/stable -r slideio
conan remove -c jxrlib/cci.20260102 -r slideio
