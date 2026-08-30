#!/bin/bash
#set -e

conan upload -c slideio/2.8.0@slideio/stable -r slideio
conan upload -c opencv/4.14.0 -r slideio
conan upload -c jxrlib/cci.20260102 -r slideio
