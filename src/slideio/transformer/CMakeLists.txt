CONAN_MAIN()
set(LIBRARY_NAME ${TRANSFORMER_LIB_NAME})

set(SOURCE_FILES 
   ${SOURCE_FILES}
   ${CMAKE_CURRENT_SOURCE_DIR}/transformer.hpp
   ${CMAKE_CURRENT_SOURCE_DIR}/transformer.cpp
   ${CMAKE_CURRENT_SOURCE_DIR}/transformation.cpp
   ${CMAKE_CURRENT_SOURCE_DIR}/transformation.hpp
   ${CMAKE_CURRENT_SOURCE_DIR}/transformer_def.hpp
   ${CMAKE_CURRENT_SOURCE_DIR}/transformerscene.hpp
   ${CMAKE_CURRENT_SOURCE_DIR}/transformerscene.cpp
   ${CMAKE_CURRENT_SOURCE_DIR}/colortransformation.hpp
   ${CMAKE_CURRENT_SOURCE_DIR}/colortransformation.cpp
   ${CMAKE_CURRENT_SOURCE_DIR}/transformations.hpp
   ${CMAKE_CURRENT_SOURCE_DIR}/transformations.cpp
   ${CMAKE_CURRENT_SOURCE_DIR}/transformertools.cpp
   ${CMAKE_CURRENT_SOURCE_DIR}/transformertools.hpp
   ${CMAKE_CURRENT_SOURCE_DIR}/gaussianblurfilter.hpp
   ${CMAKE_CURRENT_SOURCE_DIR}/gaussianblurfilter.cpp
   ${CMAKE_CURRENT_SOURCE_DIR}/cannyfilter.hpp
   ${CMAKE_CURRENT_SOURCE_DIR}/cannyfilter.cpp
   ${CMAKE_CURRENT_SOURCE_DIR}/medianblurfilter.hpp
   ${CMAKE_CURRENT_SOURCE_DIR}/medianblurfilter.cpp
   ${CMAKE_CURRENT_SOURCE_DIR}/bilateralfilter.hpp
   ${CMAKE_CURRENT_SOURCE_DIR}/bilateralfilter.cpp
   ${CMAKE_CURRENT_SOURCE_DIR}/sobelfilter.hpp
   ${CMAKE_CURRENT_SOURCE_DIR}/sobelfilter.cpp
   ${CMAKE_CURRENT_SOURCE_DIR}/laplacianfilter.hpp
   ${CMAKE_CURRENT_SOURCE_DIR}/laplacianfilter.cpp
   ${CMAKE_CURRENT_SOURCE_DIR}/scharrfilter.hpp
   ${CMAKE_CURRENT_SOURCE_DIR}/scharrfilter.cpp
   )


add_library(${LIBRARY_NAME} SHARED ${SOURCE_FILES})

SOURCE_GROUP_BY_FOLDER(SOURCE_FILES)

target_link_libraries(${LIBRARY_NAME}
	${CORE_LIB_NAME}
	${SLIDEIO_LIB_NAME}
)

IF(WIN32)
   target_link_libraries(${LIBRARY_NAME} Shlwapi)
   set_target_properties(${LIBRARY_NAME} PROPERTIES LINK_FLAGS "/ignore:4099")
   target_compile_definitions(${LIBRARY_NAME} PRIVATE SLIDEIO_TRANSFORMER_API _CRT_SECURE_NO_WARNINGS)
ENDIF(WIN32)


set_target_properties(${LIBRARY_NAME} PROPERTIES LINKER_LANGUAGE CXX)

CONAN_CONFIGURE(${LIBRARY_NAME})