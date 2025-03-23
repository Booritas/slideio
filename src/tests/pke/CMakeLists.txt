set(INCLUDE_ROOT ${CMAKE_SOURCE_DIR}/src)
set(TEST_NAME slideio_pke_tests)
set(CMAKE_PREFIX_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
set(CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")



set(TEST_SOURCES
  test_pke_driver.cpp
)

add_executable(${TEST_NAME} ${TEST_SOURCES})

target_include_directories(${TEST_NAME} PRIVATE ${INCLUDE_ROOT})

find_package(glog)
find_package(GTest)
find_package(OpenCV)
find_package(TIFF)
find_package(ZLIB)
find_package(WebP)
find_package(tinyxml2)

target_link_libraries(${TEST_NAME}
  glog::glog
  gtest::gtest
  opencv::opencv
  TIFF::TIFF
  ZLIB::ZLIB
  libwebp::libwebp
  tinyxml2::tinyxml2
)

target_link_libraries(${TEST_NAME} 
    ${PKE_DRIVER_NAME}
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
