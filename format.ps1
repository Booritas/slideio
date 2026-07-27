$files = (git ls-files --exclude-standard);

$extensions = ".cpp", ".h", ".hpp", ".c", ".cc", ".cxx", ".hxx", ".ixx", ".inl"

foreach ($file in $files) {
    if ((Get-Item $file).Extension -in $extensions) {
        clang-format -i -style=file $file
    }
}