# libtiff wants ZSTD::ZSTD; conan's zstd publishes zstd::libzstd_static
# (or zstd::libzstd_shared).
find_package(zstd CONFIG REQUIRED)

if(TARGET zstd::libzstd_static)
    set(_ndpi_zstd zstd::libzstd_static)
elseif(TARGET zstd::libzstd_shared)
    set(_ndpi_zstd zstd::libzstd_shared)
else()
    set(_ndpi_zstd zstd::zstd)
endif()

if(NOT TARGET ZSTD::ZSTD)
    add_library(ZSTD::ZSTD INTERFACE IMPORTED)
    set_target_properties(ZSTD::ZSTD PROPERTIES
        INTERFACE_LINK_LIBRARIES "${_ndpi_zstd}"
        INTERFACE_INCLUDE_DIRECTORIES "${NDPI_TIFF_ZSTD_INCLUDE}")
endif()

set(ZSTD_FOUND TRUE)
set(ZSTD_LIBRARIES ZSTD::ZSTD)
set(ZSTD_INCLUDE_DIRS ${NDPI_TIFF_ZSTD_INCLUDE})
