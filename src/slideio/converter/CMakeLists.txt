CONAN_MAIN()
set(LIBRARY_NAME ${CONVERTER_LIB_NAME})

set(SOURCE_FILES 
   ${SOURCE_FILES}
   ${CMAKE_CURRENT_SOURCE_DIR}/converter.hpp
   ${CMAKE_CURRENT_SOURCE_DIR}/converter_def.hpp
   ${CMAKE_CURRENT_SOURCE_DIR}/converter.cpp
   ${CMAKE_CURRENT_SOURCE_DIR}/convertertools.cpp
   ${CMAKE_CURRENT_SOURCE_DIR}/convertertools.hpp
   ${CMAKE_CURRENT_SOURCE_DIR}/convertertifftools.cpp
   ${CMAKE_CURRENT_SOURCE_DIR}/convertertifftools.hpp
   ${CMAKE_CURRENT_SOURCE_DIR}/convertersvstools.cpp
   ${CMAKE_CURRENT_SOURCE_DIR}/convertersvstools.hpp
   )


add_library(${LIBRARY_NAME} SHARED ${SOURCE_FILES})

SOURCE_GROUP_BY_FOLDER(SOURCE_FILES)

target_link_libraries(${LIBRARY_NAME}
	${CORE_LIB_NAME}
	${SLIDEIO_LIB_NAME}
)

IF(WIN32)
   target_link_libraries(${LIBRARY_NAME} Shlwapi)
   set_target_properties(${LIBRARY_NAME} PROPERTIES LINK_FLAGS "/ignore:4099")
   target_compile_definitions(${LIBRARY_NAME} PRIVATE SLIDEIO_CONVERTER_API _CRT_SECURE_NO_WARNINGS)
ENDIF(WIN32)


set_target_properties(${LIBRARY_NAME} PROPERTIES LINKER_LANGUAGE CXX)

CONAN_CONFIGURE(${LIBRARY_NAME})