cmake_minimum_required(VERSION 3.10)
cmake_policy(SET CMP0057 NEW)
cmake_policy(SET CMP0091 NEW)

IF(WIN32 OR UNIX)
    cmake_policy(SET CMP0167 NEW)
ENDIF()

set(PROJECT_NAME slide_io)
set(projectVersion 2.7.0)

project(${PROJECT_NAME} VERSION ${projectVersion} LANGUAGES C CXX)
enable_testing()

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    add_compile_options(-Wno-switch)
    add_compile_options(-Wno-switch-default)
endif()

set( CMAKE_VERBOSE_MAKEFILE off )
set(INCLUDE_ROOT ${CMAKE_SOURCE_DIR}/src)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)
set(CMAKE_BUILD_RPATH_USE_ORIGIN 1)
set(CMAKE_CXX_STANDARD_REQUIRED, ON)
set(CMAKE_CXX_STANDARD,14)
set(CONAN_DISABLE_CHECK_COMPILER ON)

set(CORE_LIB_NAME slideio-core)
set(IMAGETOOLS_LIB_NAME slideio-imagetools)
set(SLIDEIO_LIB_NAME slideio)
set(GDAL_DRIVER_NAME slideio-gdal)
set(SVS_DRIVER_NAME slideio-svs)
set(PKE_DRIVER_NAME slideio-pke)
set(AFI_DRIVER_NAME slideio-afi)
set(CZI_DRIVER_NAME slideio-czi)
set(DCM_DRIVER_NAME slideio-dcm)
set(SCN_DRIVER_NAME slideio-scn)
set(ZVI_DRIVER_NAME slideio-zvi)
set(NDPI_DRIVER_NAME slideio-ndpi)
set(VSI_DRIVER_NAME slideio-vsi)
set(TEST_LIB_NAME slideio-test-lib)
set(BASE_LIB_NAME slideio-base)
set(CONVERTER_LIB_NAME slideio-converter)
set(TRANSFORMER_LIB_NAME slideio-transformer)

set(NAME_TOOL_LIB_LIST
    "lib${SLIDEIO_LIB_NAME}.dylib"
    "lib${CORE_LIB_NAME}.dylib"
    "lib${GDAL_DRIVER_NAME}.dylib"
    "lib${SVS_DRIVER_NAME}.dylib"
    "lib${PKE_DRIVER_NAME}.dylib"
    "lib${AFI_DRIVER_NAME}.dylib"
    "lib${CZI_DRIVER_NAME}.dylib"
    "lib${DCM_DRIVER_NAME}.dylib"
    "lib${SCN_DRIVER_NAME}.dylib"
    "lib${ZVI_DRIVER_NAME}.dylib"
    "lib${NDPI_DRIVER_NAME}.dylib"
    "lib${VSI_DRIVER_NAME}.dylib"
    "lib${IMAGETOOLS_LIB_NAME}.dylib"
    "lib${CONVERTER_LIB_NAME}.dylib"
    "lib${BASE_LIB_NAME}.dylib"
    "lib${TRANSFORMER_LIB_NAME}.dylib"
    "@rpath/libglog.dylib"
)
include_directories(${INCLUDE_ROOT})


MACRO(SOURCE_GROUP_BY_FOLDER target)
  SET(SOURCE_GROUP_DELIMITER "/")
  SET(last_dir "")
  SET(files "")
  FOREACH(file ${${target}})
    file(RELATIVE_PATH relative_file "${CMAKE_CURRENT_LIST_DIR}/${target}" ${file})
    GET_FILENAME_COMPONENT(dir "${relative_file}" PATH)
    IF (NOT "${dir}" STREQUAL "${last_dir}")
      IF (files)
        SOURCE_GROUP("${last_dir}" FILES ${files})
      ENDIF (files)
      SET(files "")
    ENDIF (NOT "${dir}" STREQUAL "${last_dir}")
    SET(files ${files} ${file})
    SET(last_dir "${dir}")
    STRING(REPLACE ".." "src" last_dir ${last_dir})
  ENDFOREACH(file)
  IF (files)
    SOURCE_GROUP("${last_dir}" FILES ${files})
  ENDIF (files)
ENDMACRO(SOURCE_GROUP_BY_FOLDER)

IF(APPLE)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -stdlib=libc++ -std=c++14")
    add_compile_definitions(_FILE_OFFSET_BITS=64)
ELSEIF(UNIX)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -static-libstdc++")
    add_compile_definitions(_GLIBCXX_USE_CXX11_ABI=0)
ENDIF()


IF(WIN32)
    set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/${CMAKE_BUILD_TYPE}/bin)
    set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/${CMAKE_BUILD_TYPE}/bin)
    set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/${CMAKE_BUILD_TYPE}/bin)
ELSE()
    set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
    set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
    set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
ENDIF(WIN32)

MACRO(FIX_MACOS_RPATH target)
    IF (APPLE)
        FOREACH(entry ${NAME_TOOL_LIB_LIST})
            string(REPLACE "@rpath/" "" new_val ${entry})
            add_custom_command(TARGET ${target}
                POST_BUILD COMMAND
                ${CMAKE_INSTALL_NAME_TOOL} -change ${entry} @loader_path/${new_val}
                $<TARGET_FILE:${target}>)
        ENDFOREACH()
    ENDIF()
ENDMACRO(FIX_MACOS_RPATH)


set(TEST_DIR ${CMAKE_RUNTIME_OUTPUT_DIRECTORY})

MACRO(HIDE_LIB LIB)
    if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
        set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -Wl,--exclude-libs,${LIB}")
    endif()
ENDMACRO(HIDE_LIB)

add_subdirectory(src)

