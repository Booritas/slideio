# CI library distribution — design

**Date:** 2026-09-13
**Branch:** `ci-library-distribution`
**Status:** implemented

## Problem

The repository builds on three platforms and ships nothing. `build-validation.yml`
configures and builds on Linux, macOS and Windows and stops there; there is no
CPack configuration, no CMake package config, no release workflow, and no
artifact of any kind leaves CI. A consumer who wants slideio has to clone it,
install Conan, and build it.

The goal is a binary distribution for Windows x86_64, Debian x86_64 and macOS
arm64, published on the GitHub Release for a version tag.

## What makes this cheaper than it looks

Two properties of the existing build do most of the work.

**Every third-party dependency is static.** The Conan profiles set
`tinyxml2/*:shared=False`, `icu/*:shared=False` and take the static default for
OpenCV, DCMTK, libtiff and the rest; `HIDE_THIRD_PARTY_SYMBOLS`
(`CMakeLists.txt`) hides their symbols and `assert_no_thirdparty_exports.cmake`
fails the build if that stops working. Linux also links libstdc++ statically
(`-static-libstdc++`). A distribution is therefore slideio's own sixteen shared
libraries plus headers, and a consumer needs no Conan, no OpenCV and no
dependency graph — only a C++17 runtime.

**`install.py -a install` already produces a sane prefix tree.** The `install()`
rules cover every library and every public header. They needed components and a
layout fix, not a rewrite.

## Decisions

| Question | Decision |
|---|---|
| Trigger | Tag `v*` publishes a draft GitHub Release; `workflow_dispatch` builds and smoke-tests everything without publishing |
| Debian layout | Split `libslideio2.10` (runtime) + `libslideio-dev` (headers, CMake config) |
| Windows / macOS | Plain `.zip` / `.tar.gz`, plus a separate `-pdb.zip` of MSVC release symbols |
| `find_package` | Yes — `slideioConfig.cmake`, consumers link `slideio::slideio` |
| Linux build host | `ubuntu-22.04`, fixing the glibc floor at 2.35 (Ubuntu 22.04+, Debian 12+) |
| SONAME | `major.minor`, Linux only |
| Release gate | Corpus-free unit suites, then a consumer smoke test against the built package |
| macOS floor | `CMAKE_OSX_DEPLOYMENT_TARGET=12.0` |
| Configurations | Release only |

## Three decisions that are not obvious

### The SONAME is Linux-only

A split runtime/`-dev` package needs an SONAME and the project had none. Adding
`VERSION`/`SOVERSION` to the targets unconditionally would have broken macOS
**silently**.

`FIX_MACOS_RPATH` (`CMakeLists.txt`) rewrites install names by matching the
literal strings in `NAME_TOOL_LIB_LIST` — `libslideio-core.dylib` and so on —
and it is applied to the `slideio` library and every test and tool executable.
`SOVERSION` renames those files to `libslideio-core.2.10.dylib`, the literals
stop matching, `install_name_tool -change` becomes a no-op, and the archive
ships binaries whose dependencies cannot be resolved. No error at build time; a
failure to load at the consumer's end.

Windows has no SONAME concept, and nothing but the `.deb` needs one. So the
property is set under `if(UNIX AND NOT APPLE)` only.

`major.minor` rather than `major`: 2.10 added virtual functions to `CVScene`
(`readResampledLevelBlockChannelsEx`, `supportsConcurrentReads`), which breaks
ABI against 2.9. A `libslideio2` package name would assert a compatibility the
project does not maintain, and would let a program built against 2.9 load 2.10
and crash. `libslideio2.9` and `libslideio2.10` are separate packages that can
be installed side by side.

### The CMake package config is written by hand

`install(EXPORT)` is the usual answer and it does not work here without first
reshaping sixteen `CMakeLists.txt` files. Every module links its dependencies
through the plain three-argument `target_link_libraries(tgt a b c)` signature,
which puts `opencv::opencv`, `TIFF::TIFF`, `SQLite::SQLite3` and the rest into
`INTERFACE_LINK_LIBRARIES`. `install(EXPORT)` refuses to export a target whose
interface names an imported target that is not itself in an export set, and the
generated config would then require the consumer to have Conan and the whole
dependency graph — to link against libraries that already contain every one of
those dependencies statically.

`cmake-scripts/slideioConfig.cmake.in` declares the five public modules as
`SHARED IMPORTED` targets, resolves their locations with `find_library` under
the package prefix, and wires the inter-module dependencies explicitly. It
describes what the package actually contains. Converting the link declarations
to `PRIVATE` throughout would be a genuine improvement, but it is a change to
every module's link semantics and does not belong in a packaging branch.

### Libraries are installed to `lib/` only

The old rules installed the shared libraries into `lib/` **and** `bin/` on Unix
— two copies of every `.so`. Harmless in a build tree, not acceptable in a
system package. Windows keeps DLLs in `bin/` and import libraries in `lib/`,
which is the platform convention and what the loader expects.

`lib/` is used rather than the Debian multiarch `lib/x86_64-linux-gnu/`. Getting
the triplet would mean adopting `GNUInstallDirs`, which resolves to `lib64` on
RHEL-family distributions and would change the layout the manylinux Docker flow
depends on. `/usr/lib` is a trusted loader directory on Debian and Ubuntu, so
the package works; it is simply not co-installable with an i386 build, which
nothing produces.

## Components

| Component | Contents | Windows | macOS | Debian |
|---|---|---|---|---|
| `Runtime` | 16 shared libraries | `bin/*.dll` | `lib/*.dylib` | `libslideio2.10` |
| `Development` | import libs, headers, CMake config | `lib/*.lib`, `include/`, `lib/cmake/slideio/` | same, minus import libs | `libslideio-dev` |
| `DebugSymbols` | MSVC PDBs | separate `-pdb.zip` | — | — |

Release PDBs did not previously exist: `CMakeLists.txt` installed them for Debug
only, and a Release MSVC build emits none. `/Zi` and `/DEBUG` are now added for
Release, together with `/OPT:REF` and `/OPT:ICF` — `/DEBUG` disables both, and
without asking for them back the shipped binaries would be larger than before
for no benefit.

## The version has one source of truth

`ImageDriverManager::getVersion()` returns `SLIDEIO_VERSION`, a literal in
`src/slideio/slideio/slideio_def.hpp`. It said `2.9.2` in a 2.10.0 tree.

That is harmless while nobody ships the binaries and actively misleading once
package names and SONAMEs are stamped with `projectVersion`. Three things now
have to agree, and two of them are checked mechanically:

- CMake configure fails if `SLIDEIO_VERSION` and `projectVersion` disagree.
- The release workflow fails if the tag and `projectVersion` disagree, before
  any platform starts building.
- The smoke test compares the version the installed library reports with the
  version the package is named for.

## The release gate

Two steps run before anything is uploaded.

**Corpus-free unit suites** — `FileReader.*` and `ContextPool.*`, the only
suites that need no slide corpus. CLAUDE.md requires CI to leave
`SLIDEIO_SKIP_MISSING_IMAGES` unset, so the rest would fail rather than skip.

**A consumer smoke test** (`auxfiles/package-smoke/`) — a standalone CMake
project built against the artifact just produced: unpacked from the archive, or
installed with `apt-get install ./libslideio*.deb`. It uses `find_package`, links
`slideio::slideio`, and checks that the reported version matches the package and
that all twelve driver ids are present. It knows nothing about the build tree,
has no toolchain file and no Conan.

This is the only step that can catch a header missing from the `-dev` package, a
driver library left out of an archive, a broken config file, or a runtime path
that resolves in the build tree and nowhere else. None of those are visible to
the library's own test suites, which run against the build tree.

## Files

| File | Change |
|---|---|
| `CMakeLists.txt` | Version guard; macOS deployment target; MSVC release PDBs; component-tagged install rules; Linux SOVERSION; package config generation |
| `cmake-scripts/slideioConfig.cmake.in` | New — the CMake package config |
| `cmake-scripts/packaging.cmake` | New — CPack generators, component names, Debian metadata |
| `install.py` | New `package` / `package-only` actions |
| `auxfiles/package-smoke/` | New — the consumer smoke test |
| `.github/workflows/release.yml` | New — tag-triggered build, gate and publish |
| `src/slideio/slideio/slideio_def.hpp` | `SLIDEIO_VERSION` 2.9.2 → 2.10.0 |

`build-validation.yml` is untouched and remains the per-commit gate.

Configuration lives in `cmake-scripts/` and not `cmake/`, because `.gitignore`
excludes `**/cmake/` for Conan-generated files — a script git refuses to track is
a script nobody else gets.

## Building a distribution locally

```bash
python3 install.py -a package -c release     # conan, configure, build, package
python3 install.py -a package-only -c release  # package an existing build
```

Artifacts land in `build/packages`. This is the same path CI takes, so a release
candidate can be inspected before a tag is pushed.

## The archives are flat

`bin/`, `lib/` and `include/` sit at the root of the `.zip` and `.tar.gz`; there
is no enclosing `slideio-<version>-<platform>/` directory.

Not a preference. Component install is what makes `CPACK_COMPONENTS_ALL`
mean anything to an archive generator — with `CPACK_ARCHIVE_COMPONENT_INSTALL`
off, CPack runs every install rule and ignores the component list, which is how
the first attempt here produced a "pdb" archive byte-for-byte the size of the
full one. Turning it on suppresses the top-level directory, and
`CPACK_INCLUDE_TOPLEVEL_DIRECTORY 1` does not bring it back.

So: unpack into a directory of your own and hand that directory to
`CMAKE_PREFIX_PATH`. The workflow's smoke steps do exactly this.

## What was verified, and where

Verified locally on Windows, end to end: configure, full Release build, both
zips, unpack, `find_package(slideio)` from a standalone project, link, run.
The smoke test reports `2.10.0` and all twelve driver ids, and the corpus-free
suites pass (22 tests). The main zip contains 16 DLLs, 16 import libraries, 36
headers and the two CMake config files, and no PDBs; the `-pdb` zip contains the
16 PDBs and nothing else. The version guard was confirmed by breaking the header
deliberately and watching configure fail.

**The Debian and macOS paths have not been run.** There is no Linux or macOS
machine on this development setup, so the `.deb` split, `dpkg-shlibdeps`, the
SOVERSION symlinks, the macOS deployment target and the `@rpath` resolution in
the tarball are all first exercised by the workflow itself. Run it once with
`workflow_dispatch` — which builds, packages and smoke-tests every platform but
publishes nothing — before pushing a tag.

One thing to watch on that first macOS run: the smoke test deliberately does not
set `DYLD_LIBRARY_PATH`, relying on CMake giving the consumer an RPATH from the
imported target's absolute location. `FIX_MACOS_RPATH` rewrites install names to
`@loader_path/...` by matching literal file names, and whether the shipped
dylibs carry `@rpath/` or bare install names decides whether that rewrite is
doing anything. If the smoke test fails to load a library there, that mechanism
is where to look — not the packaging.

## Known limitations

- **Nothing is signed.** The macOS archive is not notarized and the Windows
  binaries carry no Authenticode signature; both will show the usual warnings.
  Signing needs certificates that CI does not have.
- **macOS is arm64 only.** No x86_64 slice and no universal binary.
- **`libslideio-dev` pins an exact runtime version** (`= 2.10.0`). Correct while
  patch releases are ABI-uncertain; a later relaxation to `>=` is a deliberate
  compatibility claim, not a cleanup.
- **The conan profiles keep their default `os.version` on macOS.** Setting the
  12.0 floor there would change every package id and make CI build OpenCV, DCMTK
  and ICU from source. Only slideio's own objects carry the floor; linking them
  against dependencies built for an older target is fine, but a dependency built
  for a *newer* one would produce linker warnings.
