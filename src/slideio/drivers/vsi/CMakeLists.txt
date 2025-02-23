set(LIBRARY_NAME ${VSI_DRIVER_NAME})
set(CMAKE_PREFIX_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
set(CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

set(SOURCE_FILES 
   ${CMAKE_CURRENT_SOURCE_DIR}/vsiimagedriver.hpp
   ${CMAKE_CURRENT_SOURCE_DIR}/vsiimagedriver.cpp
   ${CMAKE_CURRENT_SOURCE_DIR}/vsiscene.hpp
   ${CMAKE_CURRENT_SOURCE_DIR}/vsiscene.cpp
   ${CMAKE_CURRENT_SOURCE_DIR}/vsislide.hpp
   ${CMAKE_CURRENT_SOURCE_DIR}/vsislide.cpp
   ${CMAKE_CURRENT_SOURCE_DIR}/vsi_api_def.hpp
   ${CMAKE_CURRENT_SOURCE_DIR}/vsistruct.hpp
   ${CMAKE_CURRENT_SOURCE_DIR}/vsitags.hpp
   ${CMAKE_CURRENT_SOURCE_DIR}/vsistream.cpp
   ${CMAKE_CURRENT_SOURCE_DIR}/vsistream.hpp
   ${CMAKE_CURRENT_SOURCE_DIR}/etsfile.hpp
   ${CMAKE_CURRENT_SOURCE_DIR}/etsfile.cpp
   ${CMAKE_CURRENT_SOURCE_DIR}/vsitools.hpp
   ${CMAKE_CURRENT_SOURCE_DIR}/vsitools.cpp
   ${CMAKE_CURRENT_SOURCE_DIR}/vsifile.hpp
   ${CMAKE_CURRENT_SOURCE_DIR}/vsifile.cpp
   ${CMAKE_CURRENT_SOURCE_DIR}/vsifilescene.hpp
   ${CMAKE_CURRENT_SOURCE_DIR}/vsifilescene.cpp
   ${CMAKE_CURRENT_SOURCE_DIR}/etsfilescene.hpp
   ${CMAKE_CURRENT_SOURCE_DIR}/etsfilescene.cpp
   ${CMAKE_CURRENT_SOURCE_DIR}/volume.hpp
   ${CMAKE_CURRENT_SOURCE_DIR}/volume.cpp
   ${CMAKE_CURRENT_SOURCE_DIR}/taginfo.hpp
   ${CMAKE_CURRENT_SOURCE_DIR}/taginfo.cpp
   ${CMAKE_CURRENT_SOURCE_DIR}/dimensions.hpp
   ${CMAKE_CURRENT_SOURCE_DIR}/pyramid.hpp
   ${CMAKE_CURRENT_SOURCE_DIR}/pyramid.cpp
)

add_library(${LIBRARY_NAME} SHARED ${SOURCE_FILES})
set_target_properties(${LIBRARY_NAME} PROPERTIES OUTPUT_NAME "${LIBRARY_NAME}${LIB_NAME_SUFFIX}")

SOURCE_GROUP_BY_FOLDER(SOURCE_FILES)

target_link_libraries(${LIBRARY_NAME} ${CORE_LIB_NAME} ${IMAGETOOLS_LIB_NAME})

find_package(SQLite3)
find_package(glog)
find_package(OpenCV)
find_package(tinyxml2)
find_package(TIFF)
find_package(ZLIB)
find_package(LibLZMA)
find_package(WebP)
find_package(nlohmann_json REQUIRED)

target_link_libraries(${LIBRARY_NAME} 
   SQLite::SQLite3  
   glog::glog  
   opencv::opencv  
   tinyxml2::tinyxml2  
   TIFF::TIFF  
   ZLIB::ZLIB  
   LibLZMA::LibLZMA  
   libwebp::libwebp
   nlohmann_json::nlohmann_json
)


IF(WIN32)
   target_link_libraries(${LIBRARY_NAME} Shlwapi)
   set_target_properties(${LIBRARY_NAME} PROPERTIES LINK_FLAGS "/ignore:4099")
   target_compile_definitions(${LIBRARY_NAME} PRIVATE SLIDEIO_VSI_API _CRT_SECURE_NO_WARNINGS)
ENDIF(WIN32)

set_target_properties(${LIBRARY_NAME} PROPERTIES LINKER_LANGUAGE CXX)

if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    target_link_options(${LIBRARY_NAME} PRIVATE "-Wl,-Bsymbolic")
endif()