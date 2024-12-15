

set(LIBRARY_NAME ${DCM_DRIVER_NAME})
set(CMAKE_PREFIX_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

set(SOURCE_FILES 
        ${SOURCE_FILES}
        ${CMAKE_CURRENT_SOURCE_DIR}/dcmimagedriver.hpp
        ${CMAKE_CURRENT_SOURCE_DIR}/dcmimagedriver.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/dcmscene.hpp
        ${CMAKE_CURRENT_SOURCE_DIR}/dcmscene.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/dcmslide.hpp
        ${CMAKE_CURRENT_SOURCE_DIR}/dcmslide.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/dcmfile.hpp
        ${CMAKE_CURRENT_SOURCE_DIR}/dcmfile.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/dcm_api_def.hpp
        ${CMAKE_CURRENT_SOURCE_DIR}/wsiscene.hpp
        ${CMAKE_CURRENT_SOURCE_DIR}/wsiscene.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/jp2codecparameter.hpp
        ${CMAKE_CURRENT_SOURCE_DIR}/jp2codecparameter.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/jp2decoder.hpp
        ${CMAKE_CURRENT_SOURCE_DIR}/jp2decoder.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/jp2decoderregistration.hpp
        ${CMAKE_CURRENT_SOURCE_DIR}/jp2decoderregistration.cpp
        )

add_library(${LIBRARY_NAME} SHARED ${SOURCE_FILES})

find_package(Boost)
find_package(SQLite3)
find_package(glog)
find_package(OpenCV)
find_package(DCMTK)
find_package(ZLIB)
find_package(LibLZMA)
find_package(Iconv)
find_package(WebP)
find_package(OpenJPEG)

target_link_libraries(${LIBRARY_NAME} 
        boost::boost
        SQLite::SQLite3
        glog::glog
        opencv::opencv
        dcmtk::dcmtk
        ZLIB::ZLIB
        LibLZMA::LibLZMA
        Iconv::Iconv
        libwebp::libwebp
        openjp2
)

SOURCE_GROUP_BY_FOLDER(SOURCE_FILES)

target_link_libraries(${LIBRARY_NAME} ${CORE_LIB_NAME} ${IMAGETOOLS_LIB_NAME})

IF(WIN32)
        target_link_libraries(${LIBRARY_NAME} Shlwapi)
        set_target_properties(${LIBRARY_NAME} PROPERTIES LINK_FLAGS "/ignore:4099")
        target_compile_definitions(${LIBRARY_NAME} PRIVATE SLIDEIO_DCM_API _CRT_SECURE_NO_WARNINGS)
ENDIF(WIN32)

set_target_properties(${LIBRARY_NAME} PROPERTIES LINKER_LANGUAGE CXX)
#set_target_properties(${LIBRARY_NAME} PROPERTIES CXX_VISIBILITY_PRESET hidden)

