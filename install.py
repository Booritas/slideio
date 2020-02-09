import os
import glob
import subprocess
import shutil
import sys
from pathlib import Path

    
def get_platform():
    platforms = {
        'linux' : 'Linux',
        'linux1' : 'Linux',
        'linux2' : 'Linux',
        'darwin' : 'OS X',
        'win32' : 'Windows'
    }
    if sys.platform not in platforms:
        return sys.platform
    
    return platforms[sys.platform]

def clean_prev_build(build_directory):
    print(F"Cleaning directory {build_directory}")
    if os.path.exists(build_directory):
        shutil.rmtree(build_directory)
    os.makedirs(build_directory)


def collect_profiles(profile_dir):
    profiles = []
    for root, dirs, files in os.walk(profile_dir):
        files = glob.glob(os.path.join(root,'*'))
        for f in files :
            profiles.append(os.path.abspath(f))
    return profiles

def process_conan_profile(profile, trg_dir, conan_file):
    platform = get_platform()
    generator = "cmake_multi"
    command = ['conan','install',
        '-pr',profile,
        '-if',trg_dir,
        '-g', generator,
        conan_file]
    print(command)
    subprocess.check_call(command)

def configure_conan(slideio_dir):
    platform = get_platform()
    conan_profile_dir_path = os.path.join(slideio_dir, "conan", platform)
    # collect paths to conan profile files
    profiles = collect_profiles(conan_profile_dir_path)
    for trg_conan_file_path in Path(slideio_dir).rglob('conanfile.txt'):
        print("-------Process file: ", trg_conan_file_path)
        for profile in profiles:
            print(F"Profile:{profile}")
            process_conan_profile(profile, os.path.dirname(trg_conan_file_path), trg_conan_file_path.absolute().as_posix())


if __name__ == "__main__":
    platform = get_platform()
    slideio_directory = os.getcwd()
    root_directory = os.path.dirname(slideio_directory)
    build_directory = os.path.join(root_directory, "slideio_build")
    print("----------Installattion of slideio-----------------")
    print(F"Slideio directory: {slideio_directory}")
    print(F"Build directory: {build_directory}")
    print("---------------------------------------------------")

    configure_conan(slideio_directory)
