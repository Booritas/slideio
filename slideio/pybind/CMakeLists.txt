set(INCLUDE_ROOT ${CMAKE_SOURCE_DIR}/slideio)
set(BINDLIB_NAME slideio)

CONAN_MAIN()

set(BINDLIB_SOURCE 
   pybind.cpp
   pyglobals.hpp
   pyglobals.cpp
   pyslide.hpp
   pyslide.cpp
   pyscene.hpp
   pyscene.cpp
)

find_package (Python3 COMPONENTS Interpreter Development NumPy)


message(STATUS "Python: version=${Python3_VERSION}")
message(STATUS "Python interpreter=${Python3_EXECUTABLE}")
message(STATUS "Python include=${Python3_INCLUDE_DIRS}")
message(STATUS "Python libraries=${Python3_LIBRARIES}")
message(STATUS "Python library directories=${Python3_LIBRARY_DIRS}")
message(STATUS "Python NumPy include=${Python3_NumPy_INCLUDE_DIRS}")

if(NOT Python3_FOUND AND Python3_Development_FOUND)
    #   find_package() will not abort the build if anything's missing.
    string(JOIN "\n" errmsg
        "  Python3 and/or development libs not found."
        "  - Python3_FOUND=${Python3_FOUND}"
        "  - Python3_Development_FOUND=${Python3_Development_FOUND}"
        )
    message(FATAL_ERROR ${errmsg})
endif()


if (CMAKE_BUILD_TYPE STREQUAL "Debug")
   set(pybind11_DIR ${CONAN_PYBIND11_ROOT_DEBUG}/share/cmake/pybind11)
else()
   set(pybind11_DIR ${CONAN_PYBIND11_ROOT_RELEASE}/share/cmake/pybind11)
endif()
find_package(pybind11 REQUIRED)
pybind11_add_module(${BINDLIB_NAME} ${BINDLIB_SOURCE})

target_include_directories(${BINDLIB_NAME} PRIVATE ${INCLUDE_ROOT} ${Python3_INCLUDE_DIRS} ${Python3_NumPy_INCLUDE_DIRS})
target_include_directories(${BINDLIB_NAME} PRIVATE ${INCLUDE_ROOT})
target_link_libraries(${BINDLIB_NAME} PRIVATE ${PROJECT_NAME})

CONAN_CONFIGURE_INCLUDES(${BINDLIB_NAME})