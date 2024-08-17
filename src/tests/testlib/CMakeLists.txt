CONAN_MAIN()
set(LIBRARY_NAME ${TEST_LIB_NAME})

set(SOURCE_FILES 
   ${SOURCE_FILES}
   testtiler.cpp
   testtiler.hpp
   testtools.cpp
   testtools.hpp
   testscene.hpp
   testscene.cpp
   )


add_library(${LIBRARY_NAME} STATIC ${SOURCE_FILES})
set_target_properties(${LIBRARY_NAME} PROPERTIES OUTPUT_NAME "${LIBRARY_NAME}${LIB_NAME_SUFFIX}")

IF(WIN32)
   target_link_libraries(${LIBRARY_NAME} Shlwapi)
   set_target_properties(${LIBRARY_NAME} PROPERTIES LINK_FLAGS "/ignore:4099")
   target_compile_definitions(${LIBRARY_NAME} PRIVATE _CRT_SECURE_NO_WARNINGS)
ENDIF(WIN32)

target_link_libraries(${LIBRARY_NAME} ${CORE_LIB_NAME})
set_target_properties(${LIBRARY_NAME} PROPERTIES LINKER_LANGUAGE CXX)

CONAN_CONFIGURE(${LIBRARY_NAME})