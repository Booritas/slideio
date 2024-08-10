CONAN_MAIN()

set(LIBRARY_NAME ${OMETIFF_DRIVER_NAME})

HIDE_LIB(libtiff)
HIDE_LIB(libjpeg)

set(SOURCE_FILES
   ${SOURCE_FILES}
   ${CMAKE_CURRENT_SOURCE_DIR}/otimagedriver.hpp
   ${CMAKE_CURRENT_SOURCE_DIR}/otimagedriver.cpp
   ${CMAKE_CURRENT_SOURCE_DIR}/otscene.hpp
   ${CMAKE_CURRENT_SOURCE_DIR}/otscene.cpp
   ${CMAKE_CURRENT_SOURCE_DIR}/otslide.hpp
   ${CMAKE_CURRENT_SOURCE_DIR}/otslide.cpp
   ${CMAKE_CURRENT_SOURCE_DIR}/otsmallscene.hpp
   ${CMAKE_CURRENT_SOURCE_DIR}/otsmallscene.cpp
   ${CMAKE_CURRENT_SOURCE_DIR}/ottiledscene.hpp
   ${CMAKE_CURRENT_SOURCE_DIR}/ottiledscene.cpp
   ${CMAKE_CURRENT_SOURCE_DIR}/ottools.hpp
   ${CMAKE_CURRENT_SOURCE_DIR}/ottools.cpp
   ${CMAKE_CURRENT_SOURCE_DIR}/ot_api_def.hpp
   )

add_library(${LIBRARY_NAME} SHARED ${SOURCE_FILES})

SOURCE_GROUP_BY_FOLDER(SOURCE_FILES)

target_link_libraries(${LIBRARY_NAME} ${CORE_LIB_NAME} ${IMAGETOOLS_LIB_NAME})

IF(WIN32)
   target_link_libraries(${LIBRARY_NAME} Shlwapi)
   set_target_properties(${LIBRARY_NAME} PROPERTIES LINK_FLAGS "/ignore:4099")
   target_compile_definitions(${LIBRARY_NAME} PRIVATE SLIDEIO_OMETIFF_API _CRT_SECURE_NO_WARNINGS)
ENDIF(WIN32)

set_target_properties(${LIBRARY_NAME} PROPERTIES LINKER_LANGUAGE CXX)

CONAN_CONFIGURE_INCLUDES(${LIBRARY_NAME})
conan_target_link_libraries_with_hide(${LIBRARY_NAME} "^(.)+(tiff|libjpeg)(.)+")
