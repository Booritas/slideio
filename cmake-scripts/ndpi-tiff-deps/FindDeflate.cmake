# libtiff wants Deflate::Deflate; conan's libdeflate publishes
# libdeflate::libdeflate_static (or ::libdeflate when shared).
find_package(libdeflate CONFIG REQUIRED)

if(TARGET libdeflate::libdeflate_static)
    set(_ndpi_deflate libdeflate::libdeflate_static)
else()
    set(_ndpi_deflate libdeflate::libdeflate)
endif()

if(NOT TARGET Deflate::Deflate)
    add_library(Deflate::Deflate INTERFACE IMPORTED)
    set_target_properties(Deflate::Deflate PROPERTIES
        INTERFACE_LINK_LIBRARIES "${_ndpi_deflate}"
        INTERFACE_INCLUDE_DIRECTORIES "${NDPI_TIFF_DEFLATE_INCLUDE}")
endif()

set(Deflate_FOUND TRUE)
set(DEFLATE_FOUND TRUE)
set(Deflate_LIBRARIES Deflate::Deflate)
