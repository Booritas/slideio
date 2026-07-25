#!/usr/bin/env python3
"""Sync Conan profiles (and CMake generator on Windows) with the host compiler.

Run this script before building SlideIO to ensure the Conan profiles under
``conan/<Platform>/`` match your installed compiler version.

On Windows the ``compiler.version`` in the Conan profiles and the CMake
generator string in ``install.py`` are both updated.  On Linux (GCC) and
macOS (Apple Clang) only the Conan profiles are synced because the CMake
generator ("Unix Makefiles") does not vary with the compiler version.

Usage::

    python sync-toolchain.py
"""

import os
import re
import subprocess
import sys


# ---------------------------------------------------------------------------
# Platform helpers
# ---------------------------------------------------------------------------

def get_platform():
    """Return 'Linux', 'OSX', or 'Windows'."""
    platforms = {
        "linux": "Linux",
        "linux1": "Linux",
        "linux2": "Linux",
        "darwin": "OSX",
        "win32": "Windows",
    }
    return platforms.get(sys.platform, sys.platform)


# ---------------------------------------------------------------------------
# MSVC → Conan / CMake mapping
# ---------------------------------------------------------------------------

# Each entry: (_MSC_VER_min, _MSC_VER_max, conan_compiler_version, cmake_generator, toolset)
#
# _MSC_VER & Conan compiler.version reference:
#   https://learn.microsoft.com/en-us/cpp/overview/compiler-versions
#
# Visual Studio 2022 (17.x) spans two toolset generations:
#   1930–1939  →  v143 toolset   (VS 2022 17.0 – 17.9)
#   1940–1949  →  v144 toolset   (VS 2022 17.10+)
_MSVC_TOOLCHAIN_MAP = [
    (1930, 1939, 193, "Visual Studio 17 2022", "v143"),
    (1940, 1949, 194, "Visual Studio 17 2022", "v144"),
    (1950, 1959, 195, "Visual Studio 18 2026", "v145"),
]


def msvc_to_cmake_generator(msc_ver):
    """Map ``_MSC_VER`` (e.g. 1944) to a CMake generator string.

    Returns:
        str or None: The CMake generator string (e.g.
                     ``"Visual Studio 17 2022"``), or ``None`` if the
                     version is unrecognised.
    """
    for lo, hi, _conan_ver, gen, _toolset in _MSVC_TOOLCHAIN_MAP:
        if lo <= msc_ver <= hi:
            return gen
    return None


# ---------------------------------------------------------------------------
# MSVC compiler detection
# ---------------------------------------------------------------------------

def detect_msvc_conan_version():
    """Detect the Conan compiler.version from the installed MSVC compiler.

    Runs ``cl.exe`` (expected on PATH from a Visual Studio developer
    environment) and parses its banner to extract the compiler version.
    Computes the Conan compiler.version as ``_MSC_VER // 10``.

    Returns:
        int or None: The Conan compiler.version (e.g., 194), or None
                     if detection fails.
    """
    if get_platform() != "Windows":
        return None

    try:
        cl_result = subprocess.run(
            ["cl.exe"], capture_output=True, text=True, timeout=10, check=False
        )
    except FileNotFoundError:
        print(
            "Warning: cl.exe not found on PATH. "
            "Run from a Visual Studio developer command prompt."
        )
        return None

    output = cl_result.stdout + cl_result.stderr
    # cl.exe banner: "Microsoft (R) C/C++ Optimizing Compiler Version 19.44.35207 for x64"
    match = re.search(r"Version\s+(\d+)\.(\d+)\.(\d+)", output)
    if not match:
        print("Warning: Could not parse cl.exe version from output")
        return None

    cl_major = int(match.group(1))
    cl_minor = int(match.group(2))
    msc_ver = cl_major * 100 + cl_minor
    conan_version = msc_ver // 10
    print(
        f"Detected cl.exe {match.group(1)}.{match.group(2)}.{match.group(3)}, "
        f"_MSC_VER={msc_ver}, Conan compiler.version={conan_version}"
    )
    return conan_version


def detect_msvc_full_version():
    """Return ``(conan_version, _MSC_VER, cmake_generator)`` or ``None``."""
    conan_ver = detect_msvc_conan_version()
    if conan_ver is None:
        return None

    # Re-derive _MSC_VER from conan_version (approximate but sufficient for lookup)
    msc_ver = conan_ver * 10
    generator = msvc_to_cmake_generator(msc_ver)
    if generator is None:
        print(
            f"Warning: Unrecognised _MSC_VER ~{msc_ver}. "
            "Add an entry to _MSVC_TOOLCHAIN_MAP in sync-toolchain.py."
        )
        return None

    return (conan_ver, msc_ver, generator)


# ---------------------------------------------------------------------------
# Conan profile updates
# ---------------------------------------------------------------------------

def update_conan_profile_compiler_version(profile_path, new_version):
    """Update the compiler.version in a Conan profile file.

    Reads the profile, replaces any line starting with ``compiler.version=``
    with the detected value, and writes back only if a change is needed.

    Args:
        profile_path: Path to the Conan profile file.
        new_version: The new compiler.version value (int or str).

    Returns:
        bool: True if the file was updated, False otherwise.
    """
    if not os.path.isfile(profile_path):
        print(f"Warning: Profile not found: {profile_path}")
        return False

    new_version = str(new_version)
    updated = False
    lines = []

    with open(profile_path, "r", encoding="utf-8") as f:
        for line in f:
            stripped = line.strip()
            if stripped.startswith("compiler.version="):
                old_value = stripped.split("=", 1)[1]
                if old_value == new_version:
                    lines.append(line)
                else:
                    lines.append(f"compiler.version={new_version}\n")
                    print(
                        f"Updated {os.path.basename(profile_path)}: "
                        f"compiler.version={old_value} -> {new_version}"
                    )
                    updated = True
            else:
                lines.append(line)

    if updated:
        with open(profile_path, "w", encoding="utf-8") as f:
            f.writelines(lines)

    return updated


def sync_profiles_in_dir(profiles_dir, compiler_version):
    """Walk *profiles_dir* and update ``compiler.version`` in every Conan profile.

    Every regular file found beneath *profiles_dir* is treated as a Conan
    profile candidate.  Files that do not contain a ``compiler.version=``
    line are silently skipped.

    Returns:
        bool: ``True`` if at least one profile was updated.
    """
    any_updated = False
    for root, _dirs, files in os.walk(profiles_dir):
        for name in files:
            profile_path = os.path.join(root, name)
            if update_conan_profile_compiler_version(profile_path, compiler_version):
                any_updated = True
    return any_updated


# ---------------------------------------------------------------------------
# GCC compiler detection (Linux)
# ---------------------------------------------------------------------------

def detect_gcc_version():
    """Detect the Conan compiler.version from the installed GCC.

    Runs ``gcc -dumpversion`` and returns the major version component.
    On some distributions ``-dumpversion`` returns a bare major ("14");
    on others it returns a full dotted version ("14.2.0").  We always
    keep only the part before the first dot.

    Returns:
        str or None: The Conan compiler.version (e.g. ``"14"``), or
                     ``None`` if detection fails.
    """
    try:
        result = subprocess.run(
            ["gcc", "-dumpversion"],
            capture_output=True, text=True, timeout=10, check=False,
        )
    except FileNotFoundError:
        print("Warning: gcc not found on PATH.")
        return None

    raw = result.stdout.strip()
    if not raw:
        print("Warning: gcc -dumpversion returned empty output")
        return None

    # Keep only the major component (before the first dot, if any)
    major = raw.split(".")[0]
    if not major.isdigit():
        print(f"Warning: Unexpected gcc -dumpversion output: {raw}")
        return None

    print(f"Detected GCC {raw}, Conan compiler.version={major}")
    return major


# ---------------------------------------------------------------------------
# Apple Clang compiler detection (macOS)
# ---------------------------------------------------------------------------

def detect_apple_clang_version():
    """Detect the Conan compiler.version from the installed Apple Clang.

    Runs ``clang --version`` and parses the banner, which looks like::

        Apple clang version 16.0.0 (clang-1600.0.26.6)

    The Conan ``compiler.version`` is the first two components of the
    version (e.g. ``"16.0"``).

    Returns:
        str or None: The Conan compiler.version (e.g. ``"16.0"``), or
                     ``None`` if detection fails.
    """
    try:
        result = subprocess.run(
            ["clang", "--version"],
            capture_output=True, text=True, timeout=10, check=False,
        )
    except FileNotFoundError:
        print("Warning: clang not found on PATH.")
        return None

    output = result.stdout + result.stderr
    match = re.search(r"Apple clang version (\d+)\.(\d+)\.(\d+)", output)
    if not match:
        print("Warning: Could not parse Apple Clang version from output")
        return None

    major, minor = match.group(1), match.group(2)
    conan_version = f"{major}.{minor}"
    print(
        f"Detected Apple Clang {match.group(1)}.{match.group(2)}.{match.group(3)}, "
        f"Conan compiler.version={conan_version}"
    )
    return conan_version


# ---------------------------------------------------------------------------
# CMake generator update in install.py
# ---------------------------------------------------------------------------

def update_cmake_generator_in_install_py(repo_dir, generator):
    """Replace the CMake generator string inside ``single_configuration()``.

    Only the ``generator`` line inside the ``if os_platform == "Windows":``
    branch is touched; Linux/macOS branches are left alone.
    """
    install_py = os.path.join(repo_dir, "install.py")
    if not os.path.isfile(install_py):
        print(f"Warning: install.py not found at {install_py}")
        return False

    with open(install_py, "r", encoding="utf-8") as f:
        content = f.read()

    # Match: generator = "Visual Studio <major> <year>"
    pattern = r'(if os_platform\s*==\s*"Windows":\s*\n\s+)(generator\s*=\s*"Visual Studio \d+ \d{4}")'
    match = re.search(pattern, content)
    if not match:
        print("Warning: Could not locate the CMake generator line in install.py")
        return False

    old_line = match.group(2)
    new_line = f'generator = "{generator}"'
    if old_line == new_line:
        print(f"install.py generator already set to: {generator}")
        return False

    new_content = content[: match.start(2)] + new_line + content[match.end(2):]
    with open(install_py, "w", encoding="utf-8") as f:
        f.write(new_content)

    print(f"Updated install.py: {old_line} -> {new_line}")
    return True


# ---------------------------------------------------------------------------
# Sync orchestration (Windows)
# ---------------------------------------------------------------------------

def sync_windows(repo_dir):
    """Run the full Windows toolchain sync."""
    print("=== Syncing Windows toolchain ===")
    detected = detect_msvc_full_version()
    if detected is None:
        print(
            "ERROR: Could not detect MSVC compiler. "
            "Run this script from a Visual Studio developer command prompt."
        )
        sys.exit(1)

    conan_ver, msc_ver, generator = detected
    print(f"  Conan compiler.version : {conan_ver}")
    print(f"  _MSC_VER               : ~{msc_ver}")
    print(f"  CMake generator        : {generator}")

    profiles_dir = os.path.join(repo_dir, "conan", "Windows")
    profiles_updated = sync_profiles_in_dir(profiles_dir, conan_ver)
    generator_updated = update_cmake_generator_in_install_py(repo_dir, generator)

    if not profiles_updated and not generator_updated:
        print("Everything is already up to date.")
    else:
        print("\nSync complete. Next steps:")
        print("  1. Run: python install.py -a conan")
        print("  2. Run: python install.py -a build")
    print("=== Done ===")


def sync_linux(repo_dir):
    """Run the full Linux toolchain sync."""
    print("=== Syncing Linux toolchain ===")
    gcc_version = detect_gcc_version()
    if gcc_version is None:
        print(
            "ERROR: Could not detect GCC compiler. "
            "Make sure gcc is installed and on PATH."
        )
        sys.exit(1)

    print(f"  GCC major version : {gcc_version}")
    profiles_dir = os.path.join(repo_dir, "conan", "Linux")
    profiles_updated = sync_profiles_in_dir(profiles_dir, gcc_version)

    if not profiles_updated:
        print("All Linux Conan profiles are already up to date.")
    else:
        print("\nSync complete. Next steps:")
        print("  1. Run: python install.py -a conan")
        print("  2. Run: python install.py -a build")
    print("=== Done ===")


def sync_osx(repo_dir):
    """Run the full macOS toolchain sync."""
    print("=== Syncing macOS toolchain ===")
    clang_version = detect_apple_clang_version()
    if clang_version is None:
        print(
            "ERROR: Could not detect Apple Clang compiler. "
            "Make sure Xcode command-line tools are installed (xcode-select --install)."
        )
        sys.exit(1)

    print(f"  Apple Clang version : {clang_version}")
    profiles_dir = os.path.join(repo_dir, "conan", "OSX")
    profiles_updated = sync_profiles_in_dir(profiles_dir, clang_version)

    if not profiles_updated:
        print("All macOS Conan profiles are already up to date.")
    else:
        print("\nSync complete. Next steps:")
        print("  1. Run: python install.py -a conan")
        print("  2. Run: python install.py -a build")
    print("=== Done ===")


# ---------------------------------------------------------------------------
# Main
# ---------------------------------------------------------------------------

def main():
    repo_dir = os.path.dirname(os.path.abspath(__file__))
    platform_name = get_platform()

    dispatchers = {
        "Windows": sync_windows,
        "Linux": sync_linux,
        "OSX": sync_osx,
    }

    dispatcher = dispatchers.get(platform_name)
    if dispatcher is None:
        print(f"Unsupported platform: {platform_name}")
        sys.exit(1)

    dispatcher(repo_dir)


if __name__ == "__main__":
    main()
