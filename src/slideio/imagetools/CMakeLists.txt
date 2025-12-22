
set(LIBRARY_NAME ${IMAGETOOLS_LIB_NAME})
set(CMAKE_PREFIX_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
set(CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

HIDE_LIB("libtiff")
HIDE_LIB("libjpeg")

set(SOURCE_FILES
   ${SOURCE_FILES}
   ${CMAKE_CURRENT_SOURCE_DIR}/gdalcodec.cpp
   ${CMAKE_CURRENT_SOURCE_DIR}/imagetools.hpp
   ${CMAKE_CURRENT_SOURCE_DIR}/imagetools.cpp
   ${CMAKE_CURRENT_SOURCE_DIR}/jp2kcodec.cpp
   ${CMAKE_CURRENT_SOURCE_DIR}/jp2kcodec.hpp
   ${CMAKE_CURRENT_SOURCE_DIR}/jxrcodec.cpp
   ${CMAKE_CURRENT_SOURCE_DIR}/jpegcodec.cpp
   ${CMAKE_CURRENT_SOURCE_DIR}/memory_stream.hpp
   ${CMAKE_CURRENT_SOURCE_DIR}/memory_stream.cpp
   ${CMAKE_CURRENT_SOURCE_DIR}/tifftools.hpp
   ${CMAKE_CURRENT_SOURCE_DIR}/tifftools.cpp
   ${CMAKE_CURRENT_SOURCE_DIR}/tiffkeeper.hpp
   ${CMAKE_CURRENT_SOURCE_DIR}/tiffkeeper.cpp
   ${CMAKE_CURRENT_SOURCE_DIR}/jpeglib_aux.hpp
   ${CMAKE_CURRENT_SOURCE_DIR}/jpeglib_aux.cpp
   ${CMAKE_CURRENT_SOURCE_DIR}/jp2kmem.hpp
   ${CMAKE_CURRENT_SOURCE_DIR}/jp2kmem.cpp
   ${CMAKE_CURRENT_SOURCE_DIR}/encodeparameters.hpp
   ${CMAKE_CURRENT_SOURCE_DIR}/similaritytools.hpp
   ${CMAKE_CURRENT_SOURCE_DIR}/tifffiles.cpp
   ${CMAKE_CURRENT_SOURCE_DIR}/tifffiles.hpp
   ${CMAKE_CURRENT_SOURCE_DIR}/tiffmessagehandler.cpp
   ${CMAKE_CURRENT_SOURCE_DIR}/tiffmessagehandler.hpp
)

add_library(${LIBRARY_NAME} SHARED ${SOURCE_FILES})
set_target_properties(${LIBRARY_NAME} PROPERTIES OUTPUT_NAME "${LIBRARY_NAME}${LIB_NAME_SUFFIX}")

find_package(glog)
find_package(OpenCV)
find_package(OpenJPEG)
find_package(jpegxrcodec)
find_package(Iconv)
find_package(GDAL)
find_package(SQLite3)
find_package(TIFF)
find_package(libdeflate)
find_package(LibLZMA)
find_package(WebP)
find_package(JPEG)
find_package(PNG)
find_package(ZLIB)

target_link_libraries(${LIBRARY_NAME} 
   glog::glog
   opencv::opencv
   openjp2
   jpegxrcodec::jpegxrcodec
   Iconv::Iconv
   GDAL::GDAL
   SQLite::SQLite3
   TIFF::TIFF
   libdeflate::libdeflate_static
   LibLZMA::LibLZMA
   libwebp::libwebp
   JPEG::JPEG
   PNG::PNG
   ZLIB::ZLIB
)

IF(WIN32)
   target_link_libraries(${LIBRARY_NAME} Shlwapi)
   set_target_properties(${LIBRARY_NAME} PROPERTIES LINK_FLAGS "/ignore:4099")
   target_compile_definitions(${LIBRARY_NAME} PRIVATE SLIDEIO_IMAGETOOLS_API _CRT_SECURE_NO_WARNINGS)
ENDIF(WIN32)

target_link_libraries(${LIBRARY_NAME}
   ${BASE_LIB_NAME}
   ${CORE_LIB_NAME}
)

set_target_properties(${LIBRARY_NAME} PROPERTIES LINKER_LANGUAGE CXX)
set_property(TARGET ${LIBRARY_NAME} PROPERTY POSITION_INDEPENDENT_CODE ON)

if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    target_link_options(${LIBRARY_NAME} PRIVATE "-Wl,-Bsymbolic")
endif()