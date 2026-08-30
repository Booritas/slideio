# Breaking Changes

There is no changelog in this repository yet; this file is a short, factual
record of exported-API changes that break out-of-tree callers, so a consumer
upgrading past a given branch/commit knows what to check. Entries are grouped
by branch.

---

## v2.10.0

### `slideio-base` was merged into `slideio-core`

**Modules:** `slideio-base` (removed), `slideio-core` (exported: `SLIDEIO_CORE_EXPORTS`)
**Files:** all of `src/slideio/base/` → `src/slideio/core/`

`slideio-base` no longer exists. Its types now live in `slideio-core`, which is
the bottom layer of the module hierarchy. The base/core split was a distinction
without an architectural rule: nothing decided which of the two a new
fundamental type belonged in, and every target that linked `slideio-base`
already linked `slideio-core`.

Three separate things break for out-of-tree callers.

**1. Include paths moved.** For the six installed headers that survive:

| Was | Now |
|---|---|
| `slideio/base/rect.hpp` | `slideio/core/rect.hpp` |
| `slideio/base/size.hpp` | `slideio/core/size.hpp` |
| `slideio/base/range.hpp` | `slideio/core/range.hpp` |
| `slideio/base/resolution.hpp` | `slideio/core/resolution.hpp` |
| `slideio/base/slideio_enums.hpp` | `slideio/core/slideio_enums.hpp` |
| `slideio/base/slideio_structs.hpp` | `slideio/core/slideio_structs.hpp` |

Fails at preprocessing with a file-not-found. The fix is a path swap.

**2. `base.hpp` and `slideio_base_def.hpp` were deleted.** Also preprocess-time,
but the fix is not a path swap. `slideio/base/base.hpp` was an umbrella over
`exceptions.hpp` and `slideio_enums.hpp`; include whichever of the two you
actually use (`exceptions.hpp` is not an installed header — if you relied on it,
you were including a private header). `SLIDEIO_BASE_EXPORTS` is gone; the macro
is now `SLIDEIO_CORE_EXPORTS`, from `slideio/core/slideio_core_def.hpp`.

**3. The `slideio-base` binary is no longer built, installed, or shipped.** A
build script that links or copies `slideio-base`, `libslideio-base.so`,
`libslideio-base.dylib`, `slideio-base.dll`, or their `_d` debug variants fails
at link or load time with no source-level hint. The migration is to drop the
entry: `slideio-core` provides those symbols, and every consumer that linked
`slideio-base` already linked `slideio-core`.

Reachability of these symbols before this change differed by platform. The
top-level `CMakeLists.txt`'s `ARCHIVE DESTINATION lib` block never listed
`${BASE_LIB_NAME}` — only `RUNTIME DESTINATION bin` did — so `slideio-base.lib`
was never installed on Windows. An out-of-tree consumer working from the
install tree got `slideio-base.dll` with no import library and could not link
`logMessage`, `setLogThreshold`, `logThresholdPtr`, `compressionToString`, or
`RuntimeError::log` at all. On Linux and macOS, `libslideio-base.so`/`.dylib`
was installed and default visibility made those same symbols linkable. This
merge improves the Windows case: `${CORE_LIB_NAME}` is in the `ARCHIVE
DESTINATION lib` block, so `slideio-core.lib` is installed and those symbols
are linkable on Windows for the first time.

### `NDPITIFFMessageHandler` is no longer copyable

**Module:** `slideio-ndpi` (exported: `SLIDEIO_NDPI_EXPORTS`)
**File:** `src/slideio/drivers/ndpi/ndpitiffmessagehandler.hpp`

The copy constructor and copy assignment operator are now `= delete`. Declaring
copy deleted also suppresses the implicit move constructor and move assignment
operator, so the type is neither copyable nor movable.

The class is RAII over libtiff's process-global error and warning handlers: it
saves them in the constructor and restores them in the destructor. A copy saved
the same two handlers twice and restored them twice, the second restore
overwriting whatever the intervening scope had installed. There was no in-tree
copy of it; every use is a stack local or a `std::unique_ptr` member, and the
`unique_ptr` moves the pointer rather than the object.

Migration for callers: hold it by value in the scope that needs it, or through
a `unique_ptr`/`shared_ptr`. There is nothing a copy did that a second
default-constructed instance does not do correctly.

How reachable this was out of tree differs by platform, and the entry is kept for the
cautious case: the class carried no export macro before this change, so on Windows it
was not in `slideio-ndpi`'s import library and no out-of-tree caller could link it at
all. On Linux and macOS default visibility left it linkable, so a consumer there could
have copied one.

### `NDPITIFFKeeper` moved header and became move-only

**Module:** `slideio-ndpi`
**Files:** `src/slideio/drivers/ndpi/ndpitiffkeeper.hpp` (new),
`src/slideio/drivers/ndpi/ndpitifftools.hpp`

The class was defined inline in `ndpitifftools.hpp` and was not exported. It now
lives in its own `ndpitiffkeeper.hpp` and is `SLIDEIO_NDPI_EXPORTS`.

Removed:

- The copy constructor and copy assignment operator (`= delete`).
- `operator libtiff::TIFF*()` (implicit conversion to the raw handle).
- `operator=(libtiff::TIFF*)`.

Changed:

- Both constructors are `explicit`; a `(const std::string& filePath)`
  constructor was added, and it installs the message handler before opening.
- A move constructor and move assignment operator were added.

Migration for callers: the same as for `TIFFKeeper` under v2.9.0 — call
`.getHandle()` where the implicit conversion was relied on, and `.reset(handle)`
where a raw handle was assigned in. The old `operator=` overwrote the member
without closing what it replaced, and leaked.

Note also a **behaviour** change that is not an API break: both keeper constructors
now install `NDPITIFFMessageHandler`, which the keeper previously never did. The
driver already installed one as a stack local at its own entry points
(`ndpiimagedriver.cpp:26`, `ndpiscene.cpp:132`, `:369`, `:418`), so on those paths
nothing changes — the keeper's handler simply nests inside the driver's. What changes
is code that reaches libtiff outside those scopes, by calling `NDPITiffTools`
directly: NDPI libtiff errors there now raise a runtime error instead of printing to
stderr. See `software-docs/TECH_DEBT.md` §1 problem 6.

### The log line's thread-id field is decimal again on macOS and Linux

**Module:** `slideio-core` (exported: the three functions in `logcontract.hpp`)
**File:** `src/slideio/core/log.cpp`

No signature changed; what changed is the text `slideio` writes to stderr, which
the logging design spec (§4.6) pins field-for-field because users' log scrapers
read it. The thread-id field was produced by streaming
`std::this_thread::get_id()`, and the formatting of `std::thread::id` is
implementation defined. libstdc++ and MSVC print a decimal number, which matched
the glog format the spec reproduces, but libc++ prints the underlying `pthread_t`
as a hex pointer, so on macOS (and any libc++ build of Linux) the line came out
as:

```
E20260824 15:07:46.676435 0x1f2755d80 widget.cpp:42] payload 9021
```

where §4.6 specifies a decimal OS thread id in that position. It now asks the OS
directly -- `GetCurrentThreadId` on Windows, `pthread_threadid_np` on macOS,
`gettid` elsewhere -- and prints:

```
E20260824 15:40:52.832592 5686447 exceptions.cpp:10] payload 9021
```

**Who is affected.** Only non-MSVC, non-libstdc++ builds, where this restores
the documented format rather than departing from it. A scraper written against a
macOS build's actual output -- matching `0x[0-9a-f]+` in the third field, or
skipping a fixed number of characters to reach the location field -- will stop
matching. A scraper written against §4.6, or against a Windows or libstdc++
build, is unaffected. The numeric value is not comparable across platforms or
with the previous value on the same platform; it identifies a thread within one
process run and nothing more.

### Driver file-pattern matching changed on Linux and macOS

**Module:** `slideio-core` (exported)
**Files:** `src/slideio/core/tools/tools.cpp`, `src/slideio/drivers/ome-tiff/otimagedriver.cpp`

No signature changed. What changed is which files `ImageDriver::canOpenFile`
claims, and therefore which driver `ImageDriverManager` selects. Two separate
corrections, pulling in opposite directions.

**1. Matching is now case-insensitive off Windows.** `Tools::matchPattern` used
`PathMatchSpecW` on Windows and `wildmat` elsewhere; the first ignores case and
the second does not. A slide named `SCAN.OME.TIFF` or `IMAGE.SVS` opened on
Windows and failed with "Cannot find driver for file" on Linux and macOS. Both
platforms now behave as Windows always has, so `canOpenFile` returns `true` for
upper- and mixed-case extensions where it previously returned `false`. Out-of-tree
code that relied on the rejection -- using `canOpenFile` to filter a directory
listing, say -- will now see those files accepted. Folding is ASCII-only, so a
non-ASCII extension is still matched byte-for-byte off Windows, which Windows
itself would fold.

**2. The OME-TIFF driver no longer claims names merely ending in "ome".** Its
pattern listed `*ome.tiff` without the separating dot, so `genome.tiff` and
`myome.tiff` matched. Since `ImageDriverManager` takes the first driver whose
`canOpenFile` answers `true`, such a file could be routed to the OME-TIFF driver
and fail there rather than reaching the driver that should have handled it.
This one was wrong on every platform, Windows included, and the fix narrows
matching: files of that shape are no longer claimed.

### OpenCV moved to 4.14.0 from conan center

**Modules:** all (the OpenCV-based interface: `CVSlide`, `CVScene`, `CVTools`)
**Files:** every `conanfile.txt`/`conanfile.py` under `src/`, `build-dependencies.sh`,
`build-dependencies.ps1`, `auxfiles/upload-dependencies.sh`,
`auxfiles/remove-dependencies.sh`

The requirement is now `opencv/4.14.0`, the conan center recipe, in place of
`opencv/4.10.0@slideio/stable`. No signature changed, but the OpenCV-based
interface passes `cv::Mat`, `cv::Rect` and `cv::Size` across the library
boundary, so an out-of-tree caller has to be compiled against the same OpenCV
slideio was. Linking a consumer built against 4.10.0 to a slideio built against
4.14.0 is undefined; rebuild the consumer.

The `@slideio/stable` fork existed for one reason: with `imgcodecs=False` --
which every profile in `conan/` sets -- OpenCV 4.10.0's
`modules/highgui/src/window_w32.cpp` failed to compile, because the
`showSaveDialog` fallback called `CV_LOG_WARNING` without its tag argument. The
fork carried a patch deleting that line, plus `INSTALL_TESTS=False`. Upstream
now passes `NULL` as the tag, so the patch has nothing left to do and the fork
is retired: `build-dependencies` no longer creates an opencv package, and
`CONAN_INDEX_HOME` is no longer consulted for one.

Every option the profiles set (`ml`, `dnn`, `gapi`, `flann`, `photo`, `video`,
`calib3d`, `videoio`, `imgcodecs`, `objdetect`, `stitching`, `with_png`,
`with_tiff`, `with_webp`, `with_quirc`, `with_ffmpeg`, `with_openexr`,
`with_wayland`, `with_protobuf`, `with_flatbuffers`, and the four
`with_imgcodec_*`) still exists in the 4.14.0 recipe, so the profiles are
unchanged. Those options are not the recipe's defaults, so no prebuilt binary
matches and opencv is built from source on a cold cache; `install.py` already
passes `-b missing`. The upload/remove scripts still name opencv so the private
`slideio` remote can keep caching that build.

### ICU moved to 78.2 from conan center, and s390x support goes with it

**Modules:** `slideio-core` (links `icu::icu` PRIVATE), `slideio-dcm`
**Files:** `src/slideio/core/conanfile.txt`, `src/slideio/drivers/dcm/conanfile.py`,
`build-dependencies.sh`, `build-dependencies.ps1`

`icu/76.1@slideio/stable` becomes `icu/78.2`, the conan center recipe. The fork
is retired and `build-dependencies` no longer creates an ICU package.

**s390x builds are expected to fail.** This is a deliberate trade, not an
oversight. The fork carried two s390x fixes in `recipes/icu/all/conanfile.py`,
and upstream has only one of them. Upstream now picks the ICU data bundle
suffix -- `icudt78b.dat` against `icudt78l.dat` -- from `self.settings.arch`
against a big-endian set that includes `s390x`, which is strictly better than
the fork's `sys.byteorder` test because it also holds when cross-compiling.
What upstream still lacks is `s390x` in the `arch64` list that decides
`--with-library-bits`, so a native s390x build is configured 32-bit and fails
there. The `conan/Linux/s390x/` profiles are left in place; they are not
expected to produce a working build until upstream takes that one-line list
addition.

Nothing in slideio's own sources changes. `Tools::fromUnicode16`
(`src/slideio/core/tools/tools.cpp`) is the only ICU call site, and both things
it touches survive in 78.2: `UChar` is still an unguarded
`typedef char16_t`, and `UnicodeString(const char16_t*, int32_t)` is unchanged.
ICU is linked `PRIVATE` and statically (`icu/*:shared=False` in every profile),
so the versioned `icu_78` symbol namespace does not reach out-of-tree callers.
A consumer that links ICU itself, however, now shares a process with ICU 78
rather than 76.

One incidental gain: `shared=False` matches the recipe default, so conan center
serves a prebuilt ICU binary. The fork always had to be compiled, and ICU is a
slow build. `force=True` stays on the dcm driver's requirement, which is what
keeps dcmtk's own ICU from landing a second version in that graph.

### The NDPI libjpeg-turbo and libtiff forks are submodules, not conan packages

**Module:** `slideio-ndpi`
**Files:** `.gitmodules`, `extern/ndpi-libjpeg-turbo`, `extern/ndpi-tiff`,
`src/slideio/drivers/ndpi/CMakeLists.txt`,
`src/slideio/drivers/ndpi/conanfile.txt`, `cmake-scripts/ndpi-tiff-deps/`,
`install.py`, `build-dependencies.sh`, `build-dependencies.ps1`,
`auxfiles/upload-dependencies.sh`, `auxfiles/remove-dependencies.sh`

`ndpi-libjpeg-turbo/2.1.2@slideio/stable` and `ndpi-libtiff/4.3.0@slideio/stable`
are gone. The same sources now build in-tree from two submodules pinned at the
same tags the recipes fetched -- `extern/ndpi-libjpeg-turbo` at v2.1.2
(6bb4790) and `extern/ndpi-tiff` at v4.3.0 (d23311c). No slideio source file
changed and no exported signature changed.

**A clone now needs its submodules.** `git clone --recurse-submodules`, or
`git submodule update --init` before configuring. Missing directories stop the
configure with a FATAL_ERROR naming them rather than failing later on a header.
This is the same requirement jpegxrcodec introduced; there are three submodules
now, all under `extern/`.

**The two forks had to move together.** ndpi-libtiff required
ndpi-libjpeg-turbo (`recipes/ndpi-libtiff/all/conanfile.py:63`) and re-exported
it, so dropping only the jpeg package would have pulled conan's copy straight
back through `NDPITIFF::NDPITIFF` and put two libjpeg-turbo builds in one
driver. The driver calls `jpeglib.h` directly and so does libtiff, and the fork
is built `WITH_JPEG8` and `WITH_MEM_SRCDST`, both of which change the size of
`jpeg_decompress_struct`. That disagreement surfaces at runtime as libtiff's
"JPEG parameter struct mismatch", not as a link error.

**`install.py` no longer force-builds ndpi-libtiff.** The `-b ndpi-libtiff/*`
entry existed because a prebuilt ndpi-libtiff recorded its jpeg dependency by
version only, so conan considered a binary compiled against a differently
configured libjpeg-turbo still valid. With both built in-tree from one source
tree there is no second configuration to disagree with, and `-b missing` is
enough again.

**Out-of-tree build scripts** that referenced either package -- creating them
from the conan-center-index fork, or uploading them to the `slideio` remote --
should drop those entries. `build-dependencies` and the auxfiles scripts already
have.

Two behaviours worth knowing. The in-tree jpeg builds without SIMD unless NASM
is on PATH, where the published conan binary may have had it; this is a decode
speed difference in the NDPI driver only, not a correctness one, and
`REQUIRE_SIMD` is left at its default so a missing NASM degrades rather than
fails. And the codec set is pinned to what the recipe configured -- zlib,
libdeflate, lzma, jbig, zstd, webp and the C++ API on, lerc and jpeg12 off --
rather than left to libtiff's autodetection.

### pole is a submodule, not a conan package

**Modules:** `slideio-zvi`
**Files:** `.gitmodules`, `extern/pole`, `CMakeLists.txt`,
`src/slideio/drivers/zvi/CMakeLists.txt`,
`src/slideio/drivers/zvi/conanfile.txt`, `src/tests/main/CMakeLists.txt`,
`src/tests/main/conanfile.txt`, `build-dependencies.sh`,
`build-dependencies.ps1`, `auxfiles/upload-dependencies.sh`,
`auxfiles/remove-dependencies.sh`

`pole/1.0.4@slideio/stable` is gone. The same sources now build in-tree from
`extern/pole`, at 3e64e5a -- the v1.0.4 tag the recipe cloned (5f7963f) plus
three fixes for warnings that tag builds with, none of which change behaviour.
No slideio source file changed and no exported signature changed: the zvi
driver still includes `<pole/polepp.hpp>` and `<pole/storage.hpp>`, and pole is
still a static archive hidden inside `slideio-zvi` by
`HIDE_THIRD_PARTY_SYMBOLS`.

**The target is now `pole`, not `pole::pole`,** and there is no
`find_package(pole)`. Two places linked it -- the zvi driver and the main test
suite -- and both changed. An out-of-tree CMake project that linked
`pole::pole` from the conan package is unaffected as long as it keeps using
that package; nothing in slideio's installed interface exposed pole.

**A clone now needs a fourth submodule.** `git clone --recurse-submodules`, or
`git submodule update --init` before configuring; a missing `extern/pole` stops
the configure with a FATAL_ERROR naming it. Plain `--init` is enough -- pole's
nested googletest submodule is only needed for its own tests, which the root
`CMakeLists.txt` forces off through the same `PACKAGE_TESTS` cache entry
jpegxrcodec uses.

**Out-of-tree build scripts** that created pole from the conan-center-index
fork, or uploaded it to the `slideio` remote, should drop those entries;
`build-dependencies` and the auxfiles scripts already have.

One implementation detail is worth knowing, because it is the only thing about
this that is not mechanical. pole publishes no include directory: its own
sources reach their headers by relative path, and the `<pole/...>` prefix every
consumer uses existed only because the recipe copied `includes/` into the
package as `include/pole`. The root `CMakeLists.txt` stages that same shape in
the build tree with `file(COPY)`. It is a configure-time copy, so a header
edited inside the submodule needs a re-configure to be picked up -- where the
package needed a rebuild and upload.

## v2.9.0

### `TIFFKeeper` is now move-only

**Module:** `slideio-imagetools` (exported: `SLIDEIO_IMAGETOOLS_EXPORTS`)
**File:** `src/slideio/imagetools/tiffkeeper.hpp`

Removed:

- The copy constructor and copy assignment operator (`= delete`).
- `operator libtiff::TIFF*()` (implicit conversion to the raw handle).
- `operator=(libtiff::TIFF*)`.
- The `TIFFKeeperPtr` macro. It is now a namespace-scoped
  `using TIFFKeeperPtr = std::shared_ptr<TIFFKeeper>;` inside `namespace slideio`.

Changed:

- Both constructors (`TIFFKeeper(libtiff::TIFF*)` and
  `TIFFKeeper(const std::string&, bool)`) are now `explicit`.
- A move constructor and move assignment operator were added.

Migration for callers:

- Anywhere a `libtiff::TIFF*` was expected and a `TIFFKeeper` was passed
  implicitly, call `.getHandle()` explicitly.
- Anywhere a raw handle was assigned into a keeper with `operator=`, call
  `.reset(handle)` instead. `reset()` closes the handle the keeper already
  holds before taking the new one; the old `operator=` did not, and leaked.
- Anywhere a `TIFFKeeper` was copied, restructure to move it (`std::move`) or
  hold it through a `shared_ptr`/`unique_ptr` instead.

This is a source-breaking change to an exported class. The SlideIO Python
bindings live in a separate repository and were **not** checked as part of
this work — anyone maintaining that repository, or any other out-of-tree
consumer of `slideio-imagetools`, needs to check for direct use of the
removed members before building against this branch.

See `software-docs/specs/2026-08-15-tiffkeeper-ownership-design.md` and
`software-docs/TECH_DEBT.md` §1 for the full rationale.

### `Tools::isXml` was removed

**Module:** `slideio-core` (exported)

`Tools::isXml` was added during Philips TIFF format-detection work to test
whether a string looked like XML. It was never adopted: the intended caller
would have had to parse the document to decide whether to call it, and could
not afford a second parse of an 844 KB description just to answer that
question. It had no caller anywhere in this repository at the time of
removal. Any out-of-tree code calling `Tools::isXml` will need to inline an
equivalent check or drop the dependency.

### `CVScene` gains a virtual method

**Module:** `slideio-core` (exported)
**File:** `src/slideio/core/cvscene.hpp`

`CVScene::readResampledLevelBlockChannelsEx` was added as a virtual method,
which changes the vtable layout of `CVScene` and of every class deriving from
it. This is a binary incompatibility: an out-of-tree driver or an application
linked against slideio 2.8.x must be recompiled against 2.9.0. Source
compatibility is unaffected — no existing signature, default argument or
documented behaviour changed, and the new method carries a working default
implementation, so a driver that does not override it continues to build and
to read correctly.

The Python bindings (separate `slideio-python` repository) must be rebuilt
against 2.9.0. The version appears in `conanfile.txt`,
`build-dependencies.ps1` and `conan.sh` of that repository, and all three
must move together.
