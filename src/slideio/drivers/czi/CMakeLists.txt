CONAN_MAIN()

set(LIBRARY_NAME ${CZI_DRIVER_NAME})

set(SOURCE_FILES 
   ${SOURCE_FILES}
   ${CMAKE_CURRENT_SOURCE_DIR}/cziimagedriver.hpp
   ${CMAKE_CURRENT_SOURCE_DIR}/cziimagedriver.cpp
   ${CMAKE_CURRENT_SOURCE_DIR}/cziscene.hpp
   ${CMAKE_CURRENT_SOURCE_DIR}/cziscene.cpp
   ${CMAKE_CURRENT_SOURCE_DIR}/czislide.hpp
   ${CMAKE_CURRENT_SOURCE_DIR}/czislide.cpp
   ${CMAKE_CURRENT_SOURCE_DIR}/czistructs.hpp
   ${CMAKE_CURRENT_SOURCE_DIR}/czisubblock.hpp
   ${CMAKE_CURRENT_SOURCE_DIR}/czisubblock.cpp
   ${CMAKE_CURRENT_SOURCE_DIR}/czithumbnail.hpp
   ${CMAKE_CURRENT_SOURCE_DIR}/czithumbnail.cpp
   ${CMAKE_CURRENT_SOURCE_DIR}/czitools.hpp
   ${CMAKE_CURRENT_SOURCE_DIR}/czitools.cpp
   ${CMAKE_CURRENT_SOURCE_DIR}/czi_api_def.hpp
   )

add_library(${LIBRARY_NAME} SHARED ${SOURCE_FILES})

SOURCE_GROUP_BY_FOLDER(SOURCE_FILES)

target_link_libraries(${LIBRARY_NAME} ${CORE_LIB_NAME} ${IMAGETOOLS_LIB_NAME})

IF(WIN32)
   target_link_libraries(${LIBRARY_NAME} Shlwapi)
   set_target_properties(${LIBRARY_NAME} PROPERTIES LINK_FLAGS "/ignore:4099")
   target_compile_definitions(${LIBRARY_NAME} PRIVATE SLIDEIO_CZI_API _CRT_SECURE_NO_WARNINGS)
ENDIF(WIN32)

set_target_properties(${LIBRARY_NAME} PROPERTIES LINKER_LANGUAGE CXX)

CONAN_CONFIGURE(${LIBRARY_NAME})