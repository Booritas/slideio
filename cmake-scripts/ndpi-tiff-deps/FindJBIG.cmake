# libtiff wants JBIG::JBIG; conan's jbig publishes jbig::jbig.
find_package(jbig CONFIG REQUIRED)

if(NOT TARGET JBIG::JBIG)
    add_library(JBIG::JBIG INTERFACE IMPORTED)
    set_target_properties(JBIG::JBIG PROPERTIES
        INTERFACE_LINK_LIBRARIES "jbig::jbig"
        INTERFACE_INCLUDE_DIRECTORIES "${NDPI_TIFF_JBIG_INCLUDE}")
endif()

set(JBIG_FOUND TRUE)
set(JBIG_LIBRARIES JBIG::JBIG)
set(JBIG_INCLUDE_DIRS ${NDPI_TIFF_JBIG_INCLUDE})
