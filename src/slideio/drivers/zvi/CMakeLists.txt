set(LIBRARY_NAME ${ZVI_DRIVER_NAME})
set(CMAKE_PREFIX_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
set(CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

set(SOURCE_FILES 
   ${SOURCE_FILES}
   ${CMAKE_CURRENT_SOURCE_DIR}/zviimagedriver.hpp
   ${CMAKE_CURRENT_SOURCE_DIR}/zviimagedriver.cpp
   ${CMAKE_CURRENT_SOURCE_DIR}/zviscene.hpp
   ${CMAKE_CURRENT_SOURCE_DIR}/zviscene.cpp
   ${CMAKE_CURRENT_SOURCE_DIR}/zvislide.hpp
   ${CMAKE_CURRENT_SOURCE_DIR}/zvislide.cpp
   ${CMAKE_CURRENT_SOURCE_DIR}/zviutils.hpp
   ${CMAKE_CURRENT_SOURCE_DIR}/zviutils.cpp
   ${CMAKE_CURRENT_SOURCE_DIR}/zvitags.hpp
   ${CMAKE_CURRENT_SOURCE_DIR}/zvipixelformat.hpp
   ${CMAKE_CURRENT_SOURCE_DIR}/zviimageitem.hpp
   ${CMAKE_CURRENT_SOURCE_DIR}/zviimageitem.cpp
   ${CMAKE_CURRENT_SOURCE_DIR}/zvitile.hpp
   ${CMAKE_CURRENT_SOURCE_DIR}/zvitile.cpp
   ${CMAKE_CURRENT_SOURCE_DIR}/zvi_api_def.hpp
   )

add_library(${LIBRARY_NAME} SHARED ${SOURCE_FILES})

find_package(Boost)
find_package(SQLite3)
find_package(glog)
find_package(OpenCV)
find_package(ZLIB)
find_package(pole)

target_link_libraries(${LIBRARY_NAME} 
   boost::boost
   SQLite::SQLite3
   glog::glog
   opencv::opencv
   ZLIB::ZLIB
   pole::pole
)

SOURCE_GROUP_BY_FOLDER(SOURCE_FILES)

target_link_libraries(${LIBRARY_NAME} ${CORE_LIB_NAME} ${IMAGETOOLS_LIB_NAME})

IF(WIN32)
   target_link_libraries(${LIBRARY_NAME} Shlwapi)
   set_target_properties(${LIBRARY_NAME} PROPERTIES LINK_FLAGS "/ignore:4099")
   target_compile_definitions(${LIBRARY_NAME} PRIVATE SLIDEIO_ZVI_API _CRT_SECURE_NO_WARNINGS)
ENDIF(WIN32)

set_target_properties(${LIBRARY_NAME} PROPERTIES LINKER_LANGUAGE CXX)

if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    target_link_options(${LIBRARY_NAME} PRIVATE "-Wl,-Bsymbolic")
endif()
