cmake_minimum_required(VERSION 3.2)

set(PROJECT_NAME slide_io)
set(projectVersion 0.5.0)

project(${PROJECT_NAME} VERSION ${projectVersion} LANGUAGES C CXX)
enable_testing()

set(INCLUDE_ROOT ${CMAKE_SOURCE_DIR}/src)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)
set(CMAKE_BUILD_RPATH_USE_ORIGIN 1)
set(CMAKE_CXX_STANDARD_REQUIRED, ON)
set(CMAKE_CXX_STANDARD,14)

IF(APPLE)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++14 -stdlib=libc++")
ELSEIF(UNIX)
    IF((CMAKE_CXX_COMPILER_ID STREQUAL "GNU") AND (CMAKE_CXX_COMPILER_VERSION VERSION_LESS "9"))
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++14")
        add_definitions(-D_GLIBCXX_USE_CXX11_ABI=0)
    ELSE()
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++14 -stdlib=libstdc++")
        add_definitions(-D_GLIBCXX_USE_CXX11_ABI=0)
    ENDIF()
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

IF(WIN32)
    set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/${CMAKE_BUILD_TYPE}/bin)
    set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/${CMAKE_BUILD_TYPE}/bin)
    set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/${CMAKE_BUILD_TYPE}/bin)
ELSE()
    set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
    set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
    set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
ENDIF(WIN32)

set(TEST_DIR ${CMAKE_RUNTIME_OUTPUT_DIRECTORY})

add_subdirectory(src)
