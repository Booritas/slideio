CONAN_MAIN()
set(LIBRARY_NAME ${TRANSFORMER_LIB_NAME})

set(SOURCE_FILES 
   ${SOURCE_FILES}
   ${CMAKE_CURRENT_SOURCE_DIR}/transformer.hpp
   ${CMAKE_CURRENT_SOURCE_DIR}/transformer.cpp
   ${CMAKE_CURRENT_SOURCE_DIR}/transformation.cpp
   ${CMAKE_CURRENT_SOURCE_DIR}/transformation.hpp
   ${CMAKE_CURRENT_SOURCE_DIR}/transformer_def.hpp
   ${CMAKE_CURRENT_SOURCE_DIR}/colortransformerscene.hpp
   ${CMAKE_CURRENT_SOURCE_DIR}/colortransformerscene.cpp
   ${CMAKE_CURRENT_SOURCE_DIR}/transformerscene.hpp
   ${CMAKE_CURRENT_SOURCE_DIR}/transformerscene.cpp
   ${CMAKE_CURRENT_SOURCE_DIR}/colortransformation.hpp
   ${CMAKE_CURRENT_SOURCE_DIR}/convolutionfilterscene.cpp
   ${CMAKE_CURRENT_SOURCE_DIR}/convolutionfilterscene.hpp
   ${CMAKE_CURRENT_SOURCE_DIR}/convolutionfilter.hpp
   ${CMAKE_CURRENT_SOURCE_DIR}/transformations.hpp
   ${CMAKE_CURRENT_SOURCE_DIR}/transformertools.cpp
   ${CMAKE_CURRENT_SOURCE_DIR}/transformertools.hpp
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
   target_compile_definitions(${LIBRARY_NAME} PRIVATE SLIDEIO_TRANSFORMER_API _CRT_SECURE_NO_WARNINGS)
ENDIF(WIN32)


set_target_properties(${LIBRARY_NAME} PROPERTIES LINKER_LANGUAGE CXX)

CONAN_CONFIGURE(${LIBRARY_NAME})