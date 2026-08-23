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
