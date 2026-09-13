// Consumer smoke test for the binary distributions.
//
// This is not a unit test and it deliberately reads no slide: it is the check
// that a *package* is usable. Everything it touches is something packaging can
// silently get wrong -- a header left out of the -dev package, a driver library
// missing from the archive, a runtime path that resolves in the build tree and
// nowhere else, a version string that does not match the package name. None of
// those show up in the library's own test suites, which run against the build
// tree.
//
// It is built by auxfiles/package-smoke/CMakeLists.txt as a standalone project
// against the installed package through find_package(slideio), never as part of
// the slideio build.

#include <slideio/slideio/slideio.hpp>

#include <algorithm>
#include <iostream>
#include <string>
#include <vector>

#ifndef SLIDEIO_EXPECTED_VERSION
#error "SLIDEIO_EXPECTED_VERSION must be defined by the build"
#endif

namespace {

// Every format the library serves. A driver that failed to reach the package
// is invisible until someone opens a file of that format, so the list is
// spelled out rather than counted: a count would still pass if one driver were
// swapped for another.
const std::vector<std::string> kExpectedDrivers = {
    "AFI", "CZI", "DCM", "GDAL", "NDPI", "OMETIFF",
    "PHTIFF", "QPTIFF", "SCN", "SVS", "VSI", "ZVI",
};

} // namespace

int main()
{
    int failures = 0;

    const std::string version = slideio::getVersion();
    std::cout << "slideio version: " << version << std::endl;
    if (version != std::string(SLIDEIO_EXPECTED_VERSION)) {
        std::cerr << "FAIL: the library reports version " << version
                  << ", but the package claims " << SLIDEIO_EXPECTED_VERSION
                  << std::endl;
        ++failures;
    }

    const std::vector<std::string> drivers = slideio::getDriverIDs();
    std::cout << "drivers (" << drivers.size() << "):";
    for (const std::string& driver : drivers) {
        std::cout << ' ' << driver;
    }
    std::cout << std::endl;

    for (const std::string& expected : kExpectedDrivers) {
        if (std::find(drivers.begin(), drivers.end(), expected) == drivers.end()) {
            std::cerr << "FAIL: driver " << expected
                      << " is missing from the installed package" << std::endl;
            ++failures;
        }
    }

    if (failures != 0) {
        std::cerr << failures << " check(s) failed" << std::endl;
        return 1;
    }

    std::cout << "package smoke test OK" << std::endl;
    return 0;
}
