set(INCLUDE_ROOT ${CMAKE_SOURCE_DIR}/src)
set(TEST_NAME slideio_lif_tests)
set(CMAKE_PREFIX_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
set(CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

# The fixtures are synthetic and committed in data/, so every suite here is corpus-free.
set(TEST_SOURCES
  test_lif_container.cpp
  test_lif_xml.cpp
  test_lif_layout.cpp
  test_lif_timestamps.cpp
)

add_executable(${TEST_NAME} ${TEST_SOURCES})

target_include_directories(${TEST_NAME} PRIVATE ${INCLUDE_ROOT})

find_package(GTest)
find_package(OpenCV)

target_link_libraries(${TEST_NAME}
  gtest::gtest
  opencv::opencv
)

target_link_libraries(${TEST_NAME}
    ${LIF_DRIVER_NAME}
    ${TEST_LIB_NAME}
    ${CORE_LIB_NAME}
    ${SLIDEIO_LIB_NAME}
    ${CONVERTER_LIB_NAME}
  )

target_compile_definitions(${TEST_NAME} PRIVATE LIF_TEST_DATA_DIR="${CMAKE_CURRENT_SOURCE_DIR}/data")

IF(WIN32)
   set_target_properties(${TEST_NAME} PROPERTIES LINK_FLAGS "/ignore:4099")
   target_compile_definitions(${TEST_NAME} PRIVATE _CRT_SECURE_NO_WARNINGS)
ENDIF(WIN32)

add_test(${TEST_NAME} ${TEST_DIR}/${TEST_NAME})

FIX_MACOS_RPATH(${TEST_NAME})
