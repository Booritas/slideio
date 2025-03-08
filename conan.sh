#!/bin/bash
set -e

#!/bin/bash

# Function to detect the operating system
detect_os() {
    if [ "$(uname)" == "Darwin" ]; then
        echo "OSX"
        return 0
    elif [ -f /etc/os-release ]; then
        . /etc/os-release
        case "$ID" in
            almalinux)
                if [ "$(uname -m)" == "x86_64" ]; then
                    echo "AlmaLinux-x86_64"
                elif [ "$(uname -m)" == "s390x" ]; then
                    echo "AlmaLinux-s390x"
                else
                    echo "unknown"
                fi
                return 0
                ;;
            ubuntu)
                echo "Ubuntu"
                return 0
                ;;
            centos)
                if [ "$(uname -m)" == "s390x" ]; then
                    echo "CentOS-s390x"
                    return 0
                else
                    echo "CentOS"
                    return 0
                fi
                ;;
            *)
                echo "unknown"
                return 0
                ;;
        esac
    else
        echo "unknown"
        return 0
    fi
}

os_name=$(detect_os)
echo "Operating system: $os_name"

if [ "$os_name" == "AlmaLinux-x86_64" ]; then
    release_profile=$SLIDEIO_HOME/conan/Linux/manylinux/linux_release
    debug_profile=$SLIDEIO_HOME/conan/Linux/manylinux/linux_debug
elif [ "$os_name" == "Ubuntu" ]; then
    release_profile=$SLIDEIO_HOME/conan/Linux/ubuntu/linux_release
    debug_profile=$SLIDEIO_HOME/conan/Linux/ubuntu/linux_debug
elif [ "$sos_name" == "AlmaLinux-s390x" ]; then
    release_profile=$SLIDEIO_HOME/conan/Linux/s390x/linux_release
    debug_profile=$SLIDEIO_HOME/conan/Linux/s390x/linux_debug
elif [ "$os_name" == "OSX" ]; then
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
elif [ "$os_name" == "unknown" ]; then
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
    invoke_conan_create "recipes/dcmtk/all" "3.6.8"
    invoke_conan_create "recipes/glog/all" "0.7.1"
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
