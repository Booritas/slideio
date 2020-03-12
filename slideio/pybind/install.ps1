$pythons = @("3.5","3.6","3.7","3.8")
for($p=0; $p -lt $pythons.Length; $p++)
{
   $pversion = "python/" +  $pythons[$p] + "@slideio/stable"
   Write-Output $pversion
   & conan install $pversion -g json -if conan
   $json = Get-Content -Raw -Path conan/conanbuildinfo.json | ConvertFrom-Json
   $python_exe = Join-Path -Path $json.dependencies.bin_paths[0] -ChildPath "python.exe"
   & $python_exe -m pip install --user --upgrade setuptools wheel
   & $python_exe setup.py sdist bdist_wheel
}