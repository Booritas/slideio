set(INCLUDE_ROOT ${CMAKE_SOURCE_DIR}/src)
set(TEST_NAME converter)

CONAN_MAIN()

set(SOURCE_FILES 
   converter.cpp
)

add_executable(${TEST_NAME} ${SOURCE_FILES})

IF(WIN32)
   #target_link_libraries(${TEST_NAME} Shlwapi)
   set_target_properties(${TEST_NAME} PROPERTIES LINK_FLAGS "/ignore:4099")
   target_compile_definitions(${TEST_NAME} PRIVATE _CRT_SECURE_NO_WARNINGS)
ENDIF(WIN32)

target_compile_definitions(${TEST_NAME} PRIVATE OPJ_HAVE_LIBTIFF OPJ_HAVE_LIBPNG)
set_target_properties(${TEST_NAME} PROPERTIES LINKER_LANGUAGE CXX)
target_include_directories(${TEST_NAME} PRIVATE ${INCLUDE_ROOT})
target_link_libraries(${TEST_NAME} PRIVATE
   ${CORE_LIB_NAME}
   ${SLIDEIO_LIB_NAME}
   ${IMAGETOOLS_LIB_NAME}
   ${TEST_LIB_NAME}
   ${BASE_LIB_NAME}
   ${CONVERTER_LIB_NAME}
)

FIX_MACOS_RPATH(${TEST_NAME})