#!/bin/bash
set -e

conan remove -c slideio/2.8.0@slideio/stable -r slideio
conan remove -c opencv/4.14.0 -r slideio
conan remove -c jxrlib/cci.20260102 -r slideio
