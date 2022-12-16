set(INCLUDE_ROOT ${CMAKE_SOURCE_DIR}/src)
set(TEST_NAME slideio_tests)

CONAN_MAIN()

set(TEST_SOURCES
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
)

add_executable(${TEST_NAME} ${TEST_SOURCES})

target_include_directories(${TEST_NAME} PRIVATE ${INCLUDE_ROOT})
target_link_libraries(${TEST_NAME} ${SLIEIO_LIB_NAME} ${TEST_LIB_NAME} ${CONAN_LIB_DIRS_POLE_DEBUG}/libpole.a)


# Remove boost libraries from linking
message("list: ${CONAN_LIBS_RELEASE}")
LIST(FILTER CONAN_LIBS_RELEASE EXCLUDE REGEX "^(.)+boost(.)+")
LIST(FILTER CONAN_LIBS_DEBUG EXCLUDE REGEX "^(.)+boost(.)+")

CONAN_CONFIGURE(${TEST_NAME})

IF(WIN32)
   set_target_properties(${TEST_NAME} PROPERTIES LINK_FLAGS "/ignore:4099")
   target_compile_definitions(${TEST_NAME} PRIVATE _CRT_SECURE_NO_WARNINGS)
ENDIF(WIN32)


add_test(${TEST_NAME} ${TEST_DIR}/${TEST_NAME})

FIX_MACOS_RPATH(${TEST_NAME})
