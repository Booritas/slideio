# Standalone consumer of an installed slideio package.
#
# Not part of the slideio build. It is configured separately against an
# unpacked distribution, exactly the way somebody who downloaded one would:
#
#   cmake -S auxfiles/package-smoke -B build/smoke \
#         -DCMAKE_PREFIX_PATH=<where the package was unpacked> \
#         -DSLIDEIO_EXPECTED_VERSION=2.10.0
#   cmake --build build/smoke --config Release
#
# The point is that nothing here knows how slideio was built: no conan, no
# toolchain file, no include or library paths. If find_package(slideio) cannot
# supply all of that from the package alone, the package is incomplete and this
# fails to configure -- which is the result we want, on a build machine, before
# the artifact reaches a release page.

cmake_minimum_required(VERSION 3.16)
project(slideio_package_smoke LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(NOT SLIDEIO_EXPECTED_VERSION)
    message(FATAL_ERROR
        "Pass -DSLIDEIO_EXPECTED_VERSION=<version> so the test can check that "
        "the library reports the version the package is named for.")
endif()

# Checked for shape, not just presence, because the interesting failure is a
# value that arrives mangled rather than missing. PowerShell splits a bare
# -DNAME=2.10.0 at the first dot, so cmake receives "2" and a stray path it
# merely warns about; the run then got all the way to comparing the library's
# 2.10.0 against a package claiming "2" before anything complained. Failing here
# names the cause instead.
if(NOT SLIDEIO_EXPECTED_VERSION MATCHES "^[0-9]+[.][0-9]+[.][0-9]+$")
    message(FATAL_ERROR
        "SLIDEIO_EXPECTED_VERSION is '${SLIDEIO_EXPECTED_VERSION}', which is not "
        "a full major.minor.patch version. It was probably split by the shell -- "
        "quote the whole argument, as \"-DSLIDEIO_EXPECTED_VERSION=1.2.3\".")
endif()

# No version argument here on purpose: SLIDEIO_EXPECTED_VERSION is what this
# run is checking, and asking find_package to enforce it too would turn a
# mismatch into a configure error that says less than the runtime comparison
# does.
#
# COMPONENTS is named explicitly even though linking slideio::slideio alone
# would do. A config that publishes a component list has to answer a request for
# one, and the plain form never asks: the first version of this file said only
# find_package(slideio REQUIRED) and so never noticed that the config set no
# slideio_<comp>_FOUND variables, leaving check_required_components guaranteed
# to fail for anyone who did ask.
find_package(slideio REQUIRED
    COMPONENTS slideio core imagetools converter transformer)

add_executable(slideio_package_smoke main.cpp)
target_link_libraries(slideio_package_smoke PRIVATE slideio::slideio)
target_compile_definitions(slideio_package_smoke PRIVATE
    SLIDEIO_EXPECTED_VERSION="${SLIDEIO_EXPECTED_VERSION}")

message(STATUS "slideio found: ${slideio_INCLUDE_DIRS}")
get_target_property(_slideio_location slideio::slideio IMPORTED_LOCATION)
message(STATUS "slideio runtime: ${_slideio_location}")
