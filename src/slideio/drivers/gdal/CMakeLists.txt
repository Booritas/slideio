

set(LIBRARY_NAME ${GDAL_DRIVER_NAME})
set(CMAKE_PREFIX_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
set(CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

HIDE_LIB("libtiff")
HIDE_LIB("libjpeg")

set(SOURCE_FILES ${SOURCE_FILES}
   ${CMAKE_CURRENT_SOURCE_DIR}/gdalimagedriver.hpp
   ${CMAKE_CURRENT_SOURCE_DIR}/gdalimagedriver.cpp
   ${CMAKE_CURRENT_SOURCE_DIR}/gdalscene.hpp
   ${CMAKE_CURRENT_SOURCE_DIR}/gdalscene.cpp
   ${CMAKE_CURRENT_SOURCE_DIR}/gdalslide.hpp
   ${CMAKE_CURRENT_SOURCE_DIR}/gdalslide.cpp
   ${CMAKE_CURRENT_SOURCE_DIR}/gdal_api_def.hpp
   )

add_library(${LIBRARY_NAME} SHARED ${SOURCE_FILES})
set_target_properties(${LIBRARY_NAME} PROPERTIES OUTPUT_NAME "${LIBRARY_NAME}${LIB_NAME_SUFFIX}")

find_package(Boost)
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
   boost::boost  
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


SOURCE_GROUP_BY_FOLDER(SOURCE_FILES)

target_link_libraries(${LIBRARY_NAME} ${CORE_LIB_NAME} ${IMAGETOOLS_LIB_NAME})

IF(WIN32)
   target_link_libraries(${LIBRARY_NAME} Shlwapi)
   set_target_properties(${LIBRARY_NAME} PROPERTIES LINK_FLAGS "/ignore:4099")
   target_compile_definitions(${LIBRARY_NAME} PRIVATE SLIDEIO_GDAL_API _CRT_SECURE_NO_WARNINGS)
ENDIF(WIN32)

set_target_properties(${LIBRARY_NAME} PROPERTIES LINKER_LANGUAGE CXX)

if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    target_link_options(${LIBRARY_NAME} PRIVATE "-Wl,-Bsymbolic")
endif()