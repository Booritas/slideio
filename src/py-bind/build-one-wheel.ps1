$ErrorActionPreference = "Stop"
conda activate p311

$pythonPath = (Get-Command python).Source
$pythonDir = Split-Path -Parent $pythonPath
$pythonLibDir = Join-Path $pythonDir "libs"
$version = (python --version).Split()[1]

Write-Host "Python path: $pythonPath"
Write-Host "Python directory: $pythonDir"        
Write-Host "Python version: $version"        

$env:PYTHON_EXECUTABLE = $pythonPath
$env:Python3_EXECUTABLE = $pythonPath
$pythonVersion = (python --version).Split()[1]
$env:Python3_VERSION = $pythonVersion
$env:Python3_INCLUDE_DIRS = Join-Path $pythonDir "include"
$env:Python3_LIBRARY_DIRS = $pythonLibDir
$env:Python3_NumPy_INCLUDE_DIRS = python -c "import numpy; print(numpy.get_include())"
$env:Python3_LIBRARIES = Join-Path $pythonLibDir "python$($version.Replace('.', '')).lib"
$env:PYTHON_LOCATION_IS_SET = 1

# Build distributable
Write-Host "Executing command in conda environment for Python $version"
python --version
Write-Host "Installing wheel in conda environment for Python $version"

try {
    Set-Location -Path .\src
    python setup.py sdist bdist_wheel
    Get-ChildItem -Path .\dist  
}
finally {
    Set-Location -Path ..\.
}


