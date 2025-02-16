set(LIBRARY_NAME ${BASE_LIB_NAME})
set(CMAKE_PREFIX_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
set(CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

set(SOURCE_FILES
   ${SOURCE_FILES}
   ${CMAKE_CURRENT_SOURCE_DIR}/slideio_enums.cpp
   ${CMAKE_CURRENT_SOURCE_DIR}/slideio_enums.hpp
   ${CMAKE_CURRENT_SOURCE_DIR}/log.hpp
   ${CMAKE_CURRENT_SOURCE_DIR}/exceptions.cpp
   ${CMAKE_CURRENT_SOURCE_DIR}/exceptions.hpp
   ${CMAKE_CURRENT_SOURCE_DIR}/slideio_base_def.hpp
   ${CMAKE_CURRENT_SOURCE_DIR}/slideio_structs.hpp
   ${CMAKE_CURRENT_SOURCE_DIR}/size.hpp
   ${CMAKE_CURRENT_SOURCE_DIR}/rect.hpp
   ${CMAKE_CURRENT_SOURCE_DIR}/resolution.hpp
)

add_library(${LIBRARY_NAME} SHARED ${SOURCE_FILES})
set_target_properties(${LIBRARY_NAME} PROPERTIES OUTPUT_NAME "${LIBRARY_NAME}${LIB_NAME_SUFFIX}")


find_package(glog REQUIRED)
target_link_libraries(${LIBRARY_NAME} glog::glog)

IF(WIN32)
   target_link_libraries(${LIBRARY_NAME} Shlwapi)
   set_target_properties(${LIBRARY_NAME} PROPERTIES LINK_FLAGS "/ignore:4099")
   target_compile_definitions(${LIBRARY_NAME} PRIVATE SLIDEIO_BASE_API _CRT_SECURE_NO_WARNINGS)
ENDIF(WIN32)

set_target_properties(${LIBRARY_NAME} PROPERTIES LINKER_LANGUAGE CXX)
set_property(TARGET ${LIBRARY_NAME} PROPERTY POSITION_INDEPENDENT_CODE ON)