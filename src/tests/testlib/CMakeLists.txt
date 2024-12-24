
set(LIBRARY_NAME ${TEST_LIB_NAME})
set(CMAKE_PREFIX_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
set(CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

set(SOURCE_FILES 
   ${SOURCE_FILES}
   testtiler.cpp
   testtiler.hpp
   testtools.cpp
   testtools.hpp
   testscene.hpp
   testscene.cpp
   )


add_library(${LIBRARY_NAME} STATIC ${SOURCE_FILES})
set_target_properties(${LIBRARY_NAME} PROPERTIES OUTPUT_NAME "${LIBRARY_NAME}${LIB_NAME_SUFFIX}")

find_package(Boost)
find_package(SQLite3)
find_package(glog)
find_package(GTest)
find_package(OpenCV)
find_package(PNG)
find_package(ZLIB)

target_link_libraries(${LIBRARY_NAME} 
   boost::boost
   SQLite::SQLite3
   glog::glog
   gtest::gtest
   opencv::opencv
   PNG::PNG
   ZLIB::ZLIB
)

IF(WIN32)
   target_link_libraries(${LIBRARY_NAME} Shlwapi)
   set_target_properties(${LIBRARY_NAME} PROPERTIES LINK_FLAGS "/ignore:4099")
   target_compile_definitions(${LIBRARY_NAME} PRIVATE _CRT_SECURE_NO_WARNINGS)
ENDIF(WIN32)

target_link_libraries(${LIBRARY_NAME} ${CORE_LIB_NAME})
set_target_properties(${LIBRARY_NAME} PROPERTIES LINKER_LANGUAGE CXX)

