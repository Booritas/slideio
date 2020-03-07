import os
import glob
import subprocess
import shutil
import sys
from pathlib import Path
import argparse
from argparse import RawTextHelpFormatter

try:
	import distro
except ImportError:
	pass
    
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

def is_linux():
    return get_platform() != "Windows"

def clean_prev_build(build_directory):
    print(F"Cleaning directory {build_directory}")
    if os.path.exists(build_directory):
        shutil.rmtree(build_directory)
    os.makedirs(build_directory)


def collect_profiles(profile_dir, compiler=""):
    compiler_dir = profile_dir
    if is_linux() and compiler=="":
        compiler = "clang-9"
        plt = distro.linux_distribution(full_distribution_name=False)
        print(plt)
        if plt[0] != "ubuntu":
            compiler = "gcc-8"
        compiler_dir = os.path.join(profile_dir, compiler)    

    profiles = []
    for root, dirs, files in os.walk(compiler_dir):
        files = glob.glob(os.path.join(root,'*'))
        for f in files :
            profiles.append(os.path.abspath(f))
    return profiles

def process_conan_profile(profile, trg_dir, conan_file):
    generator = "cmake_multi"
    command = ['conan','install',
        '-pr',profile,
        '-if',trg_dir,
        '-g', generator]
    command.append(conan_file)
    print(command)
    subprocess.check_call(command)

def configure_conan(slideio_dir):
    os_platform = get_platform()
    conan_profile_dir_path = os.path.join(slideio_dir, "conan", os_platform)
    # collect paths to conan profile files
    profiles = collect_profiles(conan_profile_dir_path)
    for trg_conan_file_path in Path(slideio_dir).rglob('conanfile.txt'):
        print("-------Process file: ", trg_conan_file_path)
        for profile in profiles:
            print(F"Profile:{profile}")
            process_conan_profile(profile, os.path.dirname(trg_conan_file_path), trg_conan_file_path.absolute().as_posix())

def single_configuration(config_name, build_dir, project_dir):
    os_platform = get_platform()
    cmake_props = {
        "CMAKE_CXX_STANDARD_REQUIRED":"ON",
    }
    architecture = None
    if os_platform=="Windows":
        generator = 'Visual Studio 16 2019'
        cmake = "cmake.exe"
        architecture = 'x64'
    else:
        generator = 'Unix Makefiles'
        cmake = "cmake"
        cmake_props["CMAKE_BUILD_TYPE"] = config_name


    cmd = [cmake, "-G", generator]
    if architecture is not None:
        cmd += ["-A", "x64"]
    cmd += ["-DCMAKE_CXX_STANDARD=14"]

    for pname, pvalue in cmake_props.items():
        cmd.append(F'-D{pname}={pvalue}')

    cmd = cmd + ["-S", project_dir, "-B", build_dir]
    print(cmd)
    subprocess.check_call(cmd, stderr=subprocess.STDOUT)

def configure_slideio(configuration):
    slideio_dir = configuration["project_directory"]
    build_dir = configuration["build_directory"]
    platform = get_platform()
    print("Start configuration")
    if platform=="Windows":
        single_configuration("", configuration["build_directory"], slideio_dir)
    else:
        if configuration["release"]:
            single_configuration("Release",configuration["build_release_directory"], slideio_dir)
        if configuration["debug"]:
            single_configuration("Debug",configuration["build_debug_directory"], slideio_dir)

def build_slideio(configuration):
    os_platform = get_platform()
    print("Start build")
    if os_platform=="Windows":
        cmake = "cmake.exe"
    else:
        cmake = "cmake"

    if configuration["release"]:
        cmd = [cmake, "--build", configuration["build_release_directory"], "--config", "Release"]
        print(cmd)
        subprocess.check_call(cmd, stderr=subprocess.STDOUT)
    if configuration["debug"]:
        cmd = [cmake, "--build", configuration["build_debug_directory"], "--config", "Debug"]
        print(cmd)
        subprocess.check_call(cmd, stderr=subprocess.STDOUT)

if __name__ == "__main__":
    action_help = """Type of action:
        conan:      run conan to prepare cmake files for 3rd party packages
        configure:  run cmake to configure the build
        build:      build the software
        install:    install the software"""
    config_help = "Software configuration to be configured and build. Select from release, debug or all."
    parser = argparse.ArgumentParser(formatter_class=RawTextHelpFormatter, description='Configuration, building and installation of the slideio library.')
    parser.add_argument('-a','--action', choices=['conan','configure','build', 'test', 'install'], default='configure', help=action_help)
    parser.add_argument('-c', '--config', choices=['release','debug', 'all'], default='all', help = config_help)
    parser.add_argument('--clean', action='store_true', help = 'Clean before build. Add this flag if you want to clean build folders before the build.')
    args = parser.parse_args()
    os_platform = get_platform()
    slideio_directory = os.getcwd()
    root_directory = os.path.dirname(slideio_directory)
    build_directory = os.path.join(slideio_directory, "build")
    print("----------Installattion of slideio-----------------")
    print(F"Slideio directory: {slideio_directory}")
    print(F"Build directory: {build_directory}")
    print("---------------------------------------------------")

    if args.clean:
        clean_prev_build(build_directory)

    configuration = {
        "project_directory": slideio_directory,
        "debug":True,
        "release":True,
        "build_directory" : build_directory,
        "build_release_directory": build_directory,
        "build_debug_directory": build_directory
    }
    if os_platform == "Linux":
        configuration["build_release_directory"] = os.path.join(build_directory,"release")
        configuration["build_debug_directory"] = os.path.join(build_directory,"debug")

    if args.config == 'debug':
        configuration["release"] = False
    if args.config == 'release':
        configuration["debug"] = False

    configure_conan(slideio_directory)
    if args.action in ['configure','build']:
        configure_slideio(configuration)
    if args.action in ['build']:
        build_slideio(configuration)