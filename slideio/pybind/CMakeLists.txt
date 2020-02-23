set(INCLUDE_ROOT ${CMAKE_SOURCE_DIR}/slideio)
set(BINDLIB_NAME pyslideio)

CONAN_MAIN()

set(BINDLIB_SOURCE 
   pybind.cpp
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

add_library(${BINDLIB_NAME} SHARED ${BINDLIB_SOURCE})
target_include_directories(${BINDLIB_NAME} PRIVATE ${INCLUDE_ROOT} ${Python3_INCLUDE_DIRS} ${Python3_NumPy_INCLUDE_DIRS})

CONAN_CONFIGURE(${BINDLIB_NAME})

target_link_libraries(${BINDLIB_NAME} ${PROJECT_NAME} ${Python3_LIBRARIES})
target_link_directories(${BINDLIB_NAME} PRIVATE ${Python3_LIBRARY_DIRS})

IF(WIN32)
   set_target_properties(${BINDLIB_NAME} PROPERTIES LINK_FLAGS "/ignore:4099")
   target_compile_definitions(${BINDLIB_NAME} PRIVATE _CRT_SECURE_NO_WARNINGS)
ENDIF(WIN32)