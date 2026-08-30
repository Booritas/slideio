set(LIBRARY_NAME ${NDPI_DRIVER_NAME})
set(CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
set(CMAKE_PREFIX_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

# ---------------------------------------------------------------------------
# The NDPI forks of libjpeg-turbo and libtiff, built from submodules.
#
# These were the ndpi-libjpeg-turbo/2.1.2@slideio/stable and
# ndpi-libtiff/4.3.0@slideio/stable conan packages. Both recipes only fetched
# these repositories and ran their CMakeLists, so the sources compiled here are
# the same ones, pinned at the same v2.1.2 and v4.3.0 tags. What changes is that
# a fix to either is testable in place instead of needing a package rebuild and
# upload first.
#
# They have to move together. libtiff calls libjpeg and so does this driver
# (ndpitifftools.cpp includes jpeglib.h directly), and the fork is configured
# WITH_JPEG8 and WITH_MEM_SRCDST, both of which change the size of
# jpeg_decompress_struct. Two libjpeg builds that disagree on that surface at
# runtime as "JPEG parameter struct mismatch", not as a link error -- which is
# what the old ndpi-libtiff force-build in install.py existed to avoid.
#
# Added here rather than in the root CMakeLists, where jpegxrcodec lives,
# because ndpi-tiff needs zlib, libdeflate, xz_utils, jbig, zstd and libwebp,
# and those are on CMAKE_PREFIX_PATH only in this directory -- this is where
# this driver's conan files are generated. This driver is their only consumer.
# ---------------------------------------------------------------------------
set(NDPI_JPEG_DIR ${CMAKE_SOURCE_DIR}/extern/ndpi-libjpeg-turbo)
set(NDPI_TIFF_DIR ${CMAKE_SOURCE_DIR}/extern/ndpi-tiff)
foreach(_ndpi_extern ${NDPI_JPEG_DIR} ${NDPI_TIFF_DIR})
   if(NOT EXISTS ${_ndpi_extern}/CMakeLists.txt)
      message(FATAL_ERROR
         "${_ndpi_extern} is empty. Run 'git submodule update --init' in "
         "${CMAKE_SOURCE_DIR} and configure again.")
   endif()
endforeach()

# Mirrors the ndpi-libjpeg-turbo recipe's toolchain variables. These are not
# free to drift: see the struct-layout note above.
set(ENABLE_STATIC ON CACHE BOOL "Build the static libjpeg-turbo" FORCE)
set(ENABLE_SHARED OFF CACHE BOOL "Build the shared libjpeg-turbo" FORCE)
set(WITH_SIMD ON CACHE BOOL "Build libjpeg-turbo SIMD extensions" FORCE)
set(WITH_ARITH_ENC ON CACHE BOOL "libjpeg-turbo arithmetic encoding" FORCE)
set(WITH_ARITH_DEC ON CACHE BOOL "libjpeg-turbo arithmetic decoding" FORCE)
set(WITH_JPEG7 ON CACHE BOOL "libjpeg v7 API/ABI emulation" FORCE)
set(WITH_JPEG8 ON CACHE BOOL "libjpeg v8 API/ABI emulation" FORCE)
set(WITH_MEM_SRCDST ON CACHE BOOL "In-memory source/destination managers" FORCE)
set(WITH_TURBOJPEG ON CACHE BOOL "Build the TurboJPEG API library" FORCE)
set(WITH_JAVA OFF CACHE BOOL "Build the libjpeg-turbo Java wrapper" FORCE)
set(WITH_12BIT OFF CACHE BOOL "12-bit JPEG samples" FORCE)
# EXCLUDE_FROM_ALL: only what this driver links gets built. That keeps the
# turbojpeg library and the cjpeg/djpeg/jpegtran/tjbench programs -- which the
# conan recipe deleted after installing -- out of the build entirely.
add_subdirectory(${NDPI_JPEG_DIR} ${CMAKE_BINARY_DIR}/extern/ndpi-libjpeg-turbo EXCLUDE_FROM_ALL)

# libjpeg-turbo uses directory-scoped include_directories(), so jpeg-static
# carries no usage requirements of its own, and jconfig.h is generated into the
# binary directory rather than shipped in the source tree.
set(NDPI_JPEG_INCLUDE_DIRS
   ${NDPI_JPEG_DIR}
   ${CMAKE_BINARY_DIR}/extern/ndpi-libjpeg-turbo)
# BUILD_INTERFACE because libjpeg-turbo declares install rules for jpeg-static:
# without it CMake rejects source- and build-tree paths in a property that would
# be exported. Nothing here is exported -- the archive is linked into this
# driver and goes no further -- but the check does not know that.
target_include_directories(jpeg-static PUBLIC
   $<BUILD_INTERFACE:${NDPI_JPEG_DIR}>
   $<BUILD_INTERFACE:${CMAKE_BINARY_DIR}/extern/ndpi-libjpeg-turbo>)

# Shims that give libtiff the imported-target names it links (Deflate::Deflate,
# JBIG::JBIG, ZSTD::ZSTD, WebP::WebP) from the ones conan actually publishes,
# and resolve find_package(JPEG) to jpeg-static above. This replaces the
# recipe's 4.3.0-0001-cmake-dependencies.patch, which a pristine submodule
# cannot carry. Prepended because ndpi-tiff appends its own cmake/ directory.
set(CMAKE_MODULE_PATH
   ${CMAKE_SOURCE_DIR}/cmake-scripts/ndpi-tiff-deps
   ${CMAKE_MODULE_PATH})

# Load the three conan packages libtiff needs that reach this driver only
# through conan's config mode, so their targets and variables exist in this
# directory -- an ancestor of ndpi-tiff and its libtiff subdirectory -- rather
# than being resolved somewhere further down.
find_package(libdeflate CONFIG REQUIRED)
find_package(jbig CONFIG REQUIRED)
find_package(zstd CONFIG REQUIRED)

# ...and hand the shims the header paths, because the finds above do not supply
# them. These three packages reach the ndpi driver only through conan's config
# mode, and there <pkg>_INCLUDE_DIRS_<CONFIG> arrives empty while everything
# else about the package is intact: <pkg>_LIBS, <pkg>_LIB_DIRS and the target's
# INTERFACE_LINK_LIBRARIES all carry the right values, so linking was never the
# problem -- tif_zip.c simply could not find libdeflate.h. <pkg>_PACKAGE_FOLDER
# is populated, so the include directory is derived from it, per config, the
# same way conan's own data file computes it.
foreach(_pkg libdeflate jbig zstd)
   string(TOUPPER ${_pkg} _PKG)
   set(_inc "")
   if(${_pkg}_PACKAGE_FOLDER_RELEASE)
      set(_inc "${_inc}$<$<NOT:$<CONFIG:Debug>>:${${_pkg}_PACKAGE_FOLDER_RELEASE}/include>")
   endif()
   if(${_pkg}_PACKAGE_FOLDER_DEBUG)
      set(_inc "${_inc}$<$<CONFIG:Debug>:${${_pkg}_PACKAGE_FOLDER_DEBUG}/include>")
   endif()
   if(NOT _inc)
      message(FATAL_ERROR
         "Cannot locate the ${_pkg} headers: neither ${_pkg}_PACKAGE_FOLDER_RELEASE "
         "nor ${_pkg}_PACKAGE_FOLDER_DEBUG is set after find_package(${_pkg} CONFIG).")
   endif()
   set(NDPI_TIFF_${_PKG}_INCLUDE "${_inc}")
endforeach()
# libtiff calls this one find_package(Deflate), not find_package(libdeflate).
set(NDPI_TIFF_DEFLATE_INCLUDE ${NDPI_TIFF_LIBDEFLATE_INCLUDE})

# Mirrors the ndpi-libtiff recipe's toolchain variables. BUILD_SHARED_LIBS is a
# plain variable, not a cache entry: ndpi-tiff sets it ON in the cache itself,
# and a normal variable shadows that here and in its subdirectories without
# leaking the setting to the rest of the build.
set(BUILD_SHARED_LIBS OFF)
# The conan toolchain turns on CMAKE_FIND_PACKAGE_PREFER_CONFIG, which would
# send libtiff's find_package(JBIG) / (ZSTD) / (Deflate) straight to conan's
# config packages -- and on a case-insensitive filesystem find_package(JBIG)
# happily matches jbig-config.cmake. That reports the package found while
# creating jbig::jbig rather than the JBIG::JBIG that libtiff/CMakeLists.txt
# links, so configuring fails on a target that does not exist. Module mode is
# what the shims need; this is a plain variable, so it applies to ndpi-tiff and
# nothing else.
set(CMAKE_FIND_PACKAGE_PREFER_CONFIG OFF)
# JBIGCodec.cmake and ZSTDCodec.cmake follow their find_package with a
# check_symbol_exists whose CMAKE_REQUIRED_LIBRARIES is the imported target the
# find produced. try_compile cannot re-export a hand-made IMPORTED target, so
# the check errors out instead of answering. The recipe's patch replaced both
# checks with a plain TRUE; seeding their result variables does the same from
# outside, because check_symbol_exists skips a question already answered.
set(HAVE_JBG_NEWLEN TRUE CACHE INTERNAL "jbig provides jbg_newlen")
set(ZSTD_RECENT_ENOUGH TRUE CACHE INTERNAL "zstd is >= 1.0")
set(zlib ON CACHE BOOL "libtiff: zlib support" FORCE)
set(libdeflate ON CACHE BOOL "libtiff: libdeflate support" FORCE)
set(lzma ON CACHE BOOL "libtiff: LZMA2 support" FORCE)
set(jbig ON CACHE BOOL "libtiff: JBIG support" FORCE)
set(zstd ON CACHE BOOL "libtiff: ZSTD support" FORCE)
set(webp ON CACHE BOOL "libtiff: WebP support" FORCE)
set(cxx ON CACHE BOOL "libtiff: C++ API" FORCE)
set(jpeg12 OFF CACHE BOOL "libtiff: 8/12-bit dual mode" FORCE)
set(lerc OFF CACHE BOOL "libtiff: LERC support" FORCE)
add_subdirectory(${NDPI_TIFF_DIR} ${CMAKE_BINARY_DIR}/extern/ndpi-tiff EXCLUDE_FROM_ALL)

# libtiff calls strcpy, sprintf and getenv, which MSVC's CRT deprecates in
# favour of its _s variants: 35 C4996 in a release build, not one of them a
# defect. Every slideio target that compiles on Windows already answers this
# the same way, this driver included; libtiff builds in-tree now, so it is
# silenced here rather than by whatever the conan recipe did. PRIVATE, so the
# definition reaches libtiff's own sources and nothing that links it.
IF(WIN32)
   target_compile_definitions(tiff PRIVATE _CRT_SECURE_NO_WARNINGS)
   if(TARGET tiffxx)
      target_compile_definitions(tiffxx PRIVATE _CRT_SECURE_NO_WARNINGS)
   endif()
ENDIF(WIN32)

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
   ${CMAKE_CURRENT_SOURCE_DIR}/ndpitiffkeeper.hpp
   ${CMAKE_CURRENT_SOURCE_DIR}/ndpitiffkeeper.cpp
   ${CMAKE_CURRENT_SOURCE_DIR}/ndpilibtiff.hpp
   ${CMAKE_CURRENT_SOURCE_DIR}/ndpitiffmessagehandler.hpp
   ${CMAKE_CURRENT_SOURCE_DIR}/ndpitiffmessagehandler.cpp
   )

add_library(${LIBRARY_NAME} SHARED ${SOURCE_FILES})
HIDE_THIRD_PARTY_SYMBOLS(${LIBRARY_NAME})
set_target_properties(${LIBRARY_NAME} PROPERTIES OUTPUT_NAME "${LIBRARY_NAME}${LIB_NAME_SUFFIX}")

find_package(SQLite3)
find_package(OpenCV)
find_package(ZLIB)
find_package(LibLZMA)
find_package(WebP)
find_package(lerc)
find_package(nlohmann_json REQUIRED)


target_link_libraries(${LIBRARY_NAME}
   SQLite::SQLite3
   opencv::opencv
   tiff
   ZLIB::ZLIB
   LibLZMA::LibLZMA
   jpeg-static
   libwebp::libwebp
   jxrcodec
   lerc::lerc
   nlohmann_json::nlohmann_json
)


SOURCE_GROUP_BY_FOLDER(SOURCE_FILES)

target_link_libraries(${LIBRARY_NAME}
   ${CORE_LIB_NAME}
)

IF(WIN32)
   target_link_libraries(${LIBRARY_NAME} Shlwapi)
   set_target_properties(${LIBRARY_NAME} PROPERTIES LINK_FLAGS "/ignore:4099")
   target_compile_definitions(${LIBRARY_NAME} PRIVATE SLIDEIO_NDPI_API _CRT_SECURE_NO_WARNINGS)
ENDIF(WIN32)

set_target_properties(${LIBRARY_NAME} PROPERTIES LINKER_LANGUAGE CXX)

# -Bsymbolic used to be applied here, for GNU only. HIDE_THIRD_PARTY_SYMBOLS now
# applies it to every ELF target that bundles libtiff/libjpeg, Clang included.
