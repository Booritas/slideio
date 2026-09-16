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
import re
import zipfile

try:
    import distro
except ImportError:
    pass
import platform

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


# Directories the cmake cleanup never descends into. The submodules under
# extern/ ship cmake/ directories of their own -- ndpi-tiff keeps libtiff's
# AutotoolsVersion.cmake, CompilerChecks.cmake, FindCMath.cmake and the rest
# there -- and deleting those leaves the tree unconfigurable until the
# submodule is checked out again.
cmake_cleanup_skipped_dirs = ["extern", ".git"]


def remove_cmake_directories(root_dir):
    """
    Recursively delete all directories named 'cmake' starting from root_dir,
    leaving third-party sources under extern/ untouched.

    :param root_dir: The root directory to start the search from
    """
    for root, dirs, files in os.walk(root_dir, topdown=True):
        dirs[:] = [d for d in dirs if d not in cmake_cleanup_skipped_dirs]
        for dir_name in list(dirs):
            if dir_name == "cmake":
                dir_path = os.path.join(root, dir_name)
                print(f"Removing directory: {dir_path}")
                shutil.rmtree(dir_path)
                dirs.remove(dir_name)


def get_platform():
    platforms = {
        "linux": "Linux",
        "linux1": "Linux",
        "linux2": "Linux",
        "darwin": "OSX",
        "win32": "Windows",
    }
    return platforms.get(sys.platform, sys.platform)


def get_processor_type():
    """Detect processor architecture: 'x86_64' or 'arm64'."""
    machine = platform.machine().lower()
    if machine in ("x86_64", "amd64"):
        return "x86_64"
    if machine in ("arm64", "aarch64"):
        return "arm64"
    return machine


def is_linux():
    return get_platform() == "Linux"


def is_osx():
    return get_platform() == "OSX"


def get_linux_distro_name():
    """Return the Linux distribution name (e.g. 'ubuntu', 'centos', 'debian').
    Returns an empty string on non-Linux platforms.
    """
    if not is_linux():
        return ""
    try:
        return distro.id()
    except NameError:
        return ""


def clean_prev_build(slideio_directory, build_directory):
    print(f"Cleaning directory {build_directory}")
    if os.path.exists(build_directory):
        shutil.rmtree(build_directory)
    os.makedirs(build_directory)
    remove_files_by_patterns(slideio_directory, patterns)
    remove_cmake_directories(slideio_directory)


def is_debug_profile(path):
    file_name = os.path.basename(path).lower()
    return file_name.find("debug") > 0


def is_release_profile(path):
    file_name = os.path.basename(path).lower()
    return file_name.find("release") > 0


def collect_profiles(profile_dir, configuration, profile_type=""):
    profile_path = profile_dir
    if is_linux() and profile_type == "":
        arch = platform.machine()
        profile_type = "ubuntu"
        plt = distro.id()
        if plt != "ubuntu":
            profile_type = "manylinux"
        if arch == "s390x":
            profile_type = "s390x"
        profile_path = os.path.join(profile_dir, profile_type)
    if is_osx():
        if platform.processor() == "arm":
            profile_path = os.path.join(profile_dir, "arm")
        else:
            profile_path = os.path.join(profile_dir, "x86-64")
    print("Collect profiles from:", profile_path)
    profiles = []
    for root, dirs, files in os.walk(profile_path):
        files = glob.glob(os.path.join(root, "*"))
        for f in files:
            profiles.append(os.path.abspath(f))
    return profiles


def process_conan_profile(profile, trg_dir, conan_file, build_folder):
    build_libs = []
    build_libs.append("missing")
    command = [
        "conan",
        "install",
        "-pr:b",
        profile,
        "-pr:h",
        profile,
        "-of",
        build_folder,
        "-g",
        "CMakeDeps",
        "-g",
        "CMakeToolchain",
    ]
    for lib in build_libs:
        command.append("-b")
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
        print(f"Profile:{profile}")
        release = is_release_profile(profile)
        debug = is_debug_profile(profile)
        if (
            (debug and configuration["debug"])
            or (release and configuration["release"])
            or (not debug and not release)
        ):
            process_conan_profile(
                profile,
                os.path.dirname(trg_conan_file_path),
                trg_conan_file_path.absolute().as_posix(),
                cmake_build_path,
            )


def configure_conan(slideio_dir, configuration):
    os_platform = get_platform()
    conan_profile_dir_path = os.path.join(slideio_dir, "conan", os_platform)
    # collect paths to conan profile files
    profiles = collect_profiles(conan_profile_dir_path, configuration)
    print(f"Detected profiles:{profiles}")

    src_dir = os.path.join(slideio_dir, "src")
    main_conan_file_path = os.path.join(slideio_dir, "conanfile.txt")
    if os.path.exists(main_conan_file_path):
        process_conan_file(profiles, configuration, Path(main_conan_file_path))
    for trg_conan_file_path in Path(src_dir).rglob("conanfile.*"):
        print("-------Process file: ", trg_conan_file_path)
        process_conan_file(profiles, configuration, trg_conan_file_path)


def single_configuration(config_name, build_dir, project_dir):
    os_platform = get_platform()
    cmake_props = {}
    architecture = None
    if os_platform=="Windows":
        generator = 'Visual Studio 17 2022'
        cmake = "cmake.exe"
        architecture = "x64"
    elif os_platform == "OSX":
        generator = "Unix Makefiles"
        cmake = "cmake"
        cmake_props["CMAKE_BUILD_TYPE"] = config_name
    else:
        generator = "Unix Makefiles"
        cmake = "cmake"
        cmake_props["CMAKE_BUILD_TYPE"] = config_name
        plt = distro.id()
        if plt == "centos":
            cmake_props["CMAKE_CXX_FLAGS"] = (
                "-D_GLIBCXX_USE_CXX11_ABI=0"  # Needed for multilinux
            )

    cmake_props["CMAKE_TOOLCHAIN_FILE"] = "./cmake/conan_toolchain.cmake"

    cmd = [cmake, "-G", generator]
    if architecture is not None:
        cmd += ["-A", "x64"]

    for pname, pvalue in cmake_props.items():
        cmd.append(f"-D{pname}={pvalue}")

    cmd = cmd + ["-S", project_dir, "-B", build_dir]
    print(cmd)
    subprocess.check_call(cmd, stderr=subprocess.STDOUT)


def configure_slideio(configuration):
    slideio_dir = configuration["project_directory"]
    build_dir = configuration["build_directory"]
    platform = get_platform()
    print("Start configuration")
    if platform == "Windows":
        single_configuration("", configuration["build_directory"], slideio_dir)
    else:
        if configuration["release"]:
            single_configuration(
                "Release", configuration["build_release_directory"], slideio_dir
            )
        if configuration["debug"]:
            single_configuration(
                "Debug", configuration["build_debug_directory"], slideio_dir
            )


def build_slideio(configuration):
    os_platform = get_platform()
    print("Start build")
    if os_platform == "Windows":
        cmake = "cmake.exe"
    else:
        cmake = "cmake"

    if configuration["release"]:
        cmd = [
            cmake,
            "--build",
            configuration["build_release_directory"],
            "--config",
            "Release",
        ]
        print(cmd)
        subprocess.check_call(cmd, stderr=subprocess.STDOUT)
    if configuration["debug"]:
        cmd = [
            cmake,
            "--build",
            configuration["build_debug_directory"],
            "--config",
            "Debug",
        ]
        print(cmd)
        subprocess.check_call(cmd, stderr=subprocess.STDOUT)


def install_slideio(configuration, prefix):
    os_platform = get_platform()
    print("Start build")
    if os_platform == "Windows":
        cmake = "cmake.exe"
    else:
        cmake = "cmake"

    if configuration["release"]:
        cmd = [
            cmake,
            "--install",
            configuration["build_release_directory"],
            "--prefix",
            prefix["release"],
            "--config",
            "Release",
        ]
        print(cmd)
        subprocess.check_call(cmd, stderr=subprocess.STDOUT)
    if configuration["debug"]:
        cmd = [
            cmake,
            "--install",
            configuration["build_debug_directory"],
            "--prefix",
            prefix["debug"],
            "--config",
            "Debug",
        ]
        print(cmd)
        subprocess.check_call(cmd, stderr=subprocess.STDOUT)



def read_cpack_package_file_name(build_dir):
    """Base name CPack gives an archive, read out of the generated CPackConfig.cmake.

    Parsed rather than recomputed here on purpose: the platform tag it contains
    is built in cmake-scripts/packaging.cmake, and a second definition in this
    file would eventually disagree with it and have the workflow upload
    artifacts whose names do not match the packages inside them.
    """
    config = os.path.join(build_dir, "CPackConfig.cmake")
    if not os.path.isfile(config):
        raise RuntimeError(
            f"{config} does not exist. Configure and build before packaging."
        )
    with open(config, encoding="utf-8") as handle:
        for line in handle:
            match = re.match(r'\s*set\(CPACK_PACKAGE_FILE_NAME\s+"([^"]+)"\)', line)
            if match:
                return match.group(1)
    raise RuntimeError(f"CPACK_PACKAGE_FILE_NAME is not set in {config}.")


def assert_archive_has_pdbs(archive):
    """Fail if the debug-symbol archive came out empty.

    The PDB install rule is OPTIONAL, because a configuration that emits no PDBs
    must not fail the install. The cost of that is silence: if the release link
    ever stops producing them -- the /Zi and /DEBUG options overridden, a
    generator or policy change -- cpack still writes a perfectly valid zip
    containing nothing, the workflow uploads it because the file exists, and the
    gap is discovered by whoever next tries to symbolise a release crash.
    """
    with zipfile.ZipFile(archive) as handle:
        pdbs = [n for n in handle.namelist() if n.lower().endswith(".pdb")]
    if not pdbs:
        raise RuntimeError(
            f"{archive} contains no .pdb files. The release build stopped "
            f"emitting debug symbols; check the MSVC /Zi and /DEBUG options in "
            f"CMakeLists.txt before shipping this."
        )
    print(f"  {os.path.basename(archive)}: {len(pdbs)} pdb files")


def drop_cpack_staging(output_dir):
    """Remove the _CPack_Packages tree cpack leaves next to the packages.

    It is a second full copy of the installed tree, one per component set, and
    it is larger than everything it was used to build: 97 MB against 42 MB of
    archives on Windows. The release workflow names the package files
    explicitly and would not upload it either way, but leaving it behind makes
    the output directory misleading to anyone looking at it by hand.
    """
    staging = os.path.join(output_dir, "_CPack_Packages")
    if os.path.isdir(staging):
        shutil.rmtree(staging, ignore_errors=True)


def package_slideio(configuration, output_dir):
    """Build the binary distribution artifacts for the current platform.

    Release only. A distribution carries release libraries, and packaging a
    debug build would produce archives full of _d-suffixed libraries that no
    consumer wants and a .deb whose SONAME matches nothing.
    """
    os_platform = get_platform()
    cpack = "cpack.exe" if os_platform == "Windows" else "cpack"

    if not configuration["release"]:
        raise RuntimeError(
            "Distributions are cut from the release build only. Run with -c release."
        )

    build_dir = configuration["build_release_directory"]
    base_name = read_cpack_package_file_name(build_dir)
    os.makedirs(output_dir, exist_ok=True)

    def run_cpack(generator, components, file_name):
        cmd = [
            cpack,
            "-G",
            generator,
            "-C",
            "Release",
            "--config",
            os.path.join(build_dir, "CPackConfig.cmake"),
            "-B",
            output_dir,
            "-D",
            "CPACK_COMPONENTS_ALL=" + ";".join(components),
            "-D",
            "CPACK_PACKAGE_FILE_NAME=" + file_name,
        ]
        print(cmd)
        subprocess.check_call(cmd, stderr=subprocess.STDOUT)

    if os_platform == "Windows":
        run_cpack("ZIP", ["Runtime", "Development"], base_name)
        # The PDBs are several times the size of the libraries they describe,
        # so they ship as their own download rather than inside the archive
        # everybody has to fetch.
        run_cpack("ZIP", ["DebugSymbols"], base_name + "-pdb")
        assert_archive_has_pdbs(os.path.join(output_dir, base_name + "-pdb.zip"))
    elif os_platform == "OSX":
        run_cpack("TGZ", ["Runtime", "Development"], base_name)
    else:
        # CPACK_DEBIAN_FILE_NAME is DEB-DEFAULT, so the two components become
        # libslideio<major>.<minor>_<version>_<arch>.deb and
        # libslideio-dev_<version>_<arch>.deb; the name passed here is ignored.
        run_cpack("DEB", ["Runtime", "Development"], base_name)

    drop_cpack_staging(output_dir)

    print("-------- packages written to", output_dir, "--------")
    for entry in sorted(os.listdir(output_dir)):
        full = os.path.join(output_dir, entry)
        if os.path.isfile(full):
            print(f"  {entry}  ({os.path.getsize(full)} bytes)")


if __name__ == "__main__":
    action_help = """Type of action:
        conan:      run conan to prepare cmake files for 3rd party packages
        configure:  run cmake to configure the build
        build:      build the software
        install:    install the software
        package:    build the binary distribution packages for this platform"""
    config_help = "Software configuration to be configured and build. Select from release, debug or all."
    parser = argparse.ArgumentParser(
        formatter_class=RawTextHelpFormatter,
        description="Configuration, building and installation of the slideio library.",
    )
    parser.add_argument(
        "-a",
        "--action",
        choices=[
            "conan",
            "configure",
            "configure-only",
            "build",
            "build-only",
            "install",
            "install-only",
            "package",
            "package-only",
            "clean",
        ],
        default="configure",
        help=action_help,
    )
    parser.add_argument(
        "-c",
        "--config",
        choices=["release", "debug", "all"],
        default="all",
        help=config_help,
    )
    parser.add_argument(
        "--clean",
        action="store_true",
        help="Clean before build. Add this flag if you want to clean build folders before the build.",
    )
    parser.add_argument("-pr", "--prefix", help="Path to the installation directory")
    parser.add_argument("-bd", "--build_dir", help="Path to the build directory")
    args = parser.parse_args()
    os_platform = get_platform()
    slideio_directory = os.getcwd()
    root_directory = os.path.dirname(slideio_directory)

    build_prefix = args.build_dir
    if not build_prefix:
        build_prefix = "build"
    if os.path.isabs(build_prefix):
        build_directory = build_prefix
    else:
        build_directory = os.path.join(slideio_directory, build_prefix)

    install_directory = args.prefix
    if not install_directory:
        install_directory = os.path.join(build_directory, "install")
    if not os.path.isabs(install_directory):
        install_directory = os.path.join(slideio_directory, install_directory)

    print("----------Installattion of slideio-----------------")
    print(f"Slideio directory: {slideio_directory}")
    print(f"Build directory: {build_directory}")
    print(f"Install directory: {install_directory}")
    print(f"Platform: {platform.system()}")
    print(f"Processor: {platform.processor()}")
    print("---------------------------------------------------")

    if args.clean:
        clean_prev_build(slideio_directory, build_directory)

    configuration = {
        "project_directory": slideio_directory,
        "debug": True,
        "release": True,
        "build_directory": build_directory,
        "build_release_directory": build_directory,
        "build_debug_directory": build_directory,
    }
    if is_linux():
        print("------------Linux detected----------------")
    if is_osx():
        print("------------Apple detected----------------")

    if is_linux() or is_osx():
        configuration["build_release_directory"] = os.path.join(
            build_directory, "release"
        )
        configuration["build_debug_directory"] = os.path.join(build_directory, "debug")

    if args.config == "debug":
        configuration["release"] = False
    if args.config == "release":
        configuration["debug"] = False
    if args.action in ["clean"]:
        clean_prev_build(slideio_directory, build_directory)
    else:
        if args.action in ["conan", "configure", "build", "install", "package"]:
            configure_conan(slideio_directory, configuration)
        if args.action in [
            "configure",
            "configure-only",
            "build",
            "install",
            "package",
        ]:
            configure_slideio(configuration)
        if args.action in ["build", "build-only", "install", "package"]:
            build_slideio(configuration)
        if args.action in ["install", "install-only"]:
            prefix = {
                "release": os.path.join(install_directory, "release"),
                "debug": os.path.join(install_directory, "debug"),
            }
            install_slideio(configuration, prefix)
        if args.action in ["package", "package-only"]:
            package_slideio(configuration, os.path.join(build_directory, "packages"))
