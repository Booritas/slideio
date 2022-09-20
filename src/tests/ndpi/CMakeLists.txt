set(INCLUDE_ROOT ${CMAKE_SOURCE_DIR}/src)
set(TEST_NAME slideio_ndpi_tests)

CONAN_MAIN()

set(TEST_SOURCES
  test_ndpitiff_tools.cpp
  test_ndpi_driver.cpp
)

add_executable(${TEST_NAME} ${TEST_SOURCES})

target_include_directories(${TEST_NAME} PRIVATE ${INCLUDE_ROOT})
target_link_libraries(${TEST_NAME} 
    ${CORE_LIB_NAME}
    ${NDPI_DRIVER_NAME}
    ${TEST_LIB_NAME}
  )

CONAN_CONFIGURE(${TEST_NAME})

IF(WIN32)
   set_target_properties(${TEST_NAME} PROPERTIES LINK_FLAGS "/ignore:4099")
   target_compile_definitions(${TEST_NAME} PRIVATE _CRT_SECURE_NO_WARNINGS)
ENDIF(WIN32)


add_test(${TEST_NAME} ${TEST_DIR}/${TEST_NAME})