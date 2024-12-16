#!/bin/bash
set -e
if [ "$(uname)" = "Linux" ]; then
    if [ "$(lsb_release -is)" = "Ubuntu" ]; then
        release_profile=$SLIDEIO_HOME/conan/Linux/ubuntu/linux_release
        debug_profile=$SLIDEIO_HOME/conan/Linux/ubuntu/linux_debug
    else
        echo "Error: No Conan profile for this Linux distribution."
        exit 1
    fi
elif [ "$(uname)" = "Darwin" ]; then
    if [ "$(uname -m)" = "arm64" ]; then
        release_profile=$SLIDEIO_HOME/conan/OSX/arm/osx_release
        debug_profile=$SLIDEIO_HOME/conan/OSX/arm/osx_debug
    elif [ "$(uname -m)" = "x86_64" ]; then
        release_profile=$SLIDEIO_HOME/conan/OSX/x86-64/osx_release
        debug_profile=$SLIDEIO_HOME/conan/OSX/x86-64/osx_debug
    else
        echo "Error: No conan profile for this macOS architecture."
        exit 1
    fi
else
    echo "Error: No conan profile for this operating system."
    exit 1
fi

# Function to execute the conan create command
invoke_conan_create() {
    local folder_path=$1
    local version=$2

    # Change to the target directory
    target_dir="$CONAN_INDEX_HOME/$folder_path"
    cd "$target_dir" || exit

    # Execute the conan create command
    conan_command="conan create -pr:h $profile -pr:b $profile -b missing --version $version --user slideio --channel stable ."
    eval "$conan_command"
}

# Save the original directory
original_dir=$(pwd)

create_conan_recipes() {
    invoke_conan_create "recipes/glog/all" "0.6.0"
    invoke_conan_create "recipes/opencv/4.x" "4.10.0"
    invoke_conan_create "recipes/jpegxrcodec/all" "1.0.3"
    invoke_conan_create "recipes/ndpi-libjpeg-turbo/all" "2.1.2"
    invoke_conan_create "recipes/ndpi-libtiff/all" "4.3.0"
    invoke_conan_create "recipes/pole/all" "1.0.4"
}

# Call the function
profile=$release_profile
#create_conan_recipes
profile=$debug_profile
create_conan_recipes

# Change back to the original directory
cd "$original_dir" || exit