# CPack configuration for the binary distributions.
#
# Included from the root CMakeLists.txt after the install() rules, because
# every CPACK_ variable has to be set before include(CPack) reads them.
#
# One build tree produces every artifact for its platform; which one you get is
# chosen by the generator and the component list, both of which install.py's
# `package` action supplies:
#
#   Windows  ZIP  Runtime+Development  -> slideio-<v>-windows-x86_64.zip
#            ZIP  DebugSymbols         -> slideio-<v>-windows-x86_64-pdb.zip
#   macOS    TGZ  Runtime+Development  -> slideio-<v>-macos-arm64.tar.gz
#   Linux    DEB  Runtime              -> libslideio<major>.<minor>_<v>_amd64.deb
#            DEB  Development          -> libslideio-dev_<v>_amd64.deb

set(CPACK_PACKAGE_NAME "slideio")
set(CPACK_PACKAGE_VENDOR "slideio")
set(CPACK_PACKAGE_CONTACT "admin@slideio.com")
set(CPACK_PACKAGE_HOMEPAGE_URL "https://www.slideio.com/")
set(CPACK_PACKAGE_VERSION ${PROJECT_VERSION})
set(CPACK_PACKAGE_VERSION_MAJOR ${PROJECT_VERSION_MAJOR})
set(CPACK_PACKAGE_VERSION_MINOR ${PROJECT_VERSION_MINOR})
set(CPACK_PACKAGE_VERSION_PATCH ${PROJECT_VERSION_PATCH})
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY
    "C++ library for reading medical and microscopy slide images")
set(CPACK_RESOURCE_FILE_LICENSE "${CMAKE_SOURCE_DIR}/LICENSE.md")
set(CPACK_RESOURCE_FILE_README "${CMAKE_SOURCE_DIR}/README.md")

# CPack's own default strips nothing and lays the tree out under a versioned
# subdirectory; both are wanted here, so only the name is overridden.
set(CPACK_PACKAGE_FILE_NAME "slideio-${PROJECT_VERSION}-${SLIDEIO_PLATFORM_TAG}")
set(CPACK_VERBATIM_VARIABLES ON)

# install.py passes CPACK_COMPONENTS_ALL on the cpack command line to choose
# what goes into each artifact. That only has an effect when component install
# is switched on for the generator: with it off, CPack runs every install rule
# and ignores the list entirely -- which is how the first attempt here produced
# a "pdb" archive byte-for-byte the size of the full one.
set(CPACK_COMPONENTS_ALL Runtime Development)

if(WIN32 OR APPLE)
    if(WIN32)
        set(CPACK_GENERATOR "ZIP")
    else()
        set(CPACK_GENERATOR "TGZ")
    endif()

    # ALL_COMPONENTS_IN_ONE keeps the selected components together in a single
    # archive named CPACK_PACKAGE_FILE_NAME, rather than one archive per
    # component with the component name appended. Set only on this side of the
    # branch: the same variable would collapse the two .deb files into one.
    set(CPACK_ARCHIVE_COMPONENT_INSTALL ON)
    set(CPACK_COMPONENTS_GROUPING ALL_COMPONENTS_IN_ONE)

    # These archives are flat: bin/, lib/ and include/ sit at the root, with no
    # enclosing slideio-<version>-<platform>/ directory. That is not a choice --
    # component install suppresses the top-level directory and
    # CPACK_INCLUDE_TOPLEVEL_DIRECTORY does not bring it back. Unpack into a
    # directory of your own, and treat that directory as CMAKE_PREFIX_PATH.
else()
    set(CPACK_GENERATOR "DEB")

    # One .deb per component rather than one holding both.
    set(CPACK_DEB_COMPONENT_INSTALL ON)
    # DEB-DEFAULT spells the file names the Debian way -- <package>_<version>_<arch>.deb
    # -- instead of reusing CPACK_PACKAGE_FILE_NAME.
    set(CPACK_DEBIAN_FILE_NAME DEB-DEFAULT)
    set(CPACK_PACKAGING_INSTALL_PREFIX "/usr")

    # The runtime package is named for the SONAME, which is major.minor: a
    # minor release of slideio is not ABI compatible with the one before it
    # (2.10 added virtuals to CVScene), so libslideio2.10 and libslideio2.9 are
    # different packages and can be installed side by side. The -dev package
    # keeps a stable name and pins the exact runtime it was built against.
    set(CPACK_DEBIAN_RUNTIME_PACKAGE_NAME
        "libslideio${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}")
    set(CPACK_DEBIAN_DEVELOPMENT_PACKAGE_NAME "libslideio-dev")
    set(CPACK_DEBIAN_DEVELOPMENT_PACKAGE_DEPENDS
        "${CPACK_DEBIAN_RUNTIME_PACKAGE_NAME} (= ${PROJECT_VERSION})")
    set(CPACK_DEBIAN_RUNTIME_PACKAGE_SECTION "libs")
    set(CPACK_DEBIAN_DEVELOPMENT_PACKAGE_SECTION "libdevel")

    # dpkg-shlibdeps derives Depends: from what the libraries actually link,
    # rather than from a hand-written guess that goes stale. It matters here
    # that the third-party dependencies are static and libstdc++ is linked
    # statically too (-static-libstdc++, set at the top of CMakeLists.txt), so
    # the computed list is short -- essentially libc6 and libgomp1.
    set(CPACK_DEBIAN_PACKAGE_SHLIBDEPS ON)
    set(CPACK_DEBIAN_PACKAGE_GENERATE_SHLIBS ON)

    set(CPACK_DEBIAN_PACKAGE_DESCRIPTION
        "C++ library for reading medical and microscopy slide images\n\
 SlideIO reads whole-slide and microscopy images through a pluggable driver\n\
 architecture, covering SVS, AFI, SCN, CZI, ZVI, NDPI, VSI, DCM, QPTIFF,\n\
 OME-TIFF, Philips TIFF and formats reachable through GDAL. It supports zoom\n\
 pyramids, multidimensional (3D and time-series) images and block reads with\n\
 arbitrary scaling.")
endif()

include(CPack)

# Names and descriptions only. REQUIRED, DISABLED and DEPENDS are deliberately
# absent: they steer the component selection UI of the installer generators
# (WiX, NSIS, productbuild), none of which is used here, and DISABLED in
# particular risks a generator dropping a component that install.py asked for by
# name. What each artifact contains is decided by CPACK_COMPONENTS_ALL on the
# cpack command line, and the -dev package pins its runtime through
# CPACK_DEBIAN_DEVELOPMENT_PACKAGE_DEPENDS above.
cpack_add_component(Runtime
    DISPLAY_NAME "Runtime libraries"
    DESCRIPTION "The slideio shared libraries and their format drivers.")
cpack_add_component(Development
    DISPLAY_NAME "Development files"
    DESCRIPTION "Public headers, import libraries and the CMake package config.")
cpack_add_component(DebugSymbols
    DISPLAY_NAME "Debug symbols"
    DESCRIPTION "MSVC PDB files matching the release libraries.")
