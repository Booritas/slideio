# libtiff wants WebP::WebP; conan's libwebp publishes WebP::webp. CMake target
# names are case sensitive, so these are two different names for one library.
find_package(WebP CONFIG REQUIRED)

if(NOT TARGET WebP::WebP)
    add_library(WebP::WebP INTERFACE IMPORTED)
    set_target_properties(WebP::WebP PROPERTIES INTERFACE_LINK_LIBRARIES WebP::webp)
endif()

set(WebP_FOUND TRUE)
set(WEBP_FOUND TRUE)
set(WebP_LIBRARIES WebP::WebP)
