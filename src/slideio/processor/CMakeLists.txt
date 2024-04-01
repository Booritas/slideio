CONAN_MAIN()
set(LIBRARY_NAME ${PROCESSOR_LIB_NAME})


set(SOURCE_FILES
   processor.hpp
   processor.cpp
   imageobject.hpp
   imageobjectpixeliterator.hpp
   imageobject.cpp
   imageobjectmanager.hpp
   imageobjectmanager.cpp
   contourmanager.hpp
   contourmanager.cpp
   project.hpp
   project.cpp
   slideio_processor_def.hpp
)

add_library(${LIBRARY_NAME} SHARED ${SOURCE_FILES})

IF(WIN32)
   target_link_libraries(${LIBRARY_NAME} Shlwapi)
   set_target_properties(${LIBRARY_NAME} PROPERTIES LINK_FLAGS "/ignore:4099")
   target_compile_definitions(${LIBRARY_NAME} PRIVATE SLIDEIO_PROCESSOR _CRT_SECURE_NO_WARNINGS)
ENDIF(WIN32)

target_link_libraries(${LIBRARY_NAME}
   ${BASE_LIB_NAME}
   ${CORE_LIB_NAME}
   ${PERSISTENT_LIB_NAME}
)

CONAN_CONFIGURE(${LIBRARY_NAME})

set_target_properties(${LIBRARY_NAME} PROPERTIES LINKER_LANGUAGE CXX)
set_property(TARGET ${LIBRARY_NAME} PROPERTY POSITION_INDEPENDENT_CODE ON)
