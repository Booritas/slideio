set(LIBRARY_NAME ${BIF_DRIVER_NAME})
set(CMAKE_PREFIX_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
set(CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

set(SOURCE_FILES
   ${SOURCE_FILES}
   ${CMAKE_CURRENT_SOURCE_DIR}/bif_api_def.hpp
   ${CMAKE_CURRENT_SOURCE_DIR}/bifdescription.hpp
   ${CMAKE_CURRENT_SOURCE_DIR}/bifdescription.cpp
   )

add_library(${LIBRARY_NAME} SHARED ${SOURCE_FILES})
HIDE_THIRD_PARTY_SYMBOLS(${LIBRARY_NAME})
set_target_properties(${LIBRARY_NAME} PROPERTIES OUTPUT_NAME "${LIBRARY_NAME}${LIB_NAME_SUFFIX}")

find_package(OpenCV)
find_package(TIFF)
find_package(JPEG)
find_package(tinyxml2)

target_link_libraries(${LIBRARY_NAME}
   opencv::opencv
   TIFF::TIFF
   JPEG::JPEG
   tinyxml2::tinyxml2
)

SOURCE_GROUP_BY_FOLDER(SOURCE_FILES)

target_link_libraries(${LIBRARY_NAME} ${CORE_LIB_NAME} ${IMAGETOOLS_LIB_NAME})

IF(WIN32)
   set_target_properties(${LIBRARY_NAME} PROPERTIES LINK_FLAGS "/ignore:4099")
   target_compile_definitions(${LIBRARY_NAME} PRIVATE SLIDEIO_BIF_API _CRT_SECURE_NO_WARNINGS)
ENDIF(WIN32)

set_target_properties(${LIBRARY_NAME} PROPERTIES LINKER_LANGUAGE CXX)

if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    target_link_options(${LIBRARY_NAME} PRIVATE "-Wl,-Bsymbolic")
endif()
