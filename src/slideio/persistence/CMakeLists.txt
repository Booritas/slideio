CONAN_MAIN()
set(LIBRARY_NAME ${PERSISTENCE_LIB_NAME})

HIDE_LIB("hdf5")

set(SOURCE_FILES
   persistence.hpp
   persistence.cpp
   storage.cpp
   storage.hpp
   hdf5storage.cpp
   hdf5storage.hpp
   slideio_persistence_def.hpp
)

add_library(${LIBRARY_NAME} SHARED ${SOURCE_FILES})

IF(WIN32)
   target_link_libraries(${LIBRARY_NAME} Shlwapi)
   set_target_properties(${LIBRARY_NAME} PROPERTIES LINK_FLAGS "/ignore:4099")
   target_compile_definitions(${LIBRARY_NAME} PRIVATE SLIDEIO_PERSISTENCE _CRT_SECURE_NO_WARNINGS)
ENDIF(WIN32)

target_link_libraries(${LIBRARY_NAME}
   ${BASE_LIB_NAME}
   ${CORE_LIB_NAME}
)

CONAN_CONFIGURE(${LIBRARY_NAME})

set_target_properties(${LIBRARY_NAME} PROPERTIES LINKER_LANGUAGE CXX)
set_property(TARGET ${LIBRARY_NAME} PROPERTY POSITION_INDEPENDENT_CODE ON)
