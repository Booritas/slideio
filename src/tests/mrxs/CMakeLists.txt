set(INCLUDE_ROOT ${CMAKE_SOURCE_DIR}/src)
set(TEST_NAME slideio_mrxs_tests)
set(CMAKE_PREFIX_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
set(CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

# Suites named MRXSCorpus* read the image corpus under SLIDEIO_IMAGES_PATH. Every other
# suite reads only the committed fixtures in data/, so CI runs this executable with
# --gtest_filter=-MRXSCorpus* and needs no corpus.
set(TEST_SOURCES
  test_mrxs_bundle.cpp
)

add_executable(${TEST_NAME} ${TEST_SOURCES})

target_include_directories(${TEST_NAME} PRIVATE ${INCLUDE_ROOT})
target_compile_definitions(${TEST_NAME} PRIVATE MRXS_TEST_DATA_DIR="${CMAKE_CURRENT_SOURCE_DIR}/data")

find_package(GTest)
find_package(OpenCV)

target_link_libraries(${TEST_NAME}
  gtest::gtest
  opencv::opencv
)

target_link_libraries(${TEST_NAME}
    ${MRXS_DRIVER_NAME}
    ${TEST_LIB_NAME}
    ${CORE_LIB_NAME}
    ${SLIDEIO_LIB_NAME}
  )

IF(WIN32)
   set_target_properties(${TEST_NAME} PROPERTIES LINK_FLAGS "/ignore:4099")
   target_compile_definitions(${TEST_NAME} PRIVATE _CRT_SECURE_NO_WARNINGS)
ENDIF(WIN32)

add_test(${TEST_NAME} ${TEST_DIR}/${TEST_NAME})

FIX_MACOS_RPATH(${TEST_NAME})
