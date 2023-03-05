CONAN_MAIN()
set(LIBRARY_NAME ${IMAGETOOLS_LIB_NAME})

HIDE_LIB("libtiff")
HIDE_LIB("libjpeg")

set(SOURCE_FILES
   ${SOURCE_FILES}
   ${CMAKE_CURRENT_SOURCE_DIR}/gdalcodec.cpp
   ${CMAKE_CURRENT_SOURCE_DIR}/imagetools.hpp
   ${CMAKE_CURRENT_SOURCE_DIR}/imagetools.cpp
   ${CMAKE_CURRENT_SOURCE_DIR}/jp2kcodec.cpp
   ${CMAKE_CURRENT_SOURCE_DIR}/jxrcodec.cpp
   ${CMAKE_CURRENT_SOURCE_DIR}/jpegcodec.cpp
   ${CMAKE_CURRENT_SOURCE_DIR}/memory_stream.hpp
   ${CMAKE_CURRENT_SOURCE_DIR}/memory_stream.cpp
   ${CMAKE_CURRENT_SOURCE_DIR}/tifftools.hpp
   ${CMAKE_CURRENT_SOURCE_DIR}/tifftools.cpp
   ${CMAKE_CURRENT_SOURCE_DIR}/tempfile.hpp
   ${CMAKE_CURRENT_SOURCE_DIR}/cvtools.cpp
   ${CMAKE_CURRENT_SOURCE_DIR}/cvtools.hpp
   ${CMAKE_CURRENT_SOURCE_DIR}/tiffkeeper.hpp
   ${CMAKE_CURRENT_SOURCE_DIR}/tiffkeeper.cpp
   ${CMAKE_CURRENT_SOURCE_DIR}/jpeglib_aux.hpp
   ${CMAKE_CURRENT_SOURCE_DIR}/jpeglib_aux.cpp
   ${CMAKE_CURRENT_SOURCE_DIR}/jp2kmem.hpp
   ${CMAKE_CURRENT_SOURCE_DIR}/jp2kmem.cpp
)

add_library(${LIBRARY_NAME} SHARED ${SOURCE_FILES})

IF(WIN32)
   target_link_libraries(${LIBRARY_NAME} Shlwapi)
   set_target_properties(${LIBRARY_NAME} PROPERTIES LINK_FLAGS "/ignore:4099")
   target_compile_definitions(${LIBRARY_NAME} PRIVATE SLIDEIO_IMAGETOOLS_API _CRT_SECURE_NO_WARNINGS)
ENDIF(WIN32)

target_link_libraries(${LIBRARY_NAME}
   ${BASE_LIB_NAME}
)

CONAN_CONFIGURE_INCLUDES(${LIBRARY_NAME})
conan_target_link_libraries_with_hide(${LIBRARY_NAME} "^(.)+(tiff|libjpeg)(.)+")

set_target_properties(${LIBRARY_NAME} PROPERTIES LINKER_LANGUAGE CXX)
set_property(TARGET ${LIBRARY_NAME} PROPERTY POSITION_INDEPENDENT_CODE ON)