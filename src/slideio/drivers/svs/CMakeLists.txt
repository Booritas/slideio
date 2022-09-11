CONAN_MAIN()

set(LIBRARY_NAME ${SVS_DRIVER_NAME})

set(SOURCE_FILES 
   ${SOURCE_FILES}
   ${CMAKE_CURRENT_SOURCE_DIR}/svsimagedriver.hpp
   ${CMAKE_CURRENT_SOURCE_DIR}/svsimagedriver.cpp
   ${CMAKE_CURRENT_SOURCE_DIR}/svsscene.hpp
   ${CMAKE_CURRENT_SOURCE_DIR}/svsscene.cpp
   ${CMAKE_CURRENT_SOURCE_DIR}/svsslide.hpp
   ${CMAKE_CURRENT_SOURCE_DIR}/svsslide.cpp
   ${CMAKE_CURRENT_SOURCE_DIR}/svssmallscene.hpp
   ${CMAKE_CURRENT_SOURCE_DIR}/svssmallscene.cpp
   ${CMAKE_CURRENT_SOURCE_DIR}/svstiledscene.hpp
   ${CMAKE_CURRENT_SOURCE_DIR}/svstiledscene.cpp
   ${CMAKE_CURRENT_SOURCE_DIR}/svstools.hpp
   ${CMAKE_CURRENT_SOURCE_DIR}/svstools.cpp
   ${CMAKE_CURRENT_SOURCE_DIR}/svs_api_def.hpp
   )

add_library(${LIBRARY_NAME} SHARED ${SOURCE_FILES})

SOURCE_GROUP_BY_FOLDER(SOURCE_FILES)

target_link_libraries(${LIBRARY_NAME} ${CORE_LIB_NAME} ${IMAGETOOLS_LIB_NAME})

IF(WIN32)
   target_link_libraries(${LIBRARY_NAME} Shlwapi)
   set_target_properties(${LIBRARY_NAME} PROPERTIES LINK_FLAGS "/ignore:4099")
   target_compile_definitions(${LIBRARY_NAME} PRIVATE SLIDEIO_SVS_API _CRT_SECURE_NO_WARNINGS)
ENDIF(WIN32)

set_target_properties(${LIBRARY_NAME} PROPERTIES LINKER_LANGUAGE CXX)

CONAN_CONFIGURE(${LIBRARY_NAME})