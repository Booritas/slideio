
# Function to generate Python versions
function Generate-PythonVersions {
    param (
        [int]$min_version,
        [int]$max_version
    )
    $python_versions = @()
    for ($version = $min_version; $version -le $max_version; $version++) {
        $python_versions += "3.$version"
    }
    return $python_versions
}

# Function to create and activate conda environment
function Create-CondaEnv {
    param (
        [string]$version
    )
    Write-Host "Creating conda environment for Python $version"
    conda create -q -y -n "env_python_$version" python=$version
}

function Activate-CondaEnv {
    param (
        [string]$version
    )
    Write-Host "Activating conda environment for Python $version"
    conda activate "env_python_$version"
}

# Function to deactivate and remove conda environment
function Deactivate-CondaEnv {
    Write-Host "Deactivating conda environment"
    conda deactivate
}

function Remove-CondaEnv {
    param (
        [string]$version
    )
    conda remove -q -y -n "env_python_$version" --all
    Write-Host "-----end of processing python version $version"
}


function Repair-Wheel {
    param(
        [string]$Directory
    )
    Write-Host "Repairing wheel file in $Directory"

    $PythonVersion = ($extractDir -split '-')[2]
    $FileName = "slideiopybind.$PythonVersion-win_amd64.pyd"

    $TargetFile = Join-Path $Directory $FileName
    if (-not (Test-Path $TargetFile)) {
        $ExistingFile = Get-ChildItem -Path $Directory -Filter "slideiopybind*.pyd" | Select-Object -First 1
        if ($ExistingFile) {
            Rename-Item -Path $existingFile.FullName -NewName $FileName
            Write-Host "Renamed $($existingFile.FullName) to $TargetFile"
            return 1
        }
    }
    return 0
}


function Repair-Naming {

    $WheelFiles = Get-ChildItem -Path .\dist\*.whl

    foreach ($WheelFile in $WheelFiles) {
        Write-Host "Processing wheel file $WheelFile"
        $ExtractDir = $WheelFile.BaseName
        $ZipFile = [System.IO.Path]::ChangeExtension($WheelFile.FullName, "zip")

        Write-Host "Renaming file $WheelFile to $ZipFile"
        Rename-Item -Path $WheelFile.FullName -NewName $ZipFile

        # Unzip the wheel file
        Expand-Archive -Path $ZipFile -DestinationPath $ExtractDir

        # Execute Fix-Wheel function and capture the return value
        $FixWheelResult = Repair-Wheel -Directory $ExtractDir

        # Only compress if Fix-Wheel didn't return 0
        if ($FixWheelResult -ne 0) {
            Remove-Item -Path $ZipFile -Force
            # Create new wheel file
            Compress-Archive -Path "$ExtractDir\*" -DestinationPath $ZipFile -Force
        }
        Rename-Item -Path $ZipFile  -NewName $WheelFile.FullName
        # Delete the temporary directory
        Remove-Item -Path $ExtractDir -Recurse -Force
    }
}

function Create-CondaEnv-For-All-Versions {
    param(
        [string[]]$python_versions
    )
    foreach ($version in $python_versions) {
        Create-CondaEnv -version $version
    }
}

function Remove-All-CondaEnv {
    param(
        [string[]]$python_versions
    )
    foreach ($version in $python_versions) {
        Remove-CondaEnv -version $version
    }
}

function Build-Wheels {
    param(
        [string[]]$python_versions
    )
    foreach ($version in $python_versions) {
        Write-Host "-----processing python version $version"
        Create-CondaEnv -version $version
        Activate-CondaEnv -version $version
        python -m pip install numpy
        try {

            $pythonPath = (Get-Command python).Source
            $pythonDir = Split-Path -Parent $pythonPath
            $pythonLibDir = Join-Path $pythonDir "libs"
    
            $env:PYTHON_EXECUTABLE = $pythonPath
            $env:Python3_EXECUTABLE = $pythonPath
            $pythonVersion = (python --version).Split()[1]
            $env:Python3_VERSION = $pythonVersion
            $env:Python3_INCLUDE_DIRS = Join-Path $pythonDir "include"
            $env:Python3_LIBRARY_DIRS = $pythonLibDir
            $env:Python3_NumPy_INCLUDE_DIRS = python -c "import numpy; print(numpy.get_include())"
            $env:Python3_LIBRARIES = Join-Path $pythonLibDir "python$($version.Replace('.', '')).lib"
            $env:PYTHON_LOCATION_IS_SET = 1
    
            Write-Host "Python path: $pythonPath"
            Write-Host "Python directory: $pythonDir"        
            
            # Build distributable
            Remove-Item -Path .\build -Recurse -Force -ErrorAction SilentlyContinue
            Remove-Item -Path ..\..\build_py -Recurse -Force -ErrorAction SilentlyContinue
            Write-Host "Executing command in conda environment for Python $version"
            python --version
            Write-Host "Installing wheel in conda environment for Python $version"
            python -m pip install wheel
            python -m pip install conan==1.65
        
            python setup.py sdist bdist_wheel
            Get-ChildItem -Path .\dist
        }
        finally {
            Deactivate-CondaEnv
            Remove-CondaEnv -version $version
        }

    }
        
}

