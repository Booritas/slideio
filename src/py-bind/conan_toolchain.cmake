

# Conan automatically generated toolchain file
# DO NOT EDIT MANUALLY, it will be overwritten

# Avoid including toolchain file several times (bad if appending to variables like
#   CMAKE_CXX_FLAGS. See https://github.com/android/ndk/issues/323
include_guard()

message(STATUS "Using Conan toolchain: ${CMAKE_CURRENT_LIST_FILE}")

if(${CMAKE_VERSION} VERSION_LESS "3.15")
    message(FATAL_ERROR "The 'CMakeToolchain' generator only works with CMake >= 3.15")
endif()




########## generic_system block #############
# Definition of system, platform and toolset
#############################################


set(CMAKE_GENERATOR_PLATFORM "x64" CACHE STRING "" FORCE)

message(STATUS "Conan toolchain: CMAKE_GENERATOR_TOOLSET=v143")
set(CMAKE_GENERATOR_TOOLSET "v143" CACHE STRING "" FORCE)






# Definition of VS runtime, defined from build_type, compiler.runtime, compiler.runtime_type
cmake_policy(GET CMP0091 POLICY_CMP0091)
if(NOT "${POLICY_CMP0091}" STREQUAL NEW)
    message(FATAL_ERROR "The CMake policy CMP0091 must be NEW, but is '${POLICY_CMP0091}'")
endif()
set(CMAKE_MSVC_RUNTIME_LIBRARY "$<$<CONFIG:Release>:MultiThreadedDLL>")

string(APPEND CONAN_CXX_FLAGS " /MP24")
string(APPEND CONAN_C_FLAGS " /MP24")

# Conan conf flags start: Release
# Conan conf flags end

foreach(config ${CMAKE_CONFIGURATION_TYPES})
    string(TOUPPER ${config} config)
    if(DEFINED CONAN_CXX_FLAGS_${config})
      string(APPEND CMAKE_CXX_FLAGS_${config}_INIT " ${CONAN_CXX_FLAGS_${config}}")
    endif()
    if(DEFINED CONAN_C_FLAGS_${config})
      string(APPEND CMAKE_C_FLAGS_${config}_INIT " ${CONAN_C_FLAGS_${config}}")
    endif()
    if(DEFINED CONAN_SHARED_LINKER_FLAGS_${config})
      string(APPEND CMAKE_SHARED_LINKER_FLAGS_${config}_INIT " ${CONAN_SHARED_LINKER_FLAGS_${config}}")
    endif()
    if(DEFINED CONAN_EXE_LINKER_FLAGS_${config})
      string(APPEND CMAKE_EXE_LINKER_FLAGS_${config}_INIT " ${CONAN_EXE_LINKER_FLAGS_${config}}")
    endif()
endforeach()

if(DEFINED CONAN_CXX_FLAGS)
  string(APPEND CMAKE_CXX_FLAGS_INIT " ${CONAN_CXX_FLAGS}")
endif()
if(DEFINED CONAN_C_FLAGS)
  string(APPEND CMAKE_C_FLAGS_INIT " ${CONAN_C_FLAGS}")
endif()
if(DEFINED CONAN_SHARED_LINKER_FLAGS)
  string(APPEND CMAKE_SHARED_LINKER_FLAGS_INIT " ${CONAN_SHARED_LINKER_FLAGS}")
endif()
if(DEFINED CONAN_EXE_LINKER_FLAGS)
  string(APPEND CMAKE_EXE_LINKER_FLAGS_INIT " ${CONAN_EXE_LINKER_FLAGS}")
endif()


get_property( _CMAKE_IN_TRY_COMPILE GLOBAL PROPERTY IN_TRY_COMPILE )
if(_CMAKE_IN_TRY_COMPILE)
    message(STATUS "Running toolchain IN_TRY_COMPILE")
    return()
endif()

set(CMAKE_FIND_PACKAGE_PREFER_CONFIG ON)

# Definition of CMAKE_MODULE_PATH
list(PREPEND CMAKE_MODULE_PATH "d:/conan2/p/b/proto3ca77e34a4607/p/lib/cmake/protobuf" "d:/conan2/p/openj58733c81470a1/p/lib/cmake" "d:/conan2/p/b/opensb2864f2baa776/p/lib/cmake")
# the generators folder (where conan generates files, like this toolchain)
list(PREPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_LIST_DIR})

# Definition of CMAKE_PREFIX_PATH, CMAKE_XXXXX_PATH
# The explicitly defined "builddirs" of "host" context dependencies must be in PREFIX_PATH
list(PREPEND CMAKE_PREFIX_PATH "d:/conan2/p/b/proto3ca77e34a4607/p/lib/cmake/protobuf" "d:/conan2/p/openj58733c81470a1/p/lib/cmake" "d:/conan2/p/b/opensb2864f2baa776/p/lib/cmake")
# The Conan local "generators" folder, where this toolchain is saved.
list(PREPEND CMAKE_PREFIX_PATH ${CMAKE_CURRENT_LIST_DIR} )
list(PREPEND CMAKE_LIBRARY_PATH "d:/conan2/p/b/boost9d1689bcb86a0/p/lib" "d:/conan2/p/sqlit04526517e6ec4/p/lib" "d:/conan2/p/b/libxm80062ab712549/p/lib" "d:/conan2/p/b/glogcdad48a570a74/p/lib" "d:/conan2/p/b/gflag184db53e2f156/p/lib" "d:/conan2/p/b/openc9581dd0499250/p/lib" "d:/conan2/p/b/proto3ca77e34a4607/p/lib" "d:/conan2/p/b/ade2b2758f6b9ee9/p/lib" "d:/conan2/p/jaspe98f9964133b73/p/lib" "d:/conan2/p/b/openeec973fea49a8f/p/lib" "d:/conan2/p/b/imatha9ba833973845/p/lib" "d:/conan2/p/b/libti22554a7cdf7cb/p/lib" "d:/conan2/p/libde712e34aa8080e/p/lib" "d:/conan2/p/libjp0a77fd78bf747/p/lib" "d:/conan2/p/jbigfe9119c355d4c/p/lib" "d:/conan2/p/zstdf1d2b6566f015/p/lib" "d:/conan2/p/quirc4a07504dcabda/p/lib" "d:/conan2/p/b/ffmpe3433691955182/p/lib" "d:/conan2/p/xz_utc437dae9165b5/p/lib" "d:/conan2/p/libicb4e788959979c/p/lib" "d:/conan2/p/b/freet1ddbef27f1297/p/lib" "d:/conan2/p/b/libpn195183b04b4b7/p/lib" "d:/conan2/p/bzip2e06444d88ab4f/p/lib" "d:/conan2/p/brotl79757a5cae055/p/lib" "d:/conan2/p/openj58733c81470a1/p/lib" "d:/conan2/p/b/openhfee5d7ccb96aa/p/lib" "d:/conan2/p/vorbib3b87ba3196b8/p/lib" "d:/conan2/p/ogg0603e0d7ed2e4/p/lib" "d:/conan2/p/opus8462564dcd469/p/lib" "d:/conan2/p/libx2023aa9caac2a2/p/lib" "d:/conan2/p/b/libx28b5e58e0c9446/p/lib" "d:/conan2/p/b/libvp2df0ace6f53a4/p/lib" "d:/conan2/p/libmpb8490c605ee04/p/lib" "d:/conan2/p/b/libfd3386b616f66a0/p/lib" "d:/conan2/p/libweb2503c6aa238e/p/lib" "d:/conan2/p/b/opensb2864f2baa776/p/lib" "d:/conan2/p/libao1ef0c9c4f4b07/p/lib" "d:/conan2/p/dav1d9fbc52b90fc14/p/lib" "d:/conan2/p/zlib4b49f8b2fd4ff/p/lib")
list(PREPEND CMAKE_INCLUDE_PATH "d:/conan2/p/b/boost9d1689bcb86a0/p/include" "d:/conan2/p/sqlit04526517e6ec4/p/include" "d:/conan2/p/b/libxm80062ab712549/p/include" "d:/conan2/p/b/libxm80062ab712549/p/include/libxml2" "d:/conan2/p/b/glogcdad48a570a74/p/include" "d:/conan2/p/b/gflag184db53e2f156/p/include" "d:/conan2/p/b/openc9581dd0499250/p/include" "d:/conan2/p/eigen3d88c0279cc26/p/include/eigen3" "d:/conan2/p/b/proto3ca77e34a4607/p/include" "d:/conan2/p/b/ade2b2758f6b9ee9/p/include" "d:/conan2/p/jaspe98f9964133b73/p/include" "d:/conan2/p/b/openeec973fea49a8f/p/include" "d:/conan2/p/b/openeec973fea49a8f/p/include/OpenEXR" "d:/conan2/p/b/imatha9ba833973845/p/include" "d:/conan2/p/b/imatha9ba833973845/p/include/Imath" "d:/conan2/p/b/libti22554a7cdf7cb/p/include" "d:/conan2/p/libde712e34aa8080e/p/include" "d:/conan2/p/libjp0a77fd78bf747/p/include" "d:/conan2/p/jbigfe9119c355d4c/p/include" "d:/conan2/p/zstdf1d2b6566f015/p/include" "d:/conan2/p/quirc4a07504dcabda/p/include" "d:/conan2/p/b/ffmpe3433691955182/p/include" "d:/conan2/p/xz_utc437dae9165b5/p/include" "d:/conan2/p/libicb4e788959979c/p/include" "d:/conan2/p/b/freet1ddbef27f1297/p/include" "d:/conan2/p/b/freet1ddbef27f1297/p/include/freetype2" "d:/conan2/p/b/libpn195183b04b4b7/p/include" "d:/conan2/p/bzip2e06444d88ab4f/p/include" "d:/conan2/p/brotl79757a5cae055/p/include" "d:/conan2/p/brotl79757a5cae055/p/include/brotli" "d:/conan2/p/openj58733c81470a1/p/include" "d:/conan2/p/openj58733c81470a1/p/include/openjpeg-2.5" "d:/conan2/p/b/openhfee5d7ccb96aa/p/include" "d:/conan2/p/vorbib3b87ba3196b8/p/include" "d:/conan2/p/ogg0603e0d7ed2e4/p/include" "d:/conan2/p/opus8462564dcd469/p/include" "d:/conan2/p/opus8462564dcd469/p/include/opus" "d:/conan2/p/libx2023aa9caac2a2/p/include" "d:/conan2/p/b/libx28b5e58e0c9446/p/include" "d:/conan2/p/b/libvp2df0ace6f53a4/p/include" "d:/conan2/p/libmpb8490c605ee04/p/include" "d:/conan2/p/b/libfd3386b616f66a0/p/include" "d:/conan2/p/libweb2503c6aa238e/p/include" "d:/conan2/p/b/opensb2864f2baa776/p/include" "d:/conan2/p/libao1ef0c9c4f4b07/p/include" "d:/conan2/p/dav1d9fbc52b90fc14/p/include" "d:/conan2/p/zlib4b49f8b2fd4ff/p/include")



if (DEFINED ENV{PKG_CONFIG_PATH})
set(ENV{PKG_CONFIG_PATH} "${CMAKE_CURRENT_LIST_DIR};$ENV{PKG_CONFIG_PATH}")
else()
set(ENV{PKG_CONFIG_PATH} "${CMAKE_CURRENT_LIST_DIR};")
endif()




# Variables
# Variables  per configuration


# Preprocessor definitions
# Preprocessor definitions per configuration


if(CMAKE_POLICY_DEFAULT_CMP0091)  # Avoid unused and not-initialized warnings
endif()