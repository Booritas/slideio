set(LIBRARY_NAME ${CORE_LIB_NAME})
set(CMAKE_PREFIX_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
message(STATUS "CMAKE_PREFIX_PATH=${CMAKE_PREFIX_PATH}")

set(SOURCE_FILES 
   ${SOURCE_FILES}
   ${CMAKE_CURRENT_SOURCE_DIR}/imagedriver.hpp
   ${CMAKE_CURRENT_SOURCE_DIR}/imagedriver.cpp
   ${CMAKE_CURRENT_SOURCE_DIR}/cvscene.hpp
   ${CMAKE_CURRENT_SOURCE_DIR}/cvscene.cpp
   ${CMAKE_CURRENT_SOURCE_DIR}/cvslide.hpp
   ${CMAKE_CURRENT_SOURCE_DIR}/cvslide.cpp
   ${CMAKE_CURRENT_SOURCE_DIR}/cvstructs.hpp
   ${CMAKE_CURRENT_SOURCE_DIR}/cvsmallscene.hpp
   ${CMAKE_CURRENT_SOURCE_DIR}/cvsmallscene.cpp
   ${CMAKE_CURRENT_SOURCE_DIR}/refcounter.hpp
   ${CMAKE_CURRENT_SOURCE_DIR}/levelinfo.hpp
   )

add_subdirectory(tools)
add_library(${LIBRARY_NAME} SHARED ${SOURCE_FILES})

find_package(Boost REQUIRED)
find_package(glog REQUIRED)
find_package(SQLITE3 REQUIRED)
find_package(OpenCV REQUIRED)
find_package(ZLIB REQUIRED)
find_package(Tinyxml2 REQUIRED)

target_link_libraries(${LIBRARY_NAME} boost::boost)
target_link_libraries(${LIBRARY_NAME} glog::glog)
target_link_libraries(${LIBRARY_NAME} SQLite::SQLite3)
target_link_libraries(${LIBRARY_NAME} opencv::opencv)
target_link_libraries(${LIBRARY_NAME} ZLIB::ZLIB)
target_link_libraries(${LIBRARY_NAME} tinyxml2::tinyxml2)


SOURCE_GROUP_BY_FOLDER(SOURCE_FILES)

IF(WIN32)
   target_link_libraries(${LIBRARY_NAME} Shlwapi)
   set_target_properties(${LIBRARY_NAME} PROPERTIES LINK_FLAGS "/ignore:4099")
   target_compile_definitions(${LIBRARY_NAME} PRIVATE SLIDEIO_CORE _CRT_SECURE_NO_WARNINGS)
ENDIF(WIN32)

set_target_properties(${LIBRARY_NAME} PROPERTIES LINKER_LANGUAGE CXX)
target_link_libraries(${LIBRARY_NAME} 
   ${IMAGETOOLS_LIB_NAME}
   ${BASE_LIB_NAME}
)

#set_target_properties(${LIBRARY_NAME} PROPERTIES CXX_VISIBILITY_PRESET hidden)