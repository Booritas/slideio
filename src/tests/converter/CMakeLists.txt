set(INCLUDE_ROOT ${CMAKE_SOURCE_DIR}/src)
set(TEST_NAME slideio_converter_tests)
set(CMAKE_PREFIX_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
set(CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")



set(TEST_SOURCES
  test_converter.cpp
  test_convertertools.cpp
  test_ometiff_converter.cpp
  test_tiffconverter.cpp
  test_tiffstructure.cpp
  test_converterparameters.cpp
  test_sourcescene_conversion.cpp
  test_converter_tool.cpp
  ${CMAKE_SOURCE_DIR}/src/tools/converter/sceneconverter.cpp
)

add_executable(${TEST_NAME} ${TEST_SOURCES})

target_include_directories(${TEST_NAME} PRIVATE ${INCLUDE_ROOT})
# The tool's open-options test converts a committed MRXS fixture.
target_compile_definitions(${TEST_NAME} PRIVATE MRXS_FIXTURE_DIR="${CMAKE_SOURCE_DIR}/src/tests/mrxs/data")


find_package(GTest)
find_package(OpenCV)
find_package(ZLIB)

target_link_libraries(${TEST_NAME} 
  gtest::gtest
  opencv::opencv
  ZLIB::ZLIB
)

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
