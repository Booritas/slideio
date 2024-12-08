conan --version
# Check if the environment variables are set
if (-not $env:CONAN_INDEX_HOME) {
    throw "Environment variable CONAN_INDEX_HOME  (root directory of conan-center-indes reporistory) is not set."
}

if (-not $env:SLIDEIO_HOME) {
    throw "Environment variable SLIDEIO_HOME (root directory of SlideIO repository) is not set."
}

# Save the current directory
$originalDir = Get-Location

function Invoke-ConanCreate {
    param (
        [string]$FolderPath,
        [string]$Version
    )

    # Change to the target directory
    $targetDir = Join-Path -Path $env:CONAN_INDEX_HOME -ChildPath $FolderPath
    Set-Location -Path $targetDir

    # Execute the conan create command
    $conanCommand = "conan create -pr:h $env:SLIDEIO_HOME/conan/Windows/x86_64_release -pr:b $env:SLIDEIO_HOME/conan/Windows/x86_64_release -b missing --version $Version --user slideio --channel stable ."
    Invoke-Expression $conanCommand
}

try {
    Invoke-ConanCreate -FolderPath "recipes\glog\all" -Version "0.6.0"
    Invoke-ConanCreate -FolderPath "recipes\opencv\4.x" -Version "4.10.0"
    Invoke-ConanCreate -FolderPath "recipes\jpegxrcodec\all" -Version "1.0.3"
    Invoke-ConanCreate -FolderPath "recipes\ndpi-libjpeg-turbo\all" -Version "2.1.2"
    Invoke-ConanCreate -FolderPath "recipes\ndpi-libtiff\all" -Version "4.3.0"
    Invoke-ConanCreate -FolderPath "recipes\pole\all" -Version "1.0.4"
}
catch {
    Write-Error "An error occurred: $_"
}
finally {
    # Change back to the original directory
    Set-Location -Path $originalDir
}