#
# dependencies with a custom build
# 
# dcmtk
conan create %CONAN_CUSTOM_DIR%/dcmtk slideio/stable -s build_type=Release
conan create %CONAN_CUSTOM_DIR%/dcmtk slideio/stable -s build_type=Debug
# opencv
conan create %CONAN_CUSTOM_DIR%/opencv slideio/stable -s build_type=Release
conan create %CONAN_CUSTOM_DIR%/opencv slideio/stable -s build_type=Debug
# jpegxr codec
conan create  %CONAN_CUSTOM_DIR%/jpegxrcodec slideio/stable -s build_type=Release
conan create  %CONAN_CUSTOM_DIR%/jpegxrcodec slideio/stable -s build_type=Debug
# pole
conan create  %CONAN_CUSTOM_DIR%/pole slideio/stable -s build_type=Release
conan create  %CONAN_CUSTOM_DIR%/pole slideio/stable -s build_type=Debug
# gtest
conan create  %CONAN_CUSTOM_DIR%/gtest slideio/stable -s build_type=Release
conan create  %CONAN_CUSTOM_DIR%/gtest slideio/stable -s build_type=Debug
# openjpeg
conan create  %CONAN_CUSTOM_DIR%/openjpeg slideio/stable -s build_type=Release
conan create  %CONAN_CUSTOM_DIR%/openjpeg slideio/stable -s build_type=Debug
# proj
conan create  %CONAN_CUSTOM_DIR%/proj slideio/stable -s build_type=Release
conan create  %CONAN_CUSTOM_DIR%/proj slideio/stable -s build_type=Debug
# gdal
conan create  %CONAN_CUSTOM_DIR%/gdal slideio/stable -s build_type=Release
conan create  %CONAN_CUSTOM_DIR%/gdal slideio/stable -s build_type=Debug
