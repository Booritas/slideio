CONAN_MAIN()

set(LIBRARY_NAME ${GDAL_DRIVER_NAME})

set(SOURCE_FILES ${SOURCE_FILES}
   ${CMAKE_CURRENT_SOURCE_DIR}/gdalimagedriver.hpp
   ${CMAKE_CURRENT_SOURCE_DIR}/gdalimagedriver.cpp
   ${CMAKE_CURRENT_SOURCE_DIR}/gdalscene.hpp
   ${CMAKE_CURRENT_SOURCE_DIR}/gdalscene.cpp
   ${CMAKE_CURRENT_SOURCE_DIR}/gdalslide.hpp
   ${CMAKE_CURRENT_SOURCE_DIR}/gdalslide.cpp
   ${CMAKE_CURRENT_SOURCE_DIR}/gdal_api_def.hpp
   )

add_library(${LIBRARY_NAME} SHARED ${SOURCE_FILES})

SOURCE_GROUP_BY_FOLDER(SOURCE_FILES)

target_link_libraries(${LIBRARY_NAME} ${CORE_LIB_NAME})

IF(WIN32)
   target_link_libraries(${LIBRARY_NAME} Shlwapi)
   set_target_properties(${LIBRARY_NAME} PROPERTIES LINK_FLAGS "/ignore:4099")
   target_compile_definitions(${LIBRARY_NAME} PRIVATE SLIDEIO_GDAL_API _CRT_SECURE_NO_WARNINGS)
ENDIF(WIN32)

set_target_properties(${LIBRARY_NAME} PROPERTIES LINKER_LANGUAGE CXX)

CONAN_CONFIGURE(${LIBRARY_NAME})