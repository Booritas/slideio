cmake_minimum_required(VERSION 3.10)
cmake_policy(SET CMP0057 NEW)
cmake_policy(SET CMP0091 NEW)

if(${CMAKE_VERSION} VERSION_GREATER_EQUAL "3.30")
    cmake_policy(SET CMP0167 NEW)
endif()

set(PROJECT_NAME slide_io)
set(projectVersion 2.9.0)

project(${PROJECT_NAME} VERSION ${projectVersion} LANGUAGES C CXX)
enable_testing()

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    add_compile_options(-Wno-switch)
    add_compile_options(-Wno-switch-default)
endif()

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    if(CMAKE_BUILD_TYPE STREQUAL "Release")
        set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -s")
        set(CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} -s")
    endif()
endif()

add_compile_definitions(SLIDEIO_INTERNAL_HEADER)

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
set(OMETIFF_DRIVER_NAME "${LIB_NAME_PREFIX}ometiff")
set(TEST_LIB_NAME "${LIB_NAME_PREFIX}test-lib")
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
    "lib${PKE_DRIVER_NAME}.dylib"
    "lib${OMETIFF_DRIVER_NAME}.dylib"
    "lib${IMAGETOOLS_LIB_NAME}.dylib"
    "lib${CONVERTER_LIB_NAME}.dylib"
    "lib${TRANSFORMER_LIB_NAME}.dylib"
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
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -stdlib=libc++")
    add_compile_definitions(_FILE_OFFSET_BITS=64)
    set(CMAKE_INSTALL_RPATH "@executable_path")
    set(CMAKE_BUILD_WITH_INSTALL_RPATH ON)
ELSEIF(UNIX)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -static-libstdc++")
    set(CMAKE_INSTALL_RPATH "$ORIGIN")
    set(CMAKE_BUILD_WITH_INSTALL_RPATH ON)
    ENDIF()

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

# Keeps a target's bundled libtiff/libjpeg private to that target, and checks
# afterwards that it worked.
#
# This project links two different builds of each into one process: the ndpi
# driver carries the ndpi-libtiff and ndpi-libjpeg-turbo forks, every other
# module carries libtiff and libjpeg. The symbol names are identical, and the
# structs are not the same size, so if the names are exported then which copy
# answers a call depends on load order and the disagreement shows up at runtime
# as libjpeg's "JPEG parameter struct mismatch" rather than as a link error.
#
# Replaces the earlier HIDE_LIB macro, which was a no-op except under GNU --
# leaving both AppleClang and Clang-on-Linux unprotected -- and which appended to
# directory-scoped CMAKE_SHARED_LINKER_FLAGS, so its effect leaked into every
# target configured after it rather than applying to the one that needed it.
FUNCTION(HIDE_THIRD_PARTY_SYMBOLS target)
    if(APPLE)
        # ld64 has no --exclude-libs; suppress by symbol pattern instead.
        target_link_options(${target} PRIVATE
            "-Wl,-unexported_symbol,_jpeg_*"
            "-Wl,-unexported_symbol,_TIFF*")
    elseif(UNIX)
        # --exclude-libs hides symbols brought in from static archives;
        # -Bsymbolic makes this object's own calls prefer its own definitions.
        target_link_options(${target} PRIVATE
            "-Wl,--exclude-libs,ALL"
            "-Wl,-Bsymbolic")
    endif()
    # Windows needs neither: only __declspec(dllexport) symbols are exported.

    # The script lives in cmake-scripts/ and not in cmake/, because .gitignore
    # excludes **/cmake/ -- those directories hold conan-generated files. A guard
    # that git refuses to track is a guard nobody else gets.
    if(NOT WIN32)
        add_custom_command(TARGET ${target} POST_BUILD
            COMMAND ${CMAKE_COMMAND}
                -DTARGET_FILE=$<TARGET_FILE:${target}>
                -DAPPLE_HOST=$<BOOL:${APPLE}>
                -P ${CMAKE_SOURCE_DIR}/cmake-scripts/assert_no_thirdparty_exports.cmake
            VERBATIM)
    endif()
ENDFUNCTION(HIDE_THIRD_PARTY_SYMBOLS)

# jpegxrcodec: our own JPEG XR codec, built from the extern/jpegxrcodec
# submodule instead of being pulled in as the jpegxrcodec/1.0.3@slideio/stable
# conan package. Same sources -- the conan recipe only cloned this repository
# and ran its CMakeLists -- but the codec now moves in step with the code that
# calls it, and a change to it no longer needs a package rebuild and upload
# before it can be tested here.
#
# The submodule ships a `jxrcodec` STATIC target; it is linked into
# slideio-imagetools, slideio-czi and slideio-ndpi, and (like every other static
# dependency) kept private to each by HIDE_THIRD_PARTY_SYMBOLS.
set(JPEGXRCODEC_DIR ${CMAKE_CURRENT_SOURCE_DIR}/extern/jpegxrcodec)
if(NOT EXISTS ${JPEGXRCODEC_DIR}/CMakeLists.txt)
    message(FATAL_ERROR
        "extern/jpegxrcodec is empty. Run 'git submodule update --init' in "
        "${CMAKE_CURRENT_SOURCE_DIR} and configure again.")
endif()
# Its tests need the googletest submodule nested inside it, which a plain
# 'git submodule update --init' does not fetch. FORCE because this is not a
# choice the slideio build offers: run those tests from their own repository.
set(PACKAGE_TESTS OFF CACHE BOOL "Build the jpegxrcodec tests" FORCE)
add_subdirectory(${JPEGXRCODEC_DIR} ${CMAKE_BINARY_DIR}/extern/jpegxrcodec)
# The codec exports no usage requirements of its own; consumers include it as
# <jxrcodec/jxrcodec.hpp>, so the repository root is the include directory.
target_include_directories(jxrcodec PUBLIC ${JPEGXRCODEC_DIR})

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
             ${OMETIFF_DRIVER_NAME}
             ${CORE_LIB_NAME}
             ${IMAGETOOLS_LIB_NAME}
    RUNTIME DESTINATION bin
)


if(NOT WIN32)
    install(TARGETS
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
                 ${OMETIFF_DRIVER_NAME}
                 ${CORE_LIB_NAME}
                 ${IMAGETOOLS_LIB_NAME}
                 ${SLIDEIO_LIB_NAME}
                 ${CONVERTER_LIB_NAME}
                 ${TRANSFORMER_LIB_NAME}
            LIBRARY DESTINATION bin
    )
endif()

# Install debug symbol files (PDB) alongside binaries for Debug builds on MSVC.
if(MSVC)
    set(SLIDEIO_PDB_TARGETS
        ${SLIDEIO_LIB_NAME}
        ${CORE_LIB_NAME}
        ${CONVERTER_LIB_NAME}
        ${TRANSFORMER_LIB_NAME}
        ${IMAGETOOLS_LIB_NAME}
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
        ${OMETIFF_DRIVER_NAME}
    )
    foreach(_pdb_target ${SLIDEIO_PDB_TARGETS})
        install(FILES $<TARGET_PDB_FILE:${_pdb_target}>
                DESTINATION bin
                CONFIGURATIONS Debug
                OPTIONAL)
    endforeach()
endif()

install(FILES 
    ${INCLUDE_ROOT}/slideio/converter/converterparameters.hpp 
    ${INCLUDE_ROOT}/slideio/converter/convertercallback.hpp 
    ${INCLUDE_ROOT}/slideio/converter/converter_def.hpp 
    ${INCLUDE_ROOT}/slideio/converter/converter.hpp
    DESTINATION include/slideio/converter)


install(FILES
    ${INCLUDE_ROOT}/slideio/core/slideio_core_def.hpp
    ${INCLUDE_ROOT}/slideio/core/levelinfo.hpp
    ${INCLUDE_ROOT}/slideio/core/metadata.hpp
    ${INCLUDE_ROOT}/slideio/core/rect.hpp
    ${INCLUDE_ROOT}/slideio/core/size.hpp
    ${INCLUDE_ROOT}/slideio/core/range.hpp
    ${INCLUDE_ROOT}/slideio/core/resolution.hpp
    ${INCLUDE_ROOT}/slideio/core/slideio_enums.hpp
    ${INCLUDE_ROOT}/slideio/core/slideio_structs.hpp
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
