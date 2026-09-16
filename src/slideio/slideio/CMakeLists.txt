set(LIBRARY_NAME ${SLIDEIO_LIB_NAME})
set(CMAKE_PREFIX_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
set(CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

set(SOURCE_FILES 
   ${SOURCE_FILES}
   ${CMAKE_CURRENT_SOURCE_DIR}/imagedrivermanager.hpp
   ${CMAKE_CURRENT_SOURCE_DIR}/imagedrivermanager.cpp
   ${CMAKE_CURRENT_SOURCE_DIR}/slideio.cpp
   ${CMAKE_CURRENT_SOURCE_DIR}/slideio.hpp
   ${CMAKE_CURRENT_SOURCE_DIR}/slideio_def.hpp
   ${CMAKE_CURRENT_SOURCE_DIR}/slide.hpp
   ${CMAKE_CURRENT_SOURCE_DIR}/slide.cpp
   ${CMAKE_CURRENT_SOURCE_DIR}/scene.hpp
   ${CMAKE_CURRENT_SOURCE_DIR}/scene.cpp
   )


add_library(${LIBRARY_NAME} SHARED ${SOURCE_FILES})
set_target_properties(${LIBRARY_NAME} PROPERTIES OUTPUT_NAME "${LIBRARY_NAME}${LIB_NAME_SUFFIX}")

find_package(SQLite3)
find_package(OpenCV)
find_package(ZLIB)

target_link_libraries(${LIBRARY_NAME}
    SQLite::SQLite3
    opencv::opencv
    ZLIB::ZLIB
)

SOURCE_GROUP_BY_FOLDER(SOURCE_FILES)

target_link_libraries(${LIBRARY_NAME} 
   ${CORE_LIB_NAME}
   ${IMAGETOOLS_LIB_NAME}
   ${GDAL_DRIVER_NAME}
   ${SVS_DRIVER_NAME}
   ${AFI_DRIVER_NAME}
   ${CZI_DRIVER_NAME}
   ${DCM_DRIVER_NAME}
   ${SCN_DRIVER_NAME}
   ${ZVI_DRIVER_NAME}
   ${NDPI_DRIVER_NAME}
   ${VSI_DRIVER_NAME}
   ${PKE_DRIVER_NAME}
   ${OMETIFF_DRIVER_NAME}
   )

IF(WIN32)
   target_link_libraries(${LIBRARY_NAME} Shlwapi)
   set_target_properties(${LIBRARY_NAME} PROPERTIES LINK_FLAGS "/ignore:4099")
   target_compile_definitions(${LIBRARY_NAME} PRIVATE SLIDEIO_API _CRT_SECURE_NO_WARNINGS)
ENDIF(WIN32)

set_target_properties(${LIBRARY_NAME} PROPERTIES LINKER_LANGUAGE CXX)

if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    target_link_options(${LIBRARY_NAME} PRIVATE "-Wl,-Bsymbolic")
endif()

FIX_MACOS_RPATH(${LIBRARY_NAME})
