set(LIBRARY_NAME ${NDPI_DRIVER_NAME})
set(CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
set(CMAKE_PREFIX_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

HIDE_LIB("libtiff")
HIDE_LIB("libjpeg")


set(SOURCE_FILES 
   ${SOURCE_FILES}
   ${CMAKE_CURRENT_SOURCE_DIR}/ndpiimagedriver.hpp
   ${CMAKE_CURRENT_SOURCE_DIR}/ndpiimagedriver.cpp
   ${CMAKE_CURRENT_SOURCE_DIR}/ndpiscene.hpp
   ${CMAKE_CURRENT_SOURCE_DIR}/ndpiscene.cpp
   ${CMAKE_CURRENT_SOURCE_DIR}/ndpislide.hpp
   ${CMAKE_CURRENT_SOURCE_DIR}/ndpislide.cpp
   ${CMAKE_CURRENT_SOURCE_DIR}/ndpi_api_def.hpp
   ${CMAKE_CURRENT_SOURCE_DIR}/ndpifile.hpp
   ${CMAKE_CURRENT_SOURCE_DIR}/ndpifile.cpp
   ${CMAKE_CURRENT_SOURCE_DIR}/ndpitifftools.hpp
   ${CMAKE_CURRENT_SOURCE_DIR}/ndpitifftools.cpp
   ${CMAKE_CURRENT_SOURCE_DIR}/ndpilibtiff.hpp
   ${CMAKE_CURRENT_SOURCE_DIR}/ndpitiffmessagehandler.hpp
   ${CMAKE_CURRENT_SOURCE_DIR}/ndpitiffmessagehandler.cpp
   )

add_library(${LIBRARY_NAME} SHARED ${SOURCE_FILES})
set_target_properties(${LIBRARY_NAME} PROPERTIES OUTPUT_NAME "${LIBRARY_NAME}${LIB_NAME_SUFFIX}")

find_package(Boost)
find_package(SQLite3)
find_package(glog)
find_package(OpenCV)
find_package(NDPITIFF)
find_package(ZLIB)
find_package(LibLZMA)
find_package(NDPIJPEG)
find_package(WebP)
find_package(jpegxrcodec)
find_package(lerc)


target_link_libraries(${LIBRARY_NAME} 
   boost::boost
   SQLite::SQLite3  
   glog::glog  
   opencv::opencv  
   NDPITIFF::NDPITIFF  
   ZLIB::ZLIB  
   LibLZMA::LibLZMA  
   NDPIJPEG::NDPIJPEG  
   libwebp::libwebp  
   jpegxrcodec::jpegxrcodec  
   lerc::lerc
)


SOURCE_GROUP_BY_FOLDER(SOURCE_FILES)

target_link_libraries(${LIBRARY_NAME} ${CORE_LIB_NAME})

IF(WIN32)
   target_link_libraries(${LIBRARY_NAME} Shlwapi)
   set_target_properties(${LIBRARY_NAME} PROPERTIES LINK_FLAGS "/ignore:4099")
   target_compile_definitions(${LIBRARY_NAME} PRIVATE SLIDEIO_NDPI_API _CRT_SECURE_NO_WARNINGS)
ENDIF(WIN32)

set_target_properties(${LIBRARY_NAME} PROPERTIES LINKER_LANGUAGE CXX)

if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    target_link_options(${LIBRARY_NAME} PRIVATE "-Wl,-Bsymbolic")
endif()