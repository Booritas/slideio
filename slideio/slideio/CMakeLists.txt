add_subdirectory(core)
add_subdirectory(drivers)
add_subdirectory(imagetools)

set(SOURCE_FILES 
	${SOURCE_FILES}
	${CMAKE_CURRENT_SOURCE_DIR}/slideio.hpp
   ${CMAKE_CURRENT_SOURCE_DIR}/slideio_def.hpp
   ${CMAKE_CURRENT_SOURCE_DIR}/gdal_lib.hpp
	PARENT_SCOPE
)

