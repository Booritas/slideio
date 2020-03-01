set(INCLUDE_ROOT ${CMAKE_SOURCE_DIR}/slideio)
set(BINDLIB_NAME slideio)

CONAN_MAIN()

set(BINDLIB_SOURCE 
   pybind.cpp
)
if (CMAKE_BUILD_TYPE STREQUAL "Debug")
   set(pybind11_DIR ${CONAN_PYBIND11_ROOT_DEBUG}/share/cmake/pybind11)
else()
   set(pybind11_DIR ${CONAN_PYBIND11_ROOT_RELEASE}/share/cmake/pybind11)
endif()
message("-------------pybind_dir: ${pybind11_DIR}"<<<<<<<<<<)
find_package(pybind11 REQUIRED)
pybind11_add_module(${BINDLIB_NAME} ${BINDLIB_SOURCE})
target_include_directories(${BINDLIB_NAME} PRIVATE ${INCLUDE_ROOT})
target_link_libraries(${BINDLIB_NAME} PRIVATE ${PROJECT_NAME})
