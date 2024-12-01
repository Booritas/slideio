#!/bin/bash
set -e

# Check OS and platform
os=$(uname -s)
platform=$(uname -m)
minversion=7

if [[ "$os" == "Darwin" && "$platform" == "arm64" ]]; then
  # Set an environment variable if OS is macOS and platform is ARM
  minversion=8
fi


# Define an array of Python version strings
# Define a function to create an array of Python versions
generate_python_versions() {
   min_version=$1
   max_version=$2
   # Initialize an empty array
   python_versions=()
   # Loop from the minimum to the maximum version
   for ((version=min_version; version<=max_version; version++)); do
      python_versions+=("3.$version")
   done
   # Print the generated array
   echo ${python_versions[@]}
}

create_and_activate_conda_env() {
  version=$1
  # Create a conda environment for each Python version
  echo "Creating conda environment for Python $version"
  conda create -y -n "env_python_$version" python=$version
  # Activate the environment
  echo "Activating conda environment for Python $version"
  conda activate "env_python_$version"
}

deactivate_and_remove_conda_env() {
  version=$1
  # Deactivate the environment
  echo "Deactivating conda environment for Python $version"
  conda deactivate
  
  # Remove the environment
  echo "Removing conda environment for Python $version"
  conda remove -y -n "env_python_$version" --all
  echo "-----end of processing python version $version"
}

generate_python_versions minversion 12
rm -rf ./dist
eval "$(conda shell.bash hook)"

for version in "${python_versions[@]}"; do

   echo "-----processing python verion $version"

   create_and_activate_conda_env $version
   
   # Build distributable
   rm -rf ./build
   rm -rf ../../build_py
   echo "Executing command in conda environment for Python $version"
   python --version
   echo "Installing wheel in conda environment for Python $version"
   python -m pip install wheel
   python -m pip install conan==1.65
   python setup.py sdist bdist_wheel
   ls -la ./dist
   
   deactivate_and_remove_conda_env $version
done

cd ./dist && rename 's/macosx_12_0_x86_64/macosx_10_5_x86_64/' *.whl && rename 's/macosx_14_0_arm64/macosx_11_0_arm64/'