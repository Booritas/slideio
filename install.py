import os
import glob
import subprocess
import shutil
import sys
from pathlib import Path
import platform
import argparse
from argparse import RawTextHelpFormatter
import fnmatch
try:
	import distro
except ImportError:
	pass

patterns = [
    "CMakePresets.json",
]


def remove_files_by_patterns(root_dir, patterns):
    for root, dirs, files in os.walk(root_dir):
        for pattern in patterns:
            for filename in fnmatch.filter(files, pattern):
                file_path = os.path.join(root, filename)
                if os.path.isfile(file_path):
                    print(f"Removing file: {file_path}")
                    os.remove(file_path)

def remove_cmake_directories(root_dir):
    """
    Recursively delete all directories named 'cmake' starting from root_dir.

    :param root_dir: The root directory to start the search from
    """
    for root, dirs, files in os.walk(root_dir, topdown=False):
        for dir_name in dirs:
            if dir_name == 'cmake':
                dir_path = os.path.join(root, dir_name)
                print(f"Removing directory: {dir_path}")
                shutil.rmtree(dir_path)
    
def get_platform():
    platforms = {
        'linux' : 'Linux',
        'linux1' : 'Linux',
        'linux2' : 'Linux',
        'darwin' : 'OSX',
        'win32' : 'Windows'
    }
    if sys.platform not in platforms:
        return sys.platform
    
    return platforms[sys.platform]


def is_linux():
    platform = get_platform()
    return platform == "Linux"

def is_osx():
    platform = get_platform()
    return platform == "OSX"


def clean_prev_build(slideio_directory, build_directory):
    print(F"Cleaning directory {build_directory}")
    if os.path.exists(build_directory):
        shutil.rmtree(build_directory)
    os.makedirs(build_directory)
    remove_files_by_patterns(slideio_directory, patterns)
    remove_cmake_directories(slideio_directory)    

def is_debug_profile(path):
    file_name = os.path.basename(path).lower();
    return file_name.find("debug") > 0

def is_release_profile(path):
    file_name = os.path.basename(path).lower();
    return file_name.find("release") > 0

def collect_profiles(profile_dir, configuration, compiler=""):
    compiler_dir = profile_dir
    if is_linux() and compiler=="":
        compiler = "ubuntu"
        plt = distro.id()
        print(plt)
        if plt[0] != "ubuntu":
            compiler = "manylinux"
        compiler_dir = os.path.join(profile_dir, compiler)
    if is_osx():
        if platform.processor()=="arm":
            compiler_dir =  os.path.join(profile_dir, 'arm')
        else:
            compiler_dir =  os.path.join(profile_dir, 'x86-64')
    print("Collect profiles from:", compiler_dir)
    profiles = []
    for root, dirs, files in os.walk(compiler_dir):
        files = glob.glob(os.path.join(root,'*'))
        for f in files :
            profiles.append(os.path.abspath(f))
    return profiles

def process_conan_profile(profile, trg_dir, conan_file, build_folder):
    build_libs = []
    build_libs.append('missing')
    command = ['conan','install',
        '-pr:b',profile,
        '-pr:h',profile,
        '-of', build_folder,
        '-g', 'CMakeDeps',
        '-g', 'CMakeToolchain',
        ]
    for lib in build_libs:
        command.append('-b')
        command.append(lib)
    command.append(conan_file)
    print(command)
    subprocess.check_call(command)

def process_conan_file(profiles, configuration, trg_conan_file_path):
    # root_path = configuration["project_directory"]
    file_directory = os.path.dirname(trg_conan_file_path)
    # relative_path = os.path.relpath(file_directory, root_path)
    cmake_build_path = os.path.join(file_directory, "cmake")
    for profile in profiles:
        print(F"Profile:{profile}")
        release = is_release_profile(profile)
        debug = is_debug_profile(profile)
        if (debug and configuration["debug"]) \
            or (release and configuration["release"]) \
            or (not debug and not release): 
                process_conan_profile(profile, os.path.dirname(trg_conan_file_path), trg_conan_file_path.absolute().as_posix(), cmake_build_path)
                
def configure_conan(slideio_dir, configuration):
    os_platform = get_platform()
    conan_profile_dir_path = os.path.join(slideio_dir, "conan", os_platform)
    # collect paths to conan profile files
    profiles = collect_profiles(conan_profile_dir_path, configuration)
    print(F"Detected profiles:{profiles}")
    
    src_dir = os.path.join(slideio_dir,"src")
    main_conan_file_path = os.path.join(slideio_dir, "conanfile.txt")
    if os.path.exists(main_conan_file_path):
        process_conan_file(profiles, configuration, Path(main_conan_file_path))
    for trg_conan_file_path in Path(src_dir).rglob('conanfile.*'):
        print("-------Process file: ", trg_conan_file_path)
        process_conan_file(profiles, configuration, trg_conan_file_path)

def single_configuration(config_name, build_dir, project_dir):
    os_platform = get_platform()
    cmake_props = {}
    architecture = None
    if os_platform=="Windows":
        generator = 'Visual Studio 17 2022'
        cmake = "cmake.exe"
        architecture = 'x64'
    elif os_platform == "OSX":
        generator = 'Unix Makefiles'
        cmake = "cmake"
        cmake_props["CMAKE_BUILD_TYPE"] = config_name
    else:
        generator = 'Unix Makefiles'
        cmake = "cmake"
        cmake_props["CMAKE_BUILD_TYPE"] = config_name
        plt = distro.id()
        if plt == 'centos':
            cmake_props["CMAKE_CXX_FLAGS"] = "-D_GLIBCXX_USE_CXX11_ABI=0" # Needed for multilinux
            
    cmake_props["CMAKE_TOOLCHAIN_FILE"] = "./cmake/conan_toolchain.cmake"

    cmd = [cmake, "-G", generator]
    if architecture is not None:
        cmd += ["-A", "x64"]

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

def install_slideio(configuration, prefix):
    os_platform = get_platform()
    print("Start build")
    if os_platform=="Windows":
        cmake = "cmake.exe"
    else:
        cmake = "cmake"

    if configuration["release"]:
        cmd = [cmake, "--install", configuration["build_release_directory"], "--prefix", prefix["release"], "--config", "Release"]
        print(cmd)
        subprocess.check_call(cmd, stderr=subprocess.STDOUT)
    if configuration["debug"]:
        cmd = [cmake, "--install", configuration["build_debug_directory"],"--prefix", prefix["debug"], "--config", "Debug"]
        print(cmd)
        subprocess.check_call(cmd, stderr=subprocess.STDOUT)


def install_slideio(configuration, prefix):
    os_platform = get_platform()
    print("Start build")
    if os_platform=="Windows":
        cmake = "cmake.exe"
    else:
        cmake = "cmake"

    if configuration["release"]:
        cmd = [cmake, "--install", configuration["build_release_directory"], "--prefix", prefix["release"], "--config", "Release"]
        print(cmd)
        subprocess.check_call(cmd, stderr=subprocess.STDOUT)
    if configuration["debug"]:
        cmd = [cmake, "--install", configuration["build_debug_directory"],"--prefix", prefix["debug"], "--config", "Debug"]
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
    parser.add_argument('-a','--action', choices=['conan','configure', 'configure-only', 'build', 'build-only', 'install', 'install-only', 'clean'], default='configure', help=action_help)
    parser.add_argument('-c', '--config', choices=['release','debug', 'all'], default='all', help = config_help)
    parser.add_argument('--clean', action='store_true', help = 'Clean before build. Add this flag if you want to clean build folders before the build.')
    parser.add_argument('--prefix', help = 'Path to the installation directory')
    args = parser.parse_args()
    os_platform = get_platform()
    slideio_directory = os.getcwd()
    root_directory = os.path.dirname(slideio_directory)
    build_directory = os.path.join(slideio_directory, "build", os_platform)
    install_directory = os.path.join(slideio_directory, "install", os_platform)
    print("----------Installattion of slideio-----------------")
    print(F"Slideio directory: {slideio_directory}")
    print(F"Build directory: {build_directory}")
    print(F"Platform: {platform.system()}")
    print(F"Processor: {platform.processor()}")
    print("---------------------------------------------------")

    if args.clean:
        clean_prev_build(slideio_directory, build_directory)

    configuration = {
        "project_directory": slideio_directory,
        "debug":True,
        "release":True,
        "build_directory" : build_directory,
        "build_release_directory": build_directory,
        "build_debug_directory": build_directory
    }
    if is_linux():
        print("------------Linux detected----------------")
    if is_osx():
        print("------------Apple detected----------------")
        
    if is_linux() or is_osx():
        configuration["build_release_directory"] = os.path.join(build_directory,"release")
        configuration["build_debug_directory"] = os.path.join(build_directory,"debug")

    if args.config == 'debug':
        configuration["release"] = False
    if args.config == 'release':
        configuration["debug"] = False
    if args.action in ['clean']:
        clean_prev_build(slideio_directory, build_directory)
    else:        
        if args.action in ['conan', 'configure', 'build', 'install']:
            configure_conan(slideio_directory, configuration)
        if args.action in ['configure','configure-only', 'build', 'install']:
            configure_slideio(configuration)
        if args.action in ['build','build-only', 'install']:
            build_slideio(configuration)
        if args.action in ['install','install-only']:
            prefix = {}
            prefix_path = args.prefix
            if not prefix_path:
                prefix_path = install_directory
            if args.config == 'release':
                prefix["release"] = prefix_path
            elif args.config == 'debug':
                prefix["debug"] = prefix_path
            else:
                prefix["release"] = os.path.join(prefix_path, "release")
                prefix["debug"] = os.path.join(prefix_path, "debug")
            install_slideio(configuration, prefix)
