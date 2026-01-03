set(INCLUDE_ROOT ${CMAKE_SOURCE_DIR}/src)
set(TEST_NAME slideio_ometiff_tests)

set(TEST_SOURCES
  test_ometiff_driver.cpp
  test_otdimensions.cpp
  test_ometiffdata.cpp
  test_ottools.cpp
)

add_executable(${TEST_NAME} ${TEST_SOURCES})

target_include_directories(${TEST_NAME} PRIVATE ${INCLUDE_ROOT})


find_package(GTest)
find_package(glog)
find_package(OpenCV)

target_link_libraries(${TEST_NAME} 
   gtest::gtest
   glog::glog
   opencv::opencv
)

target_link_libraries(${TEST_NAME} 
    ${CORE_LIB_NAME}
    ${OMETIFF_DRIVER_NAME}
    ${TEST_LIB_NAME}
    ${IMAGETOOLS_LIB_NAME}
    ${SLIDEIO_LIB_NAME}
  )


IF(WIN32)
   set_target_properties(${TEST_NAME} PROPERTIES LINK_FLAGS "/ignore:4099")
   target_compile_definitions(${TEST_NAME} PRIVATE _CRT_SECURE_NO_WARNINGS)
ENDIF(WIN32)


add_test(${TEST_NAME} ${TEST_DIR}/${TEST_NAME})

FIX_MACOS_RPATH(${TEST_NAME})
