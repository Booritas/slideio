set(LIBRARY_NAME ${SLIDEIO_LIB_NAME})
set(CMAKE_PREFIX_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

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

find_package(Boost)
find_package(SQLite3)
find_package(glog)
find_package(OpenCV)
find_package(ZLIB)

target_link_libraries(${LIBRARY_NAME} 
    boost::boost
    SQLite::SQLite3
    glog::glog
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
   )

IF(WIN32)
   target_link_libraries(${LIBRARY_NAME} Shlwapi)
   set_target_properties(${LIBRARY_NAME} PROPERTIES LINK_FLAGS "/ignore:4099")
   target_compile_definitions(${LIBRARY_NAME} PRIVATE SLIDEIO_API _CRT_SECURE_NO_WARNINGS)
ENDIF(WIN32)

set_target_properties(${LIBRARY_NAME} PROPERTIES LINKER_LANGUAGE CXX)
#set_target_properties(${LIBRARY_NAME} PROPERTIES CXX_VISIBILITY_PRESET hidden)



IF(APPLE)
    add_custom_command(TARGET ${SLIDEIO_LIB_NAME} PRE_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy 
        $<$<CONFIG:Debug>:${glog_LIB_DIRS_DEBUG}/*.dylib>$<$<CONFIG:Release>:${glog_LIB_DIRS_RELEASE}/*.dylib>
        $<TARGET_FILE_DIR:${LIBRARY_NAME}>
    )
ELSEIF(WIN32)
    add_custom_command(TARGET ${SLIDEIO_LIB_NAME} PRE_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy 
        $<$<CONFIG:Debug>:${glog_BIN_DIRS_DEBUG}/${glog_LIBS_DEBUG}.dll>$<$<CONFIG:Release>:${glog_BIN_DIRS_RELEASE}/${glog_LIBS_RELEASE}.dll>
        $<TARGET_FILE_DIR:${LIBRARY_NAME}>
    )
ELSE()
    add_custom_command(TARGET ${SLIDEIO_LIB_NAME} PRE_BUILD
        COMMAND cp -a -f
        $<$<CONFIG:Debug>:${glog_LIB_DIRS_DEBUG}/*.*>$<$<CONFIG:Release>:${glog_LIB_DIRS_RELEASE}/*.*>
        $<TARGET_FILE_DIR:${LIBRARY_NAME}>
    )
ENDIF(APPLE)