

set(LIBRARY_NAME ${AFI_DRIVER_NAME})
set(CMAKE_PREFIX_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

HIDE_LIB("libtiff")
HIDE_LIB("libjpeg")

set(SOURCE_FILES 
   ${SOURCE_FILES}
   ${CMAKE_CURRENT_SOURCE_DIR}/afiimagedriver.hpp
   ${CMAKE_CURRENT_SOURCE_DIR}/afiimagedriver.cpp
   ${CMAKE_CURRENT_SOURCE_DIR}/afislide.hpp
   ${CMAKE_CURRENT_SOURCE_DIR}/afislide.cpp
   ${CMAKE_CURRENT_SOURCE_DIR}/afi_api_def.hpp
   )

add_library(${LIBRARY_NAME} SHARED ${SOURCE_FILES})

SOURCE_GROUP_BY_FOLDER(SOURCE_FILES)

target_link_libraries(${LIBRARY_NAME} ${CORE_LIB_NAME} ${IMAGETOOLS_LIB_NAME} ${SVS_DRIVER_NAME})

find_package(Boost)
find_package(SQLite3)
find_package(glog)
find_package(OpenCV)
find_package(TIFF)
find_package(LibLZMA)
find_package(JPEG)
find_package(WebP)
find_package(PNG)
find_package(ZLIB)
find_package(OpenJPEG)
find_package(tinyxml2)
find_package(Iconv)

target_link_libraries(${LIBRARY_NAME} 
   boost::boost
   SQLite::SQLite3
   glog::glog
   opencv::opencv
   TIFF::TIFF
   LibLZMA::LibLZMA
   JPEG::JPEG
   libwebp::libwebp
   PNG::PNG
   ZLIB::ZLIB
   openjp2
   tinyxml2::tinyxml2
   Iconv::Iconv
)


IF(WIN32)
   target_link_libraries(${LIBRARY_NAME} Shlwapi)
   set_target_properties(${LIBRARY_NAME} PROPERTIES LINK_FLAGS "/ignore:4099")
   target_compile_definitions(${LIBRARY_NAME} PRIVATE SLIDEIO_AFI_API _CRT_SECURE_NO_WARNINGS)
ENDIF(WIN32)

set_target_properties(${LIBRARY_NAME} PROPERTIES LINKER_LANGUAGE CXX)

#conan_target_link_libraries_with_hide(${LIBRARY_NAME} "^(.)+(tiff|libjpeg)(.)+")
