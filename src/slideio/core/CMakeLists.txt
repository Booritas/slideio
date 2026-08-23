set(LIBRARY_NAME ${CORE_LIB_NAME})
set(CMAKE_PREFIX_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
set(CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
message(STATUS "CMAKE_PREFIX_PATH=${CMAKE_PREFIX_PATH}")

set(SOURCE_FILES 
   ${SOURCE_FILES}
   ${CMAKE_CURRENT_SOURCE_DIR}/imagedriver.hpp
   ${CMAKE_CURRENT_SOURCE_DIR}/imagedriver.cpp
   ${CMAKE_CURRENT_SOURCE_DIR}/cvscene.hpp
   ${CMAKE_CURRENT_SOURCE_DIR}/cvscene.cpp
   ${CMAKE_CURRENT_SOURCE_DIR}/cvslide.hpp
   ${CMAKE_CURRENT_SOURCE_DIR}/cvslide.cpp
   ${CMAKE_CURRENT_SOURCE_DIR}/cvsmallscene.hpp
   ${CMAKE_CURRENT_SOURCE_DIR}/cvsmallscene.cpp
   ${CMAKE_CURRENT_SOURCE_DIR}/refcounter.hpp
   ${CMAKE_CURRENT_SOURCE_DIR}/levelinfo.hpp
   ${CMAKE_CURRENT_SOURCE_DIR}/levelinfo.cpp
   ${CMAKE_CURRENT_SOURCE_DIR}/dimensions.hpp
   ${CMAKE_CURRENT_SOURCE_DIR}/metadata.hpp
   ${CMAKE_CURRENT_SOURCE_DIR}/metadata_internal.hpp
   ${CMAKE_CURRENT_SOURCE_DIR}/metadata.cpp
   ${CMAKE_CURRENT_SOURCE_DIR}/metadata_xml.cpp
   ${CMAKE_CURRENT_SOURCE_DIR}/exceptions.hpp
   ${CMAKE_CURRENT_SOURCE_DIR}/exceptions.cpp
   ${CMAKE_CURRENT_SOURCE_DIR}/slideio_enums.hpp
   ${CMAKE_CURRENT_SOURCE_DIR}/slideio_enums.cpp
   ${CMAKE_CURRENT_SOURCE_DIR}/log.hpp
   ${CMAKE_CURRENT_SOURCE_DIR}/log.cpp
   ${CMAKE_CURRENT_SOURCE_DIR}/logcontract.hpp
   ${CMAKE_CURRENT_SOURCE_DIR}/slideio_structs.hpp
   ${CMAKE_CURRENT_SOURCE_DIR}/resolution.hpp
   ${CMAKE_CURRENT_SOURCE_DIR}/rect.hpp
   ${CMAKE_CURRENT_SOURCE_DIR}/rect.inl
   ${CMAKE_CURRENT_SOURCE_DIR}/size.hpp
   ${CMAKE_CURRENT_SOURCE_DIR}/size.inl
   ${CMAKE_CURRENT_SOURCE_DIR}/range.hpp
   ${CMAKE_CURRENT_SOURCE_DIR}/range.inl
   ${CMAKE_CURRENT_SOURCE_DIR}/base.hpp
   )

add_subdirectory(tools)
add_library(${LIBRARY_NAME} SHARED ${SOURCE_FILES})
set_target_properties(${LIBRARY_NAME} PROPERTIES OUTPUT_NAME "${LIBRARY_NAME}${LIB_NAME_SUFFIX}")

find_package(SQLite3 REQUIRED)
find_package(OpenCV REQUIRED)
find_package(ZLIB REQUIRED)
find_package(Tinyxml2 REQUIRED)
find_package(nlohmann_json REQUIRED)
find_package(ICU)
find_package(spdlog REQUIRED)


target_link_libraries(${LIBRARY_NAME} PRIVATE SQLite::SQLite3)
target_link_libraries(${LIBRARY_NAME} PRIVATE opencv::opencv)
target_link_libraries(${LIBRARY_NAME} PRIVATE ZLIB::ZLIB)
target_link_libraries(${LIBRARY_NAME} PRIVATE tinyxml2::tinyxml2)
target_link_libraries(${LIBRARY_NAME} PRIVATE nlohmann_json::nlohmann_json)
target_link_libraries(${LIBRARY_NAME} PRIVATE icu::icu)
target_link_libraries(${LIBRARY_NAME} PRIVATE spdlog::spdlog)

SOURCE_GROUP_BY_FOLDER(SOURCE_FILES)

IF(WIN32)
   target_link_libraries(${LIBRARY_NAME} PRIVATE Shlwapi)
   set_target_properties(${LIBRARY_NAME} PROPERTIES LINK_FLAGS "/ignore:4099")
   target_compile_definitions(${LIBRARY_NAME} PRIVATE SLIDEIO_CORE _CRT_SECURE_NO_WARNINGS)
ENDIF(WIN32)

set_target_properties(${LIBRARY_NAME} PROPERTIES LINKER_LANGUAGE CXX)

#set_target_properties(${LIBRARY_NAME} PROPERTIES CXX_VISIBILITY_PRESET hidden)