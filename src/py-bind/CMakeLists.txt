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

pybind11_add_module(${BINDLIB_NAME} SHARED ${BINDLIB_SOURCE})

target_include_directories(${BINDLIB_NAME} PRIVATE ${INCLUDE_ROOT} ${Python3_INCLUDE_DIRS} ${Python3_NumPy_INCLUDE_DIRS})
target_include_directories(${BINDLIB_NAME} PRIVATE ${INCLUDE_ROOT})
target_link_libraries(${BINDLIB_NAME} PRIVATE 
   ${SLIEIO_LIB_NAME}
   ${CORE_LIB_NAME}
   ${GDAL_DRIVER_NAME}
   ${SVS_DRIVER_NAME}
   ${AFI_DRIVER_NAME}
   ${CZI_DRIVER_NAME}
   ${DCM_DRIVER_NAME}
   ${SCN_DRIVER_NAME}
   ${ZVI_DRIVER_NAME}
   ${NDPI_DRIVER_NAME}
   )


CONAN_CONFIGURE_INCLUDES(${BINDLIB_NAME})

MACRO(FIX_MACOS_RPATH lib)
   add_custom_command(TARGET ${BINDLIB_NAME}
      POST_BUILD COMMAND
      ${CMAKE_INSTALL_NAME_TOOL} -change lib${lib}.dylib @loader_path/lib${lib}.dylib
      $<TARGET_FILE:${BINDLIB_NAME}>)
ENDMACRO(FIX_MACOS_RPATH)


if (APPLE)
   FIX_MACOS_RPATH(${SLIEIO_LIB_NAME})
   FIX_MACOS_RPATH(${CORE_LIB_NAME})
   FIX_MACOS_RPATH(${GDAL_DRIVER_NAME})
   FIX_MACOS_RPATH(${SVS_DRIVER_NAME})
   FIX_MACOS_RPATH(${AFI_DRIVER_NAME})
   FIX_MACOS_RPATH(${CZI_DRIVER_NAME})
   FIX_MACOS_RPATH(${DCM_DRIVER_NAME})
   FIX_MACOS_RPATH(${SCN_DRIVER_NAME})
   FIX_MACOS_RPATH(${ZVI_DRIVER_NAME})
   FIX_MACOS_RPATH(${NDPI_DRIVER_NAME})
endif()