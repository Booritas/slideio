cmake_minimum_required(VERSION 3.9)
cmake_policy(SET CMP0057 NEW)
cmake_policy(SET CMP0068 OLD)

set(PROJECT_NAME slide_io)
set(projectVersion 2.7.0)

project(${PROJECT_NAME} VERSION ${projectVersion} LANGUAGES C CXX)
enable_testing()

set(LIB_NAME_SUFFIX $<$<CONFIG:Debug>:_d>$<$<NOT:$<CONFIG:Debug>>:>)
set(LIB_NAME_PREFIX "slideio-")

set( CMAKE_VERBOSE_MAKEFILE OFF )
set(INCLUDE_ROOT ${CMAKE_SOURCE_DIR}/src)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)
set(CMAKE_BUILD_RPATH_USE_ORIGIN 1)
set(CMAKE_CXX_STANDARD_REQUIRED, ON)
set(CMAKE_CXX_STANDARD,17)
set(CONAN_DISABLE_CHECK_COMPILER ON)

set(CORE_LIB_NAME "${LIB_NAME_PREFIX}core")
set(IMAGETOOLS_LIB_NAME "${LIB_NAME_PREFIX}imagetools")
set(SLIDEIO_LIB_NAME "slideio")
set(GDAL_DRIVER_NAME "${LIB_NAME_PREFIX}gdal")
set(SVS_DRIVER_NAME "${LIB_NAME_PREFIX}svs")
set(PKE_DRIVER_NAME "${LIB_NAME_PREFIX}pke")
set(AFI_DRIVER_NAME "${LIB_NAME_PREFIX}afi")
set(CZI_DRIVER_NAME "${LIB_NAME_PREFIX}czi")
set(DCM_DRIVER_NAME "${LIB_NAME_PREFIX}dcm")
set(SCN_DRIVER_NAME "${LIB_NAME_PREFIX}scn")
set(ZVI_DRIVER_NAME "${LIB_NAME_PREFIX}zvi")
set(NDPI_DRIVER_NAME "${LIB_NAME_PREFIX}ndpi")
set(VSI_DRIVER_NAME "${LIB_NAME_PREFIX}vsi")
set(TEST_LIB_NAME "${LIB_NAME_PREFIX}test-lib")
set(BASE_LIB_NAME "${LIB_NAME_PREFIX}base")
set(CONVERTER_LIB_NAME "${LIB_NAME_PREFIX}converter")
set(TRANSFORMER_LIB_NAME "${LIB_NAME_PREFIX}transformer")

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
    "@rpath/libgflags_nothreads.2.2.dylib"
    "@rpath/libglog.1.dylib"
    "@rpath/libglogd.1.dylib"
    "@rpath/libgflags_nothreads_debug.2.2.dylib"
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
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -stdlib=libc++ -std=c++17 -Wno-switch")
    add_compile_definitions(_FILE_OFFSET_BITS=64)
    set(CMAKE_INSTALL_RPATH "@executable_path")
    set(CMAKE_BUILD_WITH_INSTALL_RPATH ON)
ELSEIF(UNIX)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -static-libstdc++")
    add_compile_definitions(_GLIBCXX_USE_CXX11_ABI=0)
    set(CMAKE_INSTALL_RPATH "$ORIGIN")
    set(CMAKE_BUILD_WITH_INSTALL_RPATH ON)
    ENDIF()


macro(CONAN_MAIN)
    include(conanbuildinfo_multi.cmake)
    conan_basic_setup()
endmacro()

macro(CONAN_CONFIGURE_INCLUDES trg)
    foreach(_DIR ${CONAN_INCLUDE_DIRS_RELEASE})
        target_include_directories(${trg} PRIVATE ${_DIR})
    endforeach()
    foreach(_DIR ${CONAN_INCLUDE_DIRS_DEBUG})
        target_include_directories(${trg} PRIVATE ${_DIR})
    endforeach()
endmacro()

macro(CONAN_CONFIGURE trg)
    conan_target_link_libraries(${trg})
    CONAN_CONFIGURE_INCLUDES(${trg})
endmacro()

if(MSVC)
    add_compile_options(/wd4275 /wd4251)
endif()

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

macro(macos_conan_target_link_libraries_with_hide target hide)

    SET(HIDE_CANDIDATES_DEBUG ${CONAN_LIBS_DEBUG})
    SET(NORMAL_DEBUG ${CONAN_LIBS_DEBUG})
    LIST(FILTER HIDE_CANDIDATES_DEBUG INCLUDE REGEX ${hide})
    LIST(FILTER NORMAL_DEBUG EXCLUDE REGEX ${hide})

    SET(HIDE_CANDIDATES_RELEASE ${CONAN_LIBS_RELEASE})
    SET(NORMAL_RELEASE ${CONAN_LIBS_RELEASE})
    LIST(FILTER HIDE_CANDIDATES_RELEASE INCLUDE REGEX ${hide})

    LIST(FILTER NORMAL_RELEASE EXCLUDE REGEX ${hide})

    set(HIDDEN_DEBUG "")
    foreach(_LIB ${HIDE_CANDIDATES_DEBUG})
        LIST(APPEND HIDDEN_DEBUG "-load_hidden ${_LIB}")
    endforeach()
    
    set(HIDDEN_RELEASE "")
    foreach(_LIB ${HIDE_CANDIDATES_RELEASE})
        LIST(APPEND HIDDEN_RELEASE "-load_hidden ${_LIB}")
    endforeach()

    if(CONAN_TARGETS)
        target_link_libraries(${target} ${CONAN_TARGETS})
    else()
        target_link_libraries(${target} ${CONAN_LIBS})
        target_link_libraries(${target} $<$<CONFIG:Debug>:${NORMAL_DEBUG}>$<$<CONFIG:Release>:${NORMAL_RELEASE}>)
        target_link_libraries(${target} $<$<CONFIG:Debug>:${HIDDEN_DEBUG}>$<$<CONFIG:Release>:${HIDDEN_RELEASE}>)
    endif()
endmacro()

macro(conan_target_link_libraries_with_hide target hide)
    IF(APPLE)
        macos_conan_target_link_libraries_with_hide(${target} ${hide})
    ELSE()
        conan_target_link_libraries(${target} ${hide})
    ENDIF()
endmacro()

add_subdirectory(src)


# Install the library
install(TARGETS 
             ${SLIDEIO_LIB_NAME}
             ${CORE_LIB_NAME}
             ${CONVERTER_LIB_NAME}
             ${TRANSFORMER_LIB_NAME}
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
    RUNTIME DESTINATION bin
)

install(TARGETS 
             ${BASE_LIB_NAME}
             ${GDAL_DRIVER_NAME}
             ${SVS_DRIVER_NAME}
             ${PKE_DRIVER_NAME} 
             ${AFI_DRIVER_NAME} 
             ${CZI_DRIVER_NAME} 
             ${DCM_DRIVER_NAME} 
             ${SCN_DRIVER_NAME} 
             ${ZVI_DRIVER_NAME} 
             ${NDPI_DRIVER_NAME} 
             ${VSI_DRIVER_NAME}
             ${CORE_LIB_NAME}
             ${IMAGETOOLS_LIB_NAME}
    RUNTIME DESTINATION bin
)


if(NOT WIN32)
    install(TARGETS 
                 ${BASE_LIB_NAME}
                 ${GDAL_DRIVER_NAME}
                 ${SVS_DRIVER_NAME}
                 ${PKE_DRIVER_NAME} 
                 ${AFI_DRIVER_NAME} 
                 ${CZI_DRIVER_NAME} 
                 ${DCM_DRIVER_NAME} 
                 ${SCN_DRIVER_NAME} 
                 ${ZVI_DRIVER_NAME} 
                 ${NDPI_DRIVER_NAME} 
                 ${VSI_DRIVER_NAME}
                 ${CORE_LIB_NAME}
                 ${IMAGETOOLS_LIB_NAME}
                 ${SLIDEIO_LIB_NAME}
                 ${CONVERTER_LIB_NAME}
                 ${TRANSFORMER_LIB_NAME}
            LIBRARY DESTINATION bin
    )
endif()

install(FILES 
    ${INCLUDE_ROOT}/slideio/base/base.hpp 
    ${INCLUDE_ROOT}/slideio/base/rect.hpp 
    ${INCLUDE_ROOT}/slideio/base/size.hpp 
    ${INCLUDE_ROOT}/slideio/base/resolution.hpp
    ${INCLUDE_ROOT}/slideio/base/slideio_base_def.hpp
    ${INCLUDE_ROOT}/slideio/base/slideio_enums.hpp
    ${INCLUDE_ROOT}/slideio/base/slideio_structs.hpp
    DESTINATION include/slideio/base)

install(FILES 
    ${INCLUDE_ROOT}/slideio/converter/converterparameters.hpp 
    ${INCLUDE_ROOT}/slideio/converter/convertercallback.hpp 
    ${INCLUDE_ROOT}/slideio/converter/converter_def.hpp 
    ${INCLUDE_ROOT}/slideio/converter/converter.hpp
    DESTINATION include/slideio/converter)


install(FILES 
    ${INCLUDE_ROOT}/slideio/core/slideio_core_def.hpp 
    ${INCLUDE_ROOT}/slideio/core/levelinfo.hpp 
    DESTINATION include/slideio/core)

install(FILES 
    ${INCLUDE_ROOT}/slideio/imagetools/encodeparameters.hpp 
    DESTINATION include/slideio/imagetools)

install(FILES 
    ${INCLUDE_ROOT}/slideio/slideio/slideio_def.hpp 
    ${INCLUDE_ROOT}/slideio/slideio/slideio.hpp 
    ${INCLUDE_ROOT}/slideio/slideio/slide.hpp 
    ${INCLUDE_ROOT}/slideio/slideio/scene.hpp 
    DESTINATION include/slideio/slideio)

install(FILES 
    ${INCLUDE_ROOT}/slideio/transformer/wrappers.hpp
    ${INCLUDE_ROOT}/slideio/transformer/transformer_def.hpp
    ${INCLUDE_ROOT}/slideio/transformer/transformer.hpp
    ${INCLUDE_ROOT}/slideio/transformer/transformationwrapper.hpp
    ${INCLUDE_ROOT}/slideio/transformer/transformationtype.hpp
    ${INCLUDE_ROOT}/slideio/transformer/transformations.hpp
    ${INCLUDE_ROOT}/slideio/transformer/transformation.hpp
    ${INCLUDE_ROOT}/slideio/transformer/sobelfilterwrap.hpp
    ${INCLUDE_ROOT}/slideio/transformer/scharrfilterwrap.hpp
    ${INCLUDE_ROOT}/slideio/transformer/medianblurfilterwrap.hpp
    ${INCLUDE_ROOT}/slideio/transformer/laplacianfilterwrap.hpp
    ${INCLUDE_ROOT}/slideio/transformer/gaussianblurfilterwrap.hpp
    ${INCLUDE_ROOT}/slideio/transformer/colortransformationwrap.hpp
    ${INCLUDE_ROOT}/slideio/transformer/colorspace.hpp
    ${INCLUDE_ROOT}/slideio/transformer/cannyfilterwrap.hpp
    ${INCLUDE_ROOT}/slideio/transformer/bilateralfilterwrap.hpp
    DESTINATION include/slideio/transformer)
