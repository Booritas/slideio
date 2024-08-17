CONAN_MAIN()

set(LIBRARY_NAME ${VSI_DRIVER_NAME})

set(SOURCE_FILES 
   ${CMAKE_CURRENT_SOURCE_DIR}/vsiimagedriver.hpp
   ${CMAKE_CURRENT_SOURCE_DIR}/vsiimagedriver.cpp
   ${CMAKE_CURRENT_SOURCE_DIR}/vsiscene.hpp
   ${CMAKE_CURRENT_SOURCE_DIR}/vsiscene.cpp
   ${CMAKE_CURRENT_SOURCE_DIR}/vsislide.hpp
   ${CMAKE_CURRENT_SOURCE_DIR}/vsislide.cpp
   ${CMAKE_CURRENT_SOURCE_DIR}/vsi_api_def.hpp
   ${CMAKE_CURRENT_SOURCE_DIR}/vsistruct.hpp
   ${CMAKE_CURRENT_SOURCE_DIR}/vsitags.hpp
   ${CMAKE_CURRENT_SOURCE_DIR}/vsistream.cpp
   ${CMAKE_CURRENT_SOURCE_DIR}/vsistream.hpp
   ${CMAKE_CURRENT_SOURCE_DIR}/etsfile.hpp
   ${CMAKE_CURRENT_SOURCE_DIR}/etsfile.cpp
   ${CMAKE_CURRENT_SOURCE_DIR}/vsitools.hpp
   ${CMAKE_CURRENT_SOURCE_DIR}/vsitools.cpp
   ${CMAKE_CURRENT_SOURCE_DIR}/vsifile.hpp
   ${CMAKE_CURRENT_SOURCE_DIR}/vsifile.cpp
   ${CMAKE_CURRENT_SOURCE_DIR}/vsifilescene.hpp
   ${CMAKE_CURRENT_SOURCE_DIR}/vsifilescene.cpp
   ${CMAKE_CURRENT_SOURCE_DIR}/etsfilescene.hpp
   ${CMAKE_CURRENT_SOURCE_DIR}/etsfilescene.cpp
   ${CMAKE_CURRENT_SOURCE_DIR}/volume.hpp
   ${CMAKE_CURRENT_SOURCE_DIR}/volume.cpp
   ${CMAKE_CURRENT_SOURCE_DIR}/taginfo.hpp
   ${CMAKE_CURRENT_SOURCE_DIR}/taginfo.cpp
   ${CMAKE_CURRENT_SOURCE_DIR}/dimensions.hpp
   ${CMAKE_CURRENT_SOURCE_DIR}/pyramid.hpp
   ${CMAKE_CURRENT_SOURCE_DIR}/pyramid.cpp
)

add_library(${LIBRARY_NAME} SHARED ${SOURCE_FILES})
set_target_properties(${LIBRARY_NAME} PROPERTIES OUTPUT_NAME "${LIBRARY_NAME}${LIB_NAME_SUFFIX}")

SOURCE_GROUP_BY_FOLDER(SOURCE_FILES)

target_link_libraries(${LIBRARY_NAME} ${CORE_LIB_NAME} ${IMAGETOOLS_LIB_NAME})

IF(WIN32)
   target_link_libraries(${LIBRARY_NAME} Shlwapi)
   set_target_properties(${LIBRARY_NAME} PROPERTIES LINK_FLAGS "/ignore:4099")
   target_compile_definitions(${LIBRARY_NAME} PRIVATE SLIDEIO_VSI_API _CRT_SECURE_NO_WARNINGS)
ENDIF(WIN32)

set_target_properties(${LIBRARY_NAME} PROPERTIES LINKER_LANGUAGE CXX)

CONAN_CONFIGURE_INCLUDES(${LIBRARY_NAME})