

set(LIBRARY_NAME ${PKE_DRIVER_NAME})
set(CMAKE_PREFIX_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
set(CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

HIDE_LIB(libtiff)
HIDE_LIB(libjpeg)

set(SOURCE_FILES
   ${SOURCE_FILES}
   ${CMAKE_CURRENT_SOURCE_DIR}/pkeimagedriver.hpp
   ${CMAKE_CURRENT_SOURCE_DIR}/pkeimagedriver.cpp
   ${CMAKE_CURRENT_SOURCE_DIR}/pkescene.hpp
   ${CMAKE_CURRENT_SOURCE_DIR}/pkescene.cpp
   ${CMAKE_CURRENT_SOURCE_DIR}/pkeslide.hpp
   ${CMAKE_CURRENT_SOURCE_DIR}/pkeslide.cpp
   ${CMAKE_CURRENT_SOURCE_DIR}/pkesmallscene.hpp
   ${CMAKE_CURRENT_SOURCE_DIR}/pkesmallscene.cpp
   ${CMAKE_CURRENT_SOURCE_DIR}/pketiledscene.hpp
   ${CMAKE_CURRENT_SOURCE_DIR}/pketiledscene.cpp
   ${CMAKE_CURRENT_SOURCE_DIR}/pketools.hpp
   ${CMAKE_CURRENT_SOURCE_DIR}/pketools.cpp
   ${CMAKE_CURRENT_SOURCE_DIR}/pke_api_def.hpp
   )

add_library(${LIBRARY_NAME} SHARED ${SOURCE_FILES})

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
find_package(Iconv)
find_package(tinyxml2)

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
   Iconv::Iconv  
   tinyxml2::tinyxml2
)


SOURCE_GROUP_BY_FOLDER(SOURCE_FILES)

target_link_libraries(${LIBRARY_NAME} ${CORE_LIB_NAME} ${IMAGETOOLS_LIB_NAME})

IF(WIN32)
   target_link_libraries(${LIBRARY_NAME} Shlwapi)
   set_target_properties(${LIBRARY_NAME} PROPERTIES LINK_FLAGS "/ignore:4099")
   target_compile_definitions(${LIBRARY_NAME} PRIVATE SLIDEIO_PKE_API _CRT_SECURE_NO_WARNINGS)
ENDIF(WIN32)

set_target_properties(${LIBRARY_NAME} PROPERTIES LINKER_LANGUAGE CXX)

if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    target_link_options(${LIBRARY_NAME} PRIVATE "-Wl,-Bsymbolic")
endif()