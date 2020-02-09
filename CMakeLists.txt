cmake_minimum_required(VERSION 3.2)

set(PROJECT_NAME slideio)
set(projectVersion 0.0.1)
set(INCLUDE_ROOT ${CMAKE_SOURCE_DIR}/slideio)


message(${INCLUDE_ROOT})
project(${PROJECT_NAME} VERSION ${projectVersion} LANGUAGES C CXX)

set(BUILD_SHARED_LIBS YES)

include(conanbuildinfo_multi.cmake)
conan_basic_setup()

foreach( OUTPUTCONFIG ${CMAKE_CONFIGURATION_TYPES} )
   string( TOUPPER ${OUTPUTCONFIG} OUTPUTCONFIGL )
   set( CMAKE_ARCHIVE_OUTPUT_DIRECTORY_${OUTPUTCONFIGL} ${CMAKE_BINARY_DIR}/${OUTPUTCONFIG}/lib )
   set( CMAKE_LIBRARY_OUTPUT_DIRECTORY_${OUTPUTCONFIGL} ${CMAKE_BINARY_DIR}/${OUTPUTCONFIG}/lib )
   set( CMAKE_RUNTIME_OUTPUT_DIRECTORY_${OUTPUTCONFIGL} ${CMAKE_BINARY_DIR}/${OUTPUTCONFIG}/bin )
endforeach( OUTPUTCONFIG CMAKE_CONFIGURATION_TYPES )

add_subdirectory(slideio)

add_library(${PROJECT_NAME} ${SOURCE_FILES})

IF(WIN32)
   target_link_libraries(${PROJECT_NAME} Shlwapi)
   set_target_properties(${PROJECT_NAME} PROPERTIES LINK_FLAGS "/ignore:4099")
   target_compile_definitions(${PROJECT_NAME} PRIVATE SLIDEIO_API _CRT_SECURE_NO_WARNINGS)
ENDIF(WIN32)

conan_target_link_libraries(${PROJECT_NAME})
set_target_properties(${PROJECT_NAME} PROPERTIES LINKER_LANGUAGE CXX)
target_include_directories(${PROJECT_NAME} PRIVATE ${INCLUDE_ROOT})

