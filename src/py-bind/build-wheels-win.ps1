$ErrorActionPreference = "Stop"
$minversion = 7

. .\lib.ps1

try {
    $python_versions = Generate-PythonVersions -min_version $minversion -max_version 12
    Remove-Item -Path .\dist -Recurse -Force -ErrorAction SilentlyContinue

    Build-Wheels -python_versions $python_versions
    Repair-Naming
}
finally {
    Set-Location -Path ..\.
}