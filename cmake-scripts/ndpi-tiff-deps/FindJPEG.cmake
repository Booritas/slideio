# Resolves libtiff's find_package(JPEG) to the in-tree jpeg-static target from
# extern/ndpi-libjpeg-turbo, rather than to any libjpeg on the machine.
#
# This is the reason the shims exist at all. The ndpi driver calls jpeglib.h
# directly (ndpitifftools.cpp) and libtiff calls it too; the two must be the
# same build, because ndpi-libjpeg-turbo is configured WITH_JPEG8 and
# WITH_MEM_SRCDST, both of which change the size of jpeg_decompress_struct. Two
# builds that disagree surface at runtime as "JPEG parameter struct mismatch",
# not as a link error.
if(NOT TARGET jpeg-static)
    message(FATAL_ERROR
        "FindJPEG shim: jpeg-static does not exist yet. "
        "extern/ndpi-libjpeg-turbo must be added before extern/ndpi-tiff.")
endif()

if(NOT TARGET JPEG::JPEG)
    add_library(JPEG::JPEG INTERFACE IMPORTED)
    set_target_properties(JPEG::JPEG PROPERTIES INTERFACE_LINK_LIBRARIES jpeg-static)
endif()

set(JPEG_FOUND TRUE)
set(JPEG_LIBRARIES JPEG::JPEG)
set(JPEG_INCLUDE_DIRS ${NDPI_JPEG_INCLUDE_DIRS})
set(JPEG_INCLUDE_DIR ${NDPI_JPEG_INCLUDE_DIRS})

# Deliberately not IMPORTED GLOBAL. The vsi, ome-tiff and phtiff modules get
# their own JPEG::JPEG from conan's module-mode FindJPEG for the regular
# libjpeg; a global target here would already exist when those run, so conan's
# module would skip creating its CONAN_LIB:: targets and then fail setting
# properties on them. Directory scope is also exactly the visibility wanted:
# this JPEG::JPEG is the NDPI fork, and only ndpi-tiff below may see it.
