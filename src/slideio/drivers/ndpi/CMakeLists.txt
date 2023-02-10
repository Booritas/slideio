CONAN_MAIN()

set(LIBRARY_NAME ${NDPI_DRIVER_NAME})

HIDE_LIB("libtiff")
HIDE_LIB("libjpeg")


set(SOURCE_FILES 
   ${SOURCE_FILES}
   ${CMAKE_CURRENT_SOURCE_DIR}/ndpiimagedriver.hpp
   ${CMAKE_CURRENT_SOURCE_DIR}/ndpiimagedriver.cpp
   ${CMAKE_CURRENT_SOURCE_DIR}/ndpiscene.hpp
   ${CMAKE_CURRENT_SOURCE_DIR}/ndpiscene.cpp
   ${CMAKE_CURRENT_SOURCE_DIR}/ndpislide.hpp
   ${CMAKE_CURRENT_SOURCE_DIR}/ndpislide.cpp
   ${CMAKE_CURRENT_SOURCE_DIR}/ndpi_api_def.hpp
   ${CMAKE_CURRENT_SOURCE_DIR}/ndpifile.hpp
   ${CMAKE_CURRENT_SOURCE_DIR}/ndpifile.cpp
   ${CMAKE_CURRENT_SOURCE_DIR}/ndpitifftools.hpp
   ${CMAKE_CURRENT_SOURCE_DIR}/ndpitifftools.cpp
   ${CMAKE_CURRENT_SOURCE_DIR}/ndpilibtiff.hpp
   ${CMAKE_CURRENT_SOURCE_DIR}/ndpitiffmessagehandler.hpp
   ${CMAKE_CURRENT_SOURCE_DIR}/ndpitiffmessagehandler.cpp
   )

add_library(${LIBRARY_NAME} SHARED ${SOURCE_FILES})

SOURCE_GROUP_BY_FOLDER(SOURCE_FILES)

target_link_libraries(${LIBRARY_NAME} ${CORE_LIB_NAME} ${IMAGETOOLS_LIB_NAME})

IF(WIN32)
   target_link_libraries(${LIBRARY_NAME} Shlwapi)
   set_target_properties(${LIBRARY_NAME} PROPERTIES LINK_FLAGS "/ignore:4099")
   target_compile_definitions(${LIBRARY_NAME} PRIVATE SLIDEIO_NDPI_API _CRT_SECURE_NO_WARNINGS)
ENDIF(WIN32)

set_target_properties(${LIBRARY_NAME} PROPERTIES LINKER_LANGUAGE CXX)

CONAN_CONFIGURE_INCLUDES(${LIBRARY_NAME})
conan_target_link_libraries_with_hide(${LIBRARY_NAME} "^(.)+(tiff|libjpeg)(.)+")
