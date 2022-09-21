set(INCLUDE_ROOT ${CMAKE_SOURCE_DIR}/src)
set(TEST_NAME memory_leaks)

CONAN_MAIN()

set(SOURCE_FILES 
	memory_leaks.cpp
)

add_executable(${TEST_NAME} ${SOURCE_FILES})

IF(WIN32)
   #target_link_libraries(${TEST_NAME} Shlwapi)
   set_target_properties(${TEST_NAME} PROPERTIES LINK_FLAGS "/ignore:4099")
   target_compile_definitions(${TEST_NAME} PRIVATE SLIDEIO_API _CRT_SECURE_NO_WARNINGS)
ENDIF(WIN32)

set_target_properties(${TEST_NAME} PROPERTIES LINKER_LANGUAGE CXX)
target_include_directories(${TEST_NAME} PRIVATE ${INCLUDE_ROOT})
target_link_libraries(${TEST_NAME} PRIVATE
   ${CORE_LIB_NAME}
   $(IMAGETOOLS_LIB_NAME)
   ${CZI_DRIVER_NAME}
)
