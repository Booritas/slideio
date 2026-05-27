set(INCLUDE_ROOT ${CMAKE_SOURCE_DIR}/src)
set(TEST_NAME slideio_tests)
set(CMAKE_PREFIX_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
set(CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")



set(TEST_SOURCES
  test_color_tools.cpp
  test_imagedrivermanager.cpp
  test_czi_driver.cpp
  test_czi_tools.cpp
  test_scn_driver.cpp
  test_gdal_driver.cpp
  test_imagetools.cpp
  test_afi_driver.cpp
  test_svs_driver.cpp
  test_zvi_driver.cpp
  test_svs_tools.cpp
  test_dcm_driver.cpp
  test_tifftools.cpp
  test_zviutils.cpp
  test_tilecomposer.cpp
  test_cvtools.cpp
  test_dcmfile.cpp
  test_exception.cpp
  test_generic.cpp
  test_vsi_driver.cpp
  test_blocktiler.cpp
  test_tools.cpp
  test_similaritytools.cpp
  test_endian.cpp
  test_dimensions.cpp
  test_metadata.cpp
  test_metadata_builder.cpp
  test_tifffiles.cpp
  test_fiwrapper.cpp
  test_channel_attributes.cpp
  test_boundedqueue.cpp
  test_random_access_stream_contract.cpp
  test_filestream.cpp
  test_blockcache.cpp
  test_httpstream.cpp
  http_fixture/http_fixture.cpp
)

add_executable(${TEST_NAME} ${TEST_SOURCES})

find_package(SQLite3)
find_package(glog)
find_package(GTest)
find_package(OpenCV)
find_package(ZLIB)
find_package(tinyxml2)
find_package(pole)
find_package(nlohmann_json REQUIRED)
find_package(CURL REQUIRED)

# Python interpreter used by HttpFixture to launch the test HTTP server.
# Fall back to the known-good interpreter if CMake cannot find a real one
# (e.g. the Windows MS Store python.exe stub).
if(NOT DEFINED Python3_EXECUTABLE)
  if(EXISTS "C:/Users/Stanislav/anaconda3/python.exe")
    set(Python3_EXECUTABLE "C:/Users/Stanislav/anaconda3/python.exe" CACHE FILEPATH "")
  endif()
endif()
find_package(Python3 COMPONENTS Interpreter REQUIRED)
message(STATUS "slideio_tests: Python3_EXECUTABLE=${Python3_EXECUTABLE}")

target_link_libraries(${TEST_NAME}
  SQLite::SQLite3
  glog::glog
  gtest::gtest
  opencv::opencv
  ZLIB::ZLIB
  tinyxml2::tinyxml2
  pole::pole
  nlohmann_json::nlohmann_json
  CURL::libcurl
)

target_compile_definitions(${TEST_NAME} PRIVATE
    SLIDEIO_TEST_PYTHON="${Python3_EXECUTABLE}"
    SLIDEIO_TEST_HTTP_SERVER="${CMAKE_CURRENT_SOURCE_DIR}/http_fixture/test_http_server.py")

target_include_directories(${TEST_NAME} PRIVATE ${INCLUDE_ROOT})
target_link_libraries(${TEST_NAME} ${SLIDEIO_LIB_NAME} ${BASE_LIB_NAME} ${TEST_LIB_NAME})




IF(WIN32)
   set_target_properties(${TEST_NAME} PROPERTIES LINK_FLAGS "/ignore:4099")
   target_compile_definitions(${TEST_NAME} PRIVATE _CRT_SECURE_NO_WARNINGS)
ENDIF(WIN32)


add_test(${TEST_NAME} ${TEST_DIR}/${TEST_NAME})

FIX_MACOS_RPATH(${TEST_NAME})
