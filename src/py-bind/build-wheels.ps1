
# Define a function to create an array of Python versions
function Generate-PythonVersions {
   param (
      [int]$minVersion,
      [int]$maxVersion
   )
   $pythonVersions = @()
   for ($version = $minVersion; $version -le $maxVersion; $version++) {
      $pythonVersions += "3.$version"
   }
   return $pythonVersions
}

function Create-And-Activate-CondaEnv {
   param (
      [string]$version
   )
   # Create a conda environment for each Python version
   Write-Host "Creating conda environment for Python $version"
   conda create -y -n "env_python_$version" python=$version
   # Activate the environment
   Write-Host "Activating conda environment for Python $version"
   conda activate "env_python_$version"
}

function Deactivate-And-Remove-CondaEnv {
   param (
      [string]$version
   )
   # Deactivate the environment
   Write-Host "Deactivating conda environment for Python $version"
   conda deactivate
   
   # Remove the environment
   Write-Host "Removing conda environment for Python $version"
   conda remove -y -n "env_python_$version" --all
   Write-Host "-----end of processing python version $version"
}

$pythonVersions = Generate-PythonVersions -minVersion 6 -maxVersion 12
Remove-Item -Recurse -Force ./dist
& conda shell.powershell hook

foreach ($version in $pythonVersions) {

   Write-Host "-----processing python version $version"

   Create-And-Activate-CondaEnv -version $version
   
   # Build distributable
   Remove-Item -Recurse -Force ./build
   Write-Host "Executing command in conda environment for Python $version"
   python --version
   Write-Host "Installing wheel in conda environment for Python $version"
   python -m pip install wheel
   python -m pip install conan==1.64
   python setup.py sdist bdist_wheel
   
   Deactivate-And-Remove-CondaEnv -version $version
}