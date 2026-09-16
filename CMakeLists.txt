# 3.15 is not aspirational -- it is what this file already required.
# cmake_policy(SET CMP0091 NEW) two lines down names a policy CMake learned in
# 3.15, and setting a policy an older CMake does not know is a hard error, so
# every version below 3.15 failed here before reaching anything else. The
# declared 3.10 had been wrong for as long as that line has existed.
#
# Other floors this passes on the way, each already in use: add_compile_definitions
# (3.12), target_link_options and add_link_options (3.13, in
# HIDE_THIRD_PARTY_SYMBOLS and the MSVC release flags), and
# CMAKE_BUILD_RPATH_USE_ORIGIN (3.14).
#
# Raising the declaration also moves the policy defaults from 3.10 to 3.15. The
# one with a visible effect here is CMP0092: MSVC warning flags are no longer
# baked into CMAKE_CXX_FLAGS, so the default level drops from /W3 to the
# compiler default. Nothing in the build parses warning text, and the explicit
# /wd4275 /wd4251 suppressions are unaffected.
cmake_minimum_required(VERSION 3.15)
cmake_policy(SET CMP0057 NEW)
cmake_policy(SET CMP0091 NEW)

if(${CMAKE_VERSION} VERSION_GREATER_EQUAL "3.30")
    cmake_policy(SET CMP0167 NEW)
endif()

set(PROJECT_NAME slide_io)
set(projectVersion 2.10.0)

project(${PROJECT_NAME} VERSION ${projectVersion} LANGUAGES C CXX)
enable_testing()

# The version the library reports at runtime -- ImageDriverManager::getVersion()
# returns SLIDEIO_VERSION from src/slideio/slideio/slideio_def.hpp -- is a
# literal, and it had drifted: 2.9.2 against a 2.10.0 build. That was harmless
# while nobody shipped the binaries. It stops being harmless the moment a
# release pipeline stamps package names and SONAMEs with projectVersion, so
# configure refuses a build where the two disagree rather than producing a
# package that lies about what is inside it.
file(READ ${CMAKE_SOURCE_DIR}/src/slideio/slideio/slideio_def.hpp _slideio_def_hpp)
string(REGEX MATCH "SLIDEIO_VERSION[^\"]*\"([0-9]+[.][0-9]+[.][0-9]+)\""
       _slideio_version_match "${_slideio_def_hpp}")
if(NOT CMAKE_MATCH_1)
    message(FATAL_ERROR
        "Could not read SLIDEIO_VERSION out of src/slideio/slideio/slideio_def.hpp.")
elseif(NOT CMAKE_MATCH_1 VERSION_EQUAL projectVersion)
    message(FATAL_ERROR
        "Version mismatch: CMakeLists.txt says ${projectVersion}, but "
        "src/slideio/slideio/slideio_def.hpp says SLIDEIO_VERSION = "
        "\"${CMAKE_MATCH_1}\". Update the header.")
endif()
unset(_slideio_def_hpp)
unset(_slideio_version_match)

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
# The commas here used to make both of these no-ops: CMake splits set() on
# whitespace, so set(CMAKE_CXX_STANDARD,17) declared a variable actually named
# "CMAKE_CXX_STANDARD,17" and left the real one untouched. The build compiled as
# C++17 regardless, because the conan toolchain sets the same two variables from
# compiler.cppstd -- which meant a configure without that toolchain silently fell
# back to the compiler default and failed deep inside ICU and log.hpp on C++17
# constructs. Saying it here costs nothing (the value matches what the toolchain
# sets, so conan_modify_std_watch stays quiet) and makes the requirement the
# project own rather than something inherited by luck.
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_STANDARD 17)
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
    # Floor for the distributed arm64 build: every Apple Silicon Mac still
    # receiving security updates runs 12 or newer. Set here rather than in
    # conan/OSX/arm/* on purpose -- os.version is part of a conan package id, so
    # putting it in the profile would invalidate every cached binary and make CI
    # build opencv, dcmtk and icu from source. Only slideio's own objects need
    # the floor; linking them against dependencies built for an older target is
    # fine.
    if(NOT CMAKE_OSX_DEPLOYMENT_TARGET)
        set(CMAKE_OSX_DEPLOYMENT_TARGET "12.0" CACHE STRING "Minimum macOS version" FORCE)
    endif()
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
    # Release ships its PDBs as a separate artifact, so Release has to produce
    # them: /Zi writes the symbols, /DEBUG makes the linker keep them. /DEBUG
    # also turns off the two link-time optimisations MSVC otherwise applies to
    # a release build, so both are asked for again explicitly -- without them
    # the shipped binaries would be measurably larger than before for no reason.
    add_compile_options($<$<CONFIG:Release>:/Zi>)
    add_link_options($<$<CONFIG:Release>:/DEBUG>
                     $<$<CONFIG:Release>:/OPT:REF>
                     $<$<CONFIG:Release>:/OPT:ICF>)
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
# pole below spells its option the same way, so this one entry turns both off.
set(PACKAGE_TESTS OFF CACHE BOOL "Build the jpegxrcodec and pole tests" FORCE)
add_subdirectory(${JPEGXRCODEC_DIR} ${CMAKE_BINARY_DIR}/extern/jpegxrcodec)
# The codec exports no usage requirements of its own; consumers include it as
# <jxrcodec/jxrcodec.hpp>, so the repository root is the include directory.
target_include_directories(jxrcodec PUBLIC ${JPEGXRCODEC_DIR})

# pole: the OLE compound-file reader the zvi driver reads ZVI storages with,
# built from the extern/pole submodule instead of the pole/1.0.4@slideio/stable
# conan package. That recipe only cloned this repository at v1.0.4 and ran its
# CMakeLists, so the sources compiled here are the same ones. It needs nothing
# but the standard library, which is why it is added here rather than beside its
# consumers the way the ndpi forks are.
set(POLE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/extern/pole)
if(NOT EXISTS ${POLE_DIR}/CMakeLists.txt)
    message(FATAL_ERROR
        "extern/pole is empty. Run 'git submodule update --init' in "
        "${CMAKE_CURRENT_SOURCE_DIR} and configure again.")
endif()
add_subdirectory(${POLE_DIR} ${CMAKE_BINARY_DIR}/extern/pole)
# pole declares no usage requirements either -- its own sources reach their
# headers by relative path -- and its headers sit in includes/ while everything
# that consumes them says <pole/...>. That prefix is what the conan recipe
# produced, by copying includes/ into the package as include/pole; staging the
# same shape in the build tree keeps the include lines untouched. The copy
# happens at configure time, so a header edited in the submodule needs a
# re-configure to be seen -- the recipe needed a package rebuild.
set(POLE_INCLUDE_DIR ${CMAKE_BINARY_DIR}/extern/pole/include)
file(COPY ${POLE_DIR}/includes/ DESTINATION ${POLE_INCLUDE_DIR}/pole
    FILES_MATCHING PATTERN "*.hpp" PATTERN "*.h" PATTERN "*.hh")
target_include_directories(pole PUBLIC ${POLE_INCLUDE_DIR})
# pole points CMAKE_ARCHIVE_OUTPUT_DIRECTORY at ${CMAKE_BINARY_DIR}/install/lib,
# which used to be its own build tree and is now the directory install.py
# installs into. Keep the archive in the build tree where it belongs.
set_target_properties(pole PROPERTIES
    ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/extern/pole/lib)

add_subdirectory(src)

# ---------------------------------------------------------------------------
# Installation
# ---------------------------------------------------------------------------
#
# The installed tree is what every binary distribution is cut from, so the
# rules below are grouped into CPack components rather than written one target
# at a time:
#
#   Runtime       the shared libraries a consumer's program loads. All sixteen
#                 belong together in one directory: the format drivers are
#                 load-time linked into the slideio library (see
#                 src/slideio/slideio/CMakeLists.txt), not dlopen'd, and the
#                 installed RPATH is $ORIGIN.
#   Development   import libraries, public headers, CMake package config.
#   DebugSymbols  MSVC PDB files, shipped as an archive of their own.
#
# cmake-scripts/packaging.cmake turns those into a .zip, a .tar.gz or a pair of
# .deb files. A plain `cmake --install` is unaffected and still writes the
# whole tree.

set(SLIDEIO_PUBLIC_LIBS
    ${SLIDEIO_LIB_NAME}
    ${CORE_LIB_NAME}
    ${IMAGETOOLS_LIB_NAME}
    ${CONVERTER_LIB_NAME}
    ${TRANSFORMER_LIB_NAME}
)

set(SLIDEIO_DRIVER_LIBS
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

set(SLIDEIO_ALL_LIBS ${SLIDEIO_PUBLIC_LIBS} ${SLIDEIO_DRIVER_LIBS})

# A Debian package split into a runtime and a -dev half needs an SONAME, and
# there was none. It is major.minor and not major: 2.10 added virtual functions
# to CVScene, so a program built against 2.9 must not silently load 2.10, and
# libslideio2.9 and libslideio2.10 are separate packages that can coexist.
#
# Linux only, deliberately. On macOS, VERSION/SOVERSION would rename the
# libraries to lib<name>.2.10.dylib, and FIX_MACOS_RPATH above rewrites install
# names by matching the literal strings "lib<name>.dylib" held in
# NAME_TOOL_LIB_LIST. Those would stop matching, install_name_tool -change would
# quietly do nothing, and the macOS archive would ship binaries whose
# dependencies cannot be resolved -- with no error at build time. Windows has no
# SONAME concept at all. Nothing outside the .deb needs one, so nothing outside
# Linux gets one.
if(UNIX AND NOT APPLE)
    set_target_properties(${SLIDEIO_ALL_LIBS} PROPERTIES
        VERSION ${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}
        SOVERSION ${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR})
endif()

# Windows puts the DLLs in bin/ and the import libraries in lib/; everywhere
# else the shared objects are the runtime and go to lib/. Before this, Unix
# builds installed the libraries into both lib/ and bin/ -- two copies of every
# .so, which a system package must not carry.
#
# NAMELINK_COMPONENT is what keeps the minor-version packages co-installable.
# With VERSION/SOVERSION set, CMake emits libslideio.so.2.10 *and* an
# unversioned libslideio.so pointing at it, and the namelink follows the LIBRARY
# clause's component unless told otherwise. Left in Runtime, libslideio2.10 and
# a future libslideio2.11 would both own /usr/lib/libslideio.so -- a dpkg file
# conflict, and the end of the side-by-side installation the whole major.minor
# SONAME exists to allow. The unversioned link is a development file: it is what
# `-lslideio` resolves through, and nothing at runtime reads it.
install(TARGETS ${SLIDEIO_ALL_LIBS}
    RUNTIME DESTINATION bin COMPONENT Runtime
    LIBRARY DESTINATION lib COMPONENT Runtime NAMELINK_COMPONENT Development
    ARCHIVE DESTINATION lib COMPONENT Development
)

# Debug symbols. Release is what gets distributed, so it is the configuration
# that matters here; Debug keeps the behaviour it always had.
if(MSVC)
    foreach(_pdb_target ${SLIDEIO_ALL_LIBS})
        install(FILES $<TARGET_PDB_FILE:${_pdb_target}>
                DESTINATION bin
                CONFIGURATIONS Debug Release
                COMPONENT DebugSymbols
                OPTIONAL)
    endforeach()
endif()

install(FILES
    ${INCLUDE_ROOT}/slideio/converter/converterparameters.hpp
    ${INCLUDE_ROOT}/slideio/converter/convertercallback.hpp
    ${INCLUDE_ROOT}/slideio/converter/converter_def.hpp
    ${INCLUDE_ROOT}/slideio/converter/converter.hpp
    DESTINATION include/slideio/converter COMPONENT Development)

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
    ${INCLUDE_ROOT}/slideio/core/colorprofile.hpp
    DESTINATION include/slideio/core COMPONENT Development)

install(FILES
    ${INCLUDE_ROOT}/slideio/imagetools/encodeparameters.hpp
    DESTINATION include/slideio/imagetools COMPONENT Development)

install(FILES
    ${INCLUDE_ROOT}/slideio/slideio/slideio_def.hpp
    ${INCLUDE_ROOT}/slideio/slideio/slideio.hpp
    ${INCLUDE_ROOT}/slideio/slideio/slide.hpp
    ${INCLUDE_ROOT}/slideio/slideio/scene.hpp
    DESTINATION include/slideio/slideio COMPONENT Development)

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
    ${INCLUDE_ROOT}/slideio/transformer/colormanagementwrap.hpp
    ${INCLUDE_ROOT}/slideio/transformer/cannyfilterwrap.hpp
    ${INCLUDE_ROOT}/slideio/transformer/bilateralfilterwrap.hpp
    DESTINATION include/slideio/transformer COMPONENT Development)

# ---------------------------------------------------------------------------
# CMake package configuration
# ---------------------------------------------------------------------------
#
# Lets a consumer write find_package(slideio 2.10 REQUIRED) and link
# slideio::slideio. See cmake-scripts/slideioConfig.cmake.in for why the config
# is hand-written rather than generated by install(EXPORT).
include(CMakePackageConfigHelpers)

set(SLIDEIO_CMAKE_PACKAGE_DIR lib/cmake/slideio)

configure_package_config_file(
    ${CMAKE_SOURCE_DIR}/cmake-scripts/slideioConfig.cmake.in
    ${CMAKE_BINARY_DIR}/cmake-package/slideioConfig.cmake
    INSTALL_DESTINATION ${SLIDEIO_CMAKE_PACKAGE_DIR})

# SameMinorVersion is the honest compatibility rule: 2.10 broke ABI against 2.9,
# so a consumer asking for 2.10 must not be handed 2.11.
#
# It needs CMake 3.11 and NAMELINK_COMPONENT above needs 3.12, both comfortably
# under the 3.15 declared at the top of this file, so neither is guarded.
write_basic_package_version_file(
    ${CMAKE_BINARY_DIR}/cmake-package/slideioConfigVersion.cmake
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY SameMinorVersion)

install(FILES
    ${CMAKE_BINARY_DIR}/cmake-package/slideioConfig.cmake
    ${CMAKE_BINARY_DIR}/cmake-package/slideioConfigVersion.cmake
    DESTINATION ${SLIDEIO_CMAKE_PACKAGE_DIR} COMPONENT Development)

# ---------------------------------------------------------------------------
# Packaging
# ---------------------------------------------------------------------------

# Names the artifacts. Only x86_64 Windows, x86_64 Debian and arm64 macOS are
# distributed, but the tag is derived rather than hard-coded so a build on
# anything else is labelled for what it is instead of lying.
if(WIN32)
    set(SLIDEIO_PLATFORM_OS "windows")
elseif(APPLE)
    set(SLIDEIO_PLATFORM_OS "macos")
else()
    set(SLIDEIO_PLATFORM_OS "linux")
endif()

string(TOLOWER "${CMAKE_SYSTEM_PROCESSOR}" SLIDEIO_PLATFORM_ARCH)
if(SLIDEIO_PLATFORM_ARCH STREQUAL "amd64" OR SLIDEIO_PLATFORM_ARCH STREQUAL "x64")
    set(SLIDEIO_PLATFORM_ARCH "x86_64")
elseif(SLIDEIO_PLATFORM_ARCH STREQUAL "aarch64")
    set(SLIDEIO_PLATFORM_ARCH "arm64")
elseif(SLIDEIO_PLATFORM_ARCH STREQUAL "")
    set(SLIDEIO_PLATFORM_ARCH "unknown")
endif()

set(SLIDEIO_PLATFORM_TAG "${SLIDEIO_PLATFORM_OS}-${SLIDEIO_PLATFORM_ARCH}")

include(${CMAKE_SOURCE_DIR}/cmake-scripts/packaging.cmake)
