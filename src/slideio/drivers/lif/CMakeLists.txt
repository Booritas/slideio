set(LIBRARY_NAME ${LIF_DRIVER_NAME})
set(CMAKE_PREFIX_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
set(CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

set(SOURCE_FILES
   ${SOURCE_FILES}
   ${CMAKE_CURRENT_SOURCE_DIR}/lif_api_def.hpp
   ${CMAKE_CURRENT_SOURCE_DIR}/lifcontainer.hpp
   ${CMAKE_CURRENT_SOURCE_DIR}/lifcontainer.cpp
   ${CMAKE_CURRENT_SOURCE_DIR}/lifxml.hpp
   ${CMAKE_CURRENT_SOURCE_DIR}/lifxml.cpp
   ${CMAKE_CURRENT_SOURCE_DIR}/liftimestamps.hpp
   ${CMAKE_CURRENT_SOURCE_DIR}/liftimestamps.cpp
   ${CMAKE_CURRENT_SOURCE_DIR}/liflayout.hpp
   ${CMAKE_CURRENT_SOURCE_DIR}/liflayout.cpp
   ${CMAKE_CURRENT_SOURCE_DIR}/lifscene.hpp
   ${CMAKE_CURRENT_SOURCE_DIR}/lifscene.cpp
   ${CMAKE_CURRENT_SOURCE_DIR}/lifslide.hpp
   ${CMAKE_CURRENT_SOURCE_DIR}/lifslide.cpp
   ${CMAKE_CURRENT_SOURCE_DIR}/lifimagedriver.hpp
   ${CMAKE_CURRENT_SOURCE_DIR}/lifimagedriver.cpp
   )

add_library(${LIBRARY_NAME} SHARED ${SOURCE_FILES})
HIDE_THIRD_PARTY_SYMBOLS(${LIBRARY_NAME})
set_target_properties(${LIBRARY_NAME} PROPERTIES OUTPUT_NAME "${LIBRARY_NAME}${LIB_NAME_SUFFIX}")

find_package(OpenCV)
find_package(tinyxml2)

target_link_libraries(${LIBRARY_NAME}
   opencv::opencv
   tinyxml2::tinyxml2
)

SOURCE_GROUP_BY_FOLDER(SOURCE_FILES)

target_link_libraries(${LIBRARY_NAME} ${CORE_LIB_NAME})

IF(WIN32)
   set_target_properties(${LIBRARY_NAME} PROPERTIES LINK_FLAGS "/ignore:4099")
   target_compile_definitions(${LIBRARY_NAME} PRIVATE SLIDEIO_LIF_API _CRT_SECURE_NO_WARNINGS)
ENDIF(WIN32)

set_target_properties(${LIBRARY_NAME} PROPERTIES LINKER_LANGUAGE CXX)

if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    target_link_options(${LIBRARY_NAME} PRIVATE "-Wl,-Bsymbolic")
endif()
