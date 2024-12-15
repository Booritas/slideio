#!/bin/bash
# Check if the OS is Ubuntu
if [ "$(lsb_release -is)" = "Ubuntu" ]; then
    profile=$SLIDEIO_HOME/conan/Linux/ubuntu/linux_release
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

invoke_conan_create "recipes/glog/all" "0.6.0"
invoke_conan_create "recipes/opencv/4.x" "4.10.0"
invoke_conan_create "recipes/jpegxrcodec/all" "1.0.3"
invoke_conan_create "recipes/ndpi-libjpeg-turbo/all" "2.1.2"
invoke_conan_create "recipes/ndpi-libtiff/all" "4.3.0"
invoke_conan_create "recipes/pole/all" "1.0.4"

# Change back to the original directory
cd "$original_dir" || exit