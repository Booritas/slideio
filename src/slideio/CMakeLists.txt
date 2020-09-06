set(LIBRARY_NAME slide_io)

CONAN_MAIN()

add_subdirectory(core)
add_subdirectory(drivers)
add_subdirectory(imagetools)

set(SOURCE_FILES 
	${SOURCE_FILES}
	${CMAKE_CURRENT_SOURCE_DIR}/slideio.hpp
	${CMAKE_CURRENT_SOURCE_DIR}/slideio.cpp
   	${CMAKE_CURRENT_SOURCE_DIR}/slideio_def.hpp
   	${CMAKE_CURRENT_SOURCE_DIR}/gdal_lib.hpp
   	${CMAKE_CURRENT_SOURCE_DIR}/slide.hpp
   	${CMAKE_CURRENT_SOURCE_DIR}/slide.cpp
   	${CMAKE_CURRENT_SOURCE_DIR}/scene.hpp
   	${CMAKE_CURRENT_SOURCE_DIR}/scene.cpp
   	${CMAKE_CURRENT_SOURCE_DIR}/structs.hpp
   	${CMAKE_CURRENT_SOURCE_DIR}/slideio_enums.hpp
   	${CMAKE_CURRENT_SOURCE_DIR}/xmltools.hpp
   	${CMAKE_CURRENT_SOURCE_DIR}/xmltools.cpp
   	${CMAKE_CURRENT_SOURCE_DIR}/libtiff.hpp
)

add_library(${LIBRARY_NAME} SHARED ${SOURCE_FILES})

IF(WIN32)
   target_link_libraries(${LIBRARY_NAME} Shlwapi)
   set_target_properties(${LIBRARY_NAME} PROPERTIES LINK_FLAGS "/ignore:4099")
   target_compile_definitions(${LIBRARY_NAME} PRIVATE SLIDEIO_API _CRT_SECURE_NO_WARNINGS)
ENDIF(WIN32)

set_target_properties(${LIBRARY_NAME} PROPERTIES LINKER_LANGUAGE CXX)
target_include_directories(${LIBRARY_NAME} PRIVATE ${INCLUDE_ROOT})

CONAN_CONFIGURE(${LIBRARY_NAME})