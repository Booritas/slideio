set(INCLUDE_ROOT ${CMAKE_SOURCE_DIR}/src)
set(TEST_NAME slideio_converter_tests)

CONAN_MAIN()

set(TEST_SOURCES
  test_converter.cpp
  test_convertertools.cpp
)

add_executable(${TEST_NAME} ${TEST_SOURCES})

target_include_directories(${TEST_NAME} PRIVATE ${INCLUDE_ROOT})

CONAN_CONFIGURE(${TEST_NAME})

target_link_libraries(${TEST_NAME} 
    ${CORE_LIB_NAME}
    ${SLIDEIO_LIB_NAME}
    ${CONVERTER_LIB_NAME}
    ${TEST_LIB_NAME}
  )


IF(WIN32)
   set_target_properties(${TEST_NAME} PROPERTIES LINK_FLAGS "/ignore:4099")
   target_compile_definitions(${TEST_NAME} PRIVATE _CRT_SECURE_NO_WARNINGS)
ENDIF(WIN32)


add_test(${TEST_NAME} ${TEST_DIR}/${TEST_NAME})

FIX_MACOS_RPATH(${TEST_NAME})
