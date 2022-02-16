set(INCLUDE_ROOT ${CMAKE_SOURCE_DIR}/src)
set(BINDLIB_NAME slideiopybind)
set(CMAKE_BUILD_RPATH_USE_ORIGIN 1)

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

add_subdirectory(pybind11)

if (DEFINED ENV{PYTHON_LOCATION_IS_SET})
   message("Python location is set externaly")
   set(Python3_VERSION $ENV{Python3_VERSION})
   set(Python3_EXECUTABLE $ENV{Python3_EXECUTABLE})
   set(Python3_INCLUDE_DIRS $ENV{Python3_INCLUDE_DIRS})
   set(Python3_LIBRARY_DIRS $ENV{Python3_LIBRARY_DIRS})
   set(Python3_NumPy_INCLUDE_DIRS $ENV{Python3_NumPy_INCLUDE_DIRS})
else()
   message("Look for the python location")
   #find_package (Python3 COMPONENTS Interpreter PythonLibs NumPy)
endif()

message(STATUS "Python: version=${Python3_VERSION}")
message(STATUS "Python interpreter=${Python3_EXECUTABLE}")
message(STATUS "Python include=${Python3_INCLUDE_DIRS}")
message(STATUS "Python libraries=${Python3_LIBRARIES}")
message(STATUS "Python library directories=${Python3_LIBRARY_DIRS}")
message(STATUS "Python NumPy include=${Python3_NumPy_INCLUDE_DIRS}")

pybind11_add_module(${BINDLIB_NAME} ${BINDLIB_SOURCE})

target_include_directories(${BINDLIB_NAME} PRIVATE ${INCLUDE_ROOT} ${Python3_INCLUDE_DIRS} ${Python3_NumPy_INCLUDE_DIRS})
target_include_directories(${BINDLIB_NAME} PRIVATE ${INCLUDE_ROOT})
target_link_libraries(${BINDLIB_NAME} PRIVATE ${PROJECT_NAME})


CONAN_CONFIGURE_INCLUDES(${BINDLIB_NAME})

if (APPLE)
   add_custom_command(TARGET ${BINDLIB_NAME}
           POST_BUILD COMMAND
           ${CMAKE_INSTALL_NAME_TOOL} -change libslide_io.dylib @loader_path/libslide_io.dylib
           $<TARGET_FILE:${BINDLIB_NAME}>)
endif()