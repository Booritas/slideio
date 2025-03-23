set(INCLUDE_ROOT ${CMAKE_SOURCE_DIR}/src)
set(TEST_NAME slideio_ometiff_tests)

set(TEST_SOURCES
  test_ometiff_driver.cpp
)

add_executable(${TEST_NAME} ${TEST_SOURCES})

target_include_directories(${TEST_NAME} PRIVATE ${INCLUDE_ROOT})


find_package(GTest)
# find_package(SQLite3)
find_package(glog)
find_package(OpenCV)
# find_package(TIFF)
# find_package(LibLZMA)
# find_package(JPEG)
# find_package(WebP)
# find_package(PNG)
# find_package(ZLIB)
# find_package(OpenJPEG)
# find_package(Iconv)
# find_package(tinyxml2)

target_link_libraries(${TEST_NAME} 
   gtest::gtest
#   SQLite::SQLite3
   glog::glog
   opencv::opencv
#   TIFF::TIFF
#   LibLZMA::LibLZMA
#   JPEG::JPEG
#   libwebp::libwebp
#   PNG::PNG
#   ZLIB::ZLIB
#   openjp2
#   Iconv::Iconv
#   tinyxml2::tinyxml2
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
