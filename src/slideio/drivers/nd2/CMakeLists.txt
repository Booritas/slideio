set(LIBRARY_NAME ${ND2_DRIVER_NAME})
set(CMAKE_PREFIX_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
set(CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

set(SOURCE_FILES
   ${SOURCE_FILES}
   ${CMAKE_CURRENT_SOURCE_DIR}/nd2_api_def.hpp
   ${CMAKE_CURRENT_SOURCE_DIR}/nd2chunkfile.hpp
   ${CMAKE_CURRENT_SOURCE_DIR}/nd2chunkfile.cpp
   ${CMAKE_CURRENT_SOURCE_DIR}/nd2variant.hpp
   ${CMAKE_CURRENT_SOURCE_DIR}/nd2variant.cpp
   ${CMAKE_CURRENT_SOURCE_DIR}/nd2attributes.hpp
   ${CMAKE_CURRENT_SOURCE_DIR}/nd2attributes.cpp
   ${CMAKE_CURRENT_SOURCE_DIR}/nd2experiment.hpp
   ${CMAKE_CURRENT_SOURCE_DIR}/nd2experiment.cpp
   ${CMAKE_CURRENT_SOURCE_DIR}/nd2internal.hpp
   ${CMAKE_CURRENT_SOURCE_DIR}/nd2imageinfo.hpp
   ${CMAKE_CURRENT_SOURCE_DIR}/nd2imageinfo.cpp
   ${CMAKE_CURRENT_SOURCE_DIR}/nd2framesource.hpp
   ${CMAKE_CURRENT_SOURCE_DIR}/nd2framesource.cpp
   ${CMAKE_CURRENT_SOURCE_DIR}/nd2imagedriver.hpp
   ${CMAKE_CURRENT_SOURCE_DIR}/nd2imagedriver.cpp
   ${CMAKE_CURRENT_SOURCE_DIR}/nd2slide.hpp
   ${CMAKE_CURRENT_SOURCE_DIR}/nd2slide.cpp
   ${CMAKE_CURRENT_SOURCE_DIR}/nd2scene.hpp
   ${CMAKE_CURRENT_SOURCE_DIR}/nd2scene.cpp
   )

add_library(${LIBRARY_NAME} SHARED ${SOURCE_FILES})
HIDE_THIRD_PARTY_SYMBOLS(${LIBRARY_NAME})
set_target_properties(${LIBRARY_NAME} PROPERTIES OUTPUT_NAME "${LIBRARY_NAME}${LIB_NAME_SUFFIX}")

find_package(OpenCV)
find_package(tinyxml2)
find_package(ZLIB)
find_package(nlohmann_json REQUIRED)

target_link_libraries(${LIBRARY_NAME}
   opencv::opencv
   tinyxml2::tinyxml2
   ZLIB::ZLIB
   nlohmann_json::nlohmann_json
)

SOURCE_GROUP_BY_FOLDER(SOURCE_FILES)

target_link_libraries(${LIBRARY_NAME} ${CORE_LIB_NAME} ${IMAGETOOLS_LIB_NAME})

IF(WIN32)
   set_target_properties(${LIBRARY_NAME} PROPERTIES LINK_FLAGS "/ignore:4099")
   target_compile_definitions(${LIBRARY_NAME} PRIVATE SLIDEIO_ND2_API _CRT_SECURE_NO_WARNINGS)
ENDIF(WIN32)

set_target_properties(${LIBRARY_NAME} PROPERTIES LINKER_LANGUAGE CXX)

if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    target_link_options(${LIBRARY_NAME} PRIVATE "-Wl,-Bsymbolic")
endif()
