#!/bin/bash
#set -e

conan upload -c slideio/2.8.0@slideio/stable -r slideio
conan upload -c dcmtk/3.6.8@slideio/stable -r slideio
conan upload -c opencv/4.10.0@slideio/stable -r slideio
conan upload -c jpegxrcodec/1.0.3@slideio/stable -r slideio
conan upload -c ndpi-libjpeg-turbo/2.1.2@slideio/stable -r slideio
conan upload -c ndpi-libtiff/4.3.0@slideio/stable -r slideio
conan upload -c pole/1.0.4@slideio/stable -r slideio
conan upload -c jxrlib/cci.20260102 -r slideio