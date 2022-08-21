CONAN_MAIN()

set(LIBRARY_NAME ${DCM_DRIVER_NAME})

set(SOURCE_FILES 
        ${SOURCE_FILES}
        ${CMAKE_CURRENT_SOURCE_DIR}/dcmimagedriver.hpp
        ${CMAKE_CURRENT_SOURCE_DIR}/dcmimagedriver.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/dcmscene.hpp
        ${CMAKE_CURRENT_SOURCE_DIR}/dcmscene.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/dcmslide.hpp
        ${CMAKE_CURRENT_SOURCE_DIR}/dcmslide.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/dcmfile.hpp
        ${CMAKE_CURRENT_SOURCE_DIR}/dcmfile.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/dcm_api_def.hpp
        )

add_library(${LIBRARY_NAME} SHARED ${SOURCE_FILES})

SOURCE_GROUP_BY_FOLDER(SOURCE_FILES)

IF(WIN32)
        target_link_libraries(${LIBRARY_NAME} ${CORE_LIB_NAME} Shlwapi)
        set_target_properties(${LIBRARY_NAME} PROPERTIES LINK_FLAGS "/ignore:4099")
        target_compile_definitions(${LIBRARY_NAME} PRIVATE SLIDEIO_DCM_API _CRT_SECURE_NO_WARNINGS)
ENDIF(WIN32)

set_target_properties(${LIBRARY_NAME} PROPERTIES LINKER_LANGUAGE CXX)

CONAN_CONFIGURE(${LIBRARY_NAME})