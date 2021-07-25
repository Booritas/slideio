#
# conan center dependencies needed rebuild
#
conan create %CONAN_INDEX_DIR%/recipes/sqlite3/all 3.35.5@_/_ -s build_type=Release
conan create %CONAN_INDEX_DIR%/recipes/sqlite3/all 3.35.5@_/_ -s build_type=Debug
