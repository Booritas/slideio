

set(LIBRARY_NAME ${SCN_DRIVER_NAME})
set(CMAKE_PREFIX_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

HIDE_LIB(libtiff)

set(SOURCE_FILES 
   ${SOURCE_FILES}
   ${CMAKE_CURRENT_SOURCE_DIR}/scnimagedriver.hpp
   ${CMAKE_CURRENT_SOURCE_DIR}/scnimagedriver.cpp
   ${CMAKE_CURRENT_SOURCE_DIR}/scnscene.hpp
   ${CMAKE_CURRENT_SOURCE_DIR}/scnscene.cpp
   ${CMAKE_CURRENT_SOURCE_DIR}/scnslide.hpp
   ${CMAKE_CURRENT_SOURCE_DIR}/scnslide.cpp
   ${CMAKE_CURRENT_SOURCE_DIR}/scn_api_def.hpp
   )

add_library(${LIBRARY_NAME} SHARED ${SOURCE_FILES})

find_package(Boost)
find_package(SQLite3)
find_package(glog)
find_package(OpenCV)
find_package(tinyxml2)
find_package(TIFF)
find_package(ZLIB)
find_package(LibLZMA)
find_package(WebP)

target_link_libraries(${LIBRARY_NAME} 
   boost::boost  
   SQLite::SQLite3  
   glog::glog  
   opencv::opencv  
   tinyxml2::tinyxml2  
   TIFF::TIFF  
   ZLIB::ZLIB  
   LibLZMA::LibLZMA  
   libwebp::libwebp
)


SOURCE_GROUP_BY_FOLDER(SOURCE_FILES)

target_link_libraries(${LIBRARY_NAME} ${CORE_LIB_NAME} ${IMAGETOOLS_LIB_NAME} ${SVS_DRIVER_NAME})

IF(WIN32)
   target_link_libraries(${LIBRARY_NAME} Shlwapi)
   set_target_properties(${LIBRARY_NAME} PROPERTIES LINK_FLAGS "/ignore:4099")
   target_compile_definitions(${LIBRARY_NAME} PRIVATE SLIDEIO_SCN_API _CRT_SECURE_NO_WARNINGS)
ENDIF(WIN32)

set_target_properties(${LIBRARY_NAME} PROPERTIES LINKER_LANGUAGE CXX)

#conan_target_link_libraries_with_hide(${LIBRARY_NAME} "^(.)+(tiff|libjpeg)(.)+")
