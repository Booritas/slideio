set(INCLUDE_ROOT ${CMAKE_SOURCE_DIR}/src)
set(TOOL_NAME tiffinspector)



set(SOURCE_FILES 
   tiffinspector.cpp
)

add_executable(${TOOL_NAME} ${SOURCE_FILES})

IF(WIN32)
   #target_link_libraries(${TOOL_NAME} Shlwapi)
   set_target_properties(${TOOL_NAME} PROPERTIES LINK_FLAGS "/ignore:4099")
   target_compile_definitions(${TOOL_NAME} PRIVATE _CRT_SECURE_NO_WARNINGS)
ENDIF(WIN32)

target_compile_definitions(${TOOL_NAME} PRIVATE OPJ_HAVE_LIBTIFF OPJ_HAVE_LIBPNG)
set_target_properties(${TOOL_NAME} PROPERTIES LINKER_LANGUAGE CXX)
target_include_directories(${TOOL_NAME} PRIVATE ${INCLUDE_ROOT})
target_link_libraries(${TOOL_NAME} PRIVATE
   ${CORE_LIB_NAME}
   ${SLIDEIO_LIB_NAME}
   ${IMAGETOOLS_LIB_NAME}
   ${TEST_LIB_NAME}
   ${BASE_LIB_NAME}
   ${CONVERTER_LIB_NAME}
)

FIX_MACOS_RPATH(${TOOL_NAME})