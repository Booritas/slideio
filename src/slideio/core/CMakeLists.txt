CONAN_MAIN()
set(LIBRARY_NAME ${CORE_LIB_NAME})

set(SOURCE_FILES 
   ${SOURCE_FILES}
   ${CMAKE_CURRENT_SOURCE_DIR}/imagedriver.hpp
   ${CMAKE_CURRENT_SOURCE_DIR}/imagedriver.cpp
   ${CMAKE_CURRENT_SOURCE_DIR}/cvscene.hpp
   ${CMAKE_CURRENT_SOURCE_DIR}/cvscene.cpp
   ${CMAKE_CURRENT_SOURCE_DIR}/cvslide.hpp
   ${CMAKE_CURRENT_SOURCE_DIR}/cvslide.cpp
   ${CMAKE_CURRENT_SOURCE_DIR}/cvstructs.hpp
   ${CMAKE_CURRENT_SOURCE_DIR}/cvtools.cpp
   ${CMAKE_CURRENT_SOURCE_DIR}/cvtools.hpp
   ${CMAKE_CURRENT_SOURCE_DIR}/cvsmallscene.hpp
   ${CMAKE_CURRENT_SOURCE_DIR}/cvsmallscene.cpp
   ${CMAKE_CURRENT_SOURCE_DIR}/refcounter.hpp
   )

add_subdirectory(tools)


add_library(${LIBRARY_NAME} SHARED ${SOURCE_FILES})

SOURCE_GROUP_BY_FOLDER(SOURCE_FILES)

IF(WIN32)
   target_link_libraries(${LIBRARY_NAME} Shlwapi)
   set_target_properties(${LIBRARY_NAME} PROPERTIES LINK_FLAGS "/ignore:4099")
   target_compile_definitions(${LIBRARY_NAME} PRIVATE SLIDEIO_CORE _CRT_SECURE_NO_WARNINGS)
ENDIF(WIN32)

set_target_properties(${LIBRARY_NAME} PROPERTIES LINKER_LANGUAGE CXX)
#set_target_properties(${LIBRARY_NAME} PROPERTIES CXX_VISIBILITY_PRESET hidden)

CONAN_CONFIGURE(${LIBRARY_NAME})