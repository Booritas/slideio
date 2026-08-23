# Merging `slideio-base` into `slideio-core`

**Date:** 2026-08-23
**Branch:** v2.10.0
**Status:** Design approved, pending implementation plan

---

## 1. Goal

Dissolve `slideio-base` into `slideio-core`, leaving `slideio-core` as the
bottom layer of the module hierarchy.

The motivation is not binary count. It is that the base/core boundary is
artificial: there is no rule that decides which of the two a new fundamental
type belongs in, so every addition at the bottom of the stack costs a judgment
call that carries no architectural information. Alongside that, the split
imposes real bookkeeping — thirteen link lists name both libraries, a second
export macro exists for eight declarations, and a second per-module Conan tree
is maintained — with no isolation benefit in practice, because every in-tree
target that links `slideio-base` already links `slideio-core`.

After the merge the hierarchy in `CLAUDE.md` drops from 7 modules to 6.

## 2. Relationship to the logging-library replacement

`software-docs/specs/2026-08-22-logging-library-replacement-design.md` §4.1
deliberately placed the logging seam in `slideio-base` *rather than*
`slideio-core`, on the grounds that base sits below everything and carries no
heavy dependencies, while core pulls OpenCV, SQLite3, ZLIB, tinyxml2,
nlohmann_json, and ICU.

This merge reverses that placement, and the reversal is intentional. The
argument in §4.1 was about *relative* position — the log seam must sit at or
below every module that logs — and that property survives the merge, because
`slideio-core` becomes the bottom layer. The dependency-weight half of the
argument was never load-bearing for correctness: no module gains a dependency
here, since nothing linked `slideio-base` without also linking `slideio-core`
(§6.3 lists the evidence).

The one property from that spec that **must** be preserved is the link-time
singleton: spdlog is a static library and the logging state it backs must exist
in exactly one shared library, or `setLogThreshold` and `logThresholdPtr` stop
referring to the same variable. After this merge that one shared library is
`slideio-core`. Preserving this is a hard requirement, not a nicety — see §5.2.

## 3. Scope decisions

Each of these was decided explicitly; alternatives considered are recorded so
the reasoning is not re-litigated later.

| Decision | Chosen | Rejected alternative |
|---|---|---|
| Physical layout | Full move: files to `src/slideio/core/`, all includes rewritten, public headers install to `include/slideio/core/`. `slideio/base/` ceases to exist. | Keeping `src/slideio/base/` as a source directory compiling into the core target ("target-only merge"). Rejected: it deletes the library but leaves the artificial boundary standing as a folder, which is the thing being removed. |
| Compatibility shims | None. | Installing one-line `slideio/base/*.hpp` forwarding headers for one release. Rejected: doubles the surface during the deprecation window and defers the include-path break rather than removing it. |
| `SLIDEIO_BASE_EXPORTS` | Purged. All 8 declarations become `SLIDEIO_CORE_EXPORTS`; `slideio_base_def.hpp` deleted. | Keeping `slideio_base_def.hpp` as an alias defining `SLIDEIO_BASE_EXPORTS` as `SLIDEIO_CORE_EXPORTS`. Rejected: the boundary would survive in the macro namespace. |
| `base/base.hpp` umbrella | Deleted; its 9 consumers include the real headers directly. | Renaming to `core.hpp`. Rejected: an umbrella named after the module but covering 2 of its ~20 headers invites recurring "why isn't `cvscene` in here?" churn. |
| `SLIDEIO_CORE` compile definition | Left alone; recorded in `TECH_DEBT.md`. | Renaming to `SLIDEIO_CORE_API` for consistency with the other 15 modules. Rejected here — see §7.2. |
| Landing strategy | Two commits: mechanical, then judgment-bearing. | Single atomic commit; four staged commits. See §4. |

## 4. Landing strategy

Two commits, both of which build and test green.

**Commit 1 — mechanical.** File moves, the scripted include rewrite, and all
CMake/Conan wiring. Everything required to compile, and nothing requiring
per-file judgment. The rewrite script goes in the commit message so review
verifies the transformation rather than reading 169 one-line diffs.

**Commit 2 — judgment-bearing.** Export-macro purge, `base.hpp` deletion with
per-consumer include expansion, install/packaging lists, `CLAUDE.md`,
`BREAKING_CHANGES.md`, `TECH_DEBT.md`.

Why not finer: moving the files breaks every `#include "slideio/base/..."` in
the same instant, so the move and the include rewrite cannot be separated
without an intermediate commit that does not compile. Commit 1 is therefore the
finest granularity the compiler permits.

Why not coarser: a single commit would interleave 169 mechanical line changes
with the nine hand-read `base.hpp` expansions and the export-macro edits. The
mechanical changes are reviewable only in bulk against a script; the others need
reading. Separating them puts each on the right side of that line.

## 5. Design

### 5.1 File moves

Fifteen files move from `src/slideio/base/` to `src/slideio/core/`, flat,
alongside `cvscene.hpp` and its siblings:

```
exceptions.hpp      exceptions.cpp
slideio_enums.hpp   slideio_enums.cpp
log.hpp             log.cpp
logcontract.hpp
slideio_structs.hpp
resolution.hpp
rect.hpp            rect.inl
size.hpp            size.inl
range.hpp           range.inl
```

Two files are deleted rather than moved: `base.hpp` and
`slideio_base_def.hpp`.

`src/slideio/core/` contains no file with any of those fifteen names, and no
`core.hpp` exists to collide with the deleted umbrella, so the move introduces
no collisions.

`rect.hpp`, `size.hpp`, and `range.hpp` keep their
`#if defined(SLIDEIO_INTERNAL_HEADER)` guard around the corresponding `.inl`
include, and the three `.inl` files stay out of the install list exactly as
today. That arrangement is untouched: the guard macro is added globally at
`CMakeLists.txt:27` and is module-agnostic.

### 5.2 Build system

**Directory teardown.** `src/slideio/CMakeLists.txt:1` loses
`add_subdirectory(base)`. Deleted: `src/slideio/base/CMakeLists.txt`,
`src/slideio/base/conanfile.txt`, `src/slideio/base/CMakeUserPresets.json`, and
the entire checked-in `src/slideio/base/cmake/` tree (26 files).

**`src/slideio/core/CMakeLists.txt`.**

- The fifteen moved paths join `SOURCE_FILES`.
- Add `find_package(spdlog REQUIRED)` and
  `target_link_libraries(${LIBRARY_NAME} PRIVATE spdlog::spdlog)`, carried over
  from `base/CMakeLists.txt:31-32`. `PRIVATE` is required, not stylistic: it is
  what keeps spdlog's symbols from propagating out of the one shared library
  that owns the logging state (§2).
- Delete the trailing `target_link_libraries(${LIBRARY_NAME} PUBLIC
  ${BASE_LIB_NAME})` block at lines 54-56.
- The existing `target_compile_definitions(... PRIVATE SLIDEIO_CORE ...)` at
  line 50 now also governs the moved declarations. `SLIDEIO_BASE_API` is not
  added anywhere; it disappears with `slideio_base_def.hpp`.

**Conan.** `src/slideio/core/conanfile.txt` gains `spdlog/1.17.0` under
`[requires]`. Note that `core/conanfile.txt` currently has no `[generators]`
section while `base/conanfile.txt` declares `CMakeDeps` and `CMakeToolchain`.
`install.py` discovers module conanfiles by `Path(src_dir).rglob("conanfile.*")`
and drives Conan itself; whether it supplies the generators for a file that omits
them must be confirmed during implementation rather than assumed. If core's file
needs `[generators]` added for spdlog's config files to be produced, that
addition is part of commit 1.

Re-running `python install.py -a conan` regenerates `src/slideio/core/cmake/`
with the 14 `spdlog*` and `fmt*` config files and refreshes
`conandeps_legacy.cmake`, `conan_toolchain.cmake`, and `CMakePresets.json`.
Those regenerated files are committed, because this repository checks in the
generated per-module `cmake/` trees.

**`BASE_LIB_NAME` purge.** The variable definition at `CMakeLists.txt:55` is
deleted, along with every use:

| File | Line | Action |
|---|---|---|
| `CMakeLists.txt` | 55 | delete the `set(BASE_LIB_NAME ...)` |
| `CMakeLists.txt` | 76 | drop `lib${BASE_LIB_NAME}.dylib` from `NAME_TOOL_LIB_LIST` |
| `CMakeLists.txt` | 164 | drop from `install(TARGETS ... RUNTIME DESTINATION bin)` |
| `CMakeLists.txt` | 184 | drop from the non-WIN32 `install(TARGETS ... LIBRARY DESTINATION bin)` |
| `CMakeLists.txt` | 212 | drop from `SLIDEIO_PDB_TARGETS` |
| `src/slideio/core/CMakeLists.txt` | 54-56 | delete the whole `PUBLIC` link block |
| `src/slideio/imagetools/CMakeLists.txt` | 78 | drop the entry |
| `src/slideio/drivers/ndpi/CMakeLists.txt` | 61 | drop the entry |
| `src/tests/converter/CMakeLists.txt` | 33 | drop the entry |
| `src/tests/ndpi/CMakeLists.txt` | 29 | drop the entry |
| `src/tests/transformer/CMakeLists.txt` | 31 | drop the entry |
| `src/tests/main/CMakeLists.txt` | 67 | **replace** with `${CORE_LIB_NAME}` — see below |
| `src/tools/converter/CMakeLists.txt` | 32 | drop the entry |
| `src/tools/tiffinspector/CMakeLists.txt` | 26 | drop the entry |
| `src/single_tests/converter/CMakeLists.txt` | 26 | drop the entry |
| `src/single_tests/jp2k/CMakeLists.txt` | 45 | drop the entry |
| `src/single_tests/memory_leaks/CMakeLists.txt` | 25 | drop the entry |
| `src/single_tests/ndpi_memory/CMakeLists.txt` | 25 | drop the entry |
| `src/single_tests/performance/CMakeLists.txt` | 25 | drop the entry |

Thirteen of these lists already name `${CORE_LIB_NAME}`, so deleting the base
entry is sufficient. **`src/tests/main/CMakeLists.txt:67` is the exception**: it
reads `target_link_libraries(${TEST_NAME} ${SLIDEIO_LIB_NAME} ${BASE_LIB_NAME}
${TEST_LIB_NAME})` and reaches core only transitively, through
`slideio-test-lib`'s keyword-less — therefore `PUBLIC` — link at
`src/tests/testlib/CMakeLists.txt:40`. Since `tests/main` uses `exceptions.hpp`,
`log.hpp`, and `logcontract.hpp` directly, and those symbols now live in
`slideio-core`, that entry becomes `${CORE_LIB_NAME}` rather than being dropped.

**Two load-bearing link-graph facts.** `src/slideio/imagetools/CMakeLists.txt:77-80`
and `src/slideio/drivers/ndpi/CMakeLists.txt:60-63` both use the keyword-less
`target_link_libraries` form, which defaults to `PUBLIC`. That is what carries
the logging symbols out to the drivers — the logging-replacement spec made this
dependency deliberate. Dropping the base entry from those two lists leaves the
`PUBLIC` core link intact, which is exactly what must not change.

**Header installation.** The 8-file block at `CMakeLists.txt:234-243`
(`install(FILES ... DESTINATION include/slideio/base)`) is deleted entirely. Of its eight headers,
two are gone (`base.hpp`, `slideio_base_def.hpp`) and six are appended to the
existing core block at `CMakeLists.txt:253-257`:

```cmake
install(FILES
    ${INCLUDE_ROOT}/slideio/core/slideio_core_def.hpp
    ${INCLUDE_ROOT}/slideio/core/levelinfo.hpp
    ${INCLUDE_ROOT}/slideio/core/metadata.hpp
    ${INCLUDE_ROOT}/slideio/core/rect.hpp
    ${INCLUDE_ROOT}/slideio/core/size.hpp
    ${INCLUDE_ROOT}/slideio/core/range.hpp
    ${INCLUDE_ROOT}/slideio/core/resolution.hpp
    ${INCLUDE_ROOT}/slideio/core/slideio_enums.hpp
    ${INCLUDE_ROOT}/slideio/core/slideio_structs.hpp
    DESTINATION include/slideio/core)
```

The public header set is otherwise unchanged: six headers in, two out, none
newly exposed. `exceptions.hpp`, `log.hpp`, and `logcontract.hpp` were not
installed before and are not installed now.

### 5.3 Include rewrite

183 lines across the tree reference `slideio/base/`. They fall into three
populations plus a remainder that vanishes, and the four numbers account for the
183 exactly:

| Population | Lines | Treatment |
|---|---|---|
| A — plain path rewrites | 169 | scripted `slideio/base/ → slideio/core/` |
| B — `slideio_base_def.hpp` includes | 3 | → `slideio/core/slideio_core_def.hpp` |
| C — `base/base.hpp` includes | 9 | expanded per consumer |
| Inside `base.hpp` itself | 2 | deleted with the file |

**Population A — 169 plain path rewrites.** `slideio/base/ → slideio/core/`,
scripted, no other change. Five of the 169 are internal to the moved files:
`log.hpp:5`, `exceptions.cpp:4`, `exceptions.cpp:5`, `log.cpp:8`, and
`slideio_enums.cpp:7`.

One oddity is carried across mechanically rather than fixed:
`src/tests/converter/test_tiffconverter.cpp:15` includes `slideio/base/rect.inl`
directly *without* including `rect.hpp`, relying on `Rect`'s declaration
arriving transitively from another header in that file. It becomes
`slideio/core/rect.inl` and nothing else. Straightening that include out is a
separate concern; folding it in would make commit 1 non-mechanical, which is the
one property that commit is buying.

**Population B — 3 `slideio_base_def.hpp` includes.** All three are inside the
moved headers: `exceptions.hpp:5`, `logcontract.hpp:5`, `slideio_enums.hpp:6`.
None of the three already includes `slideio_core_def.hpp`, so each simply
becomes `#include "slideio/core/slideio_core_def.hpp"` with no duplicate-include
cleanup needed. The 8 `SLIDEIO_BASE_EXPORTS` declarations — 1 in
`exceptions.hpp`, 3 in `logcontract.hpp`, 4 in `slideio_enums.hpp` — become
`SLIDEIO_CORE_EXPORTS`. `slideio_base_def.hpp` is then deleted.

This is the change that makes the merge link on Windows. Compiled into
`slideio-core`, those declarations must resolve to `__declspec(dllexport)` under
the `SLIDEIO_CORE` definition; left as `SLIDEIO_BASE_EXPORTS` they would resolve
to `dllimport` (nothing defines `SLIDEIO_BASE_API` any more) while being defined
in the same library.

**Population C — 9 `base/base.hpp` includes.** Expanded per consumer to only
what that file actually uses, not blanket-replaced with both headers:

```
src/slideio/core/tools/cvtools.cpp:7
src/slideio/drivers/dcm/dcmscene.cpp:8
src/slideio/drivers/dcm/dcmslide.cpp:6
src/slideio/drivers/ndpi/ndpislide.cpp:7
src/slideio/drivers/ndpi/ndpitifftools.hpp:10
src/slideio/drivers/pke/pkeslide.cpp:9
src/slideio/imagetools/tifftools.hpp:11
src/tests/main/test_exception.cpp:5
src/tests/main/test_logging.cpp:17
```

Each is read individually and gets `slideio/core/exceptions.hpp`,
`slideio/core/slideio_enums.hpp`, or both. This is why population C belongs in
commit 2.

### 5.4 Downstream repositories

`slideio-python` requires no changes, and the reasoning is worth recording so
the verification step in §6.5 is understood as a confirmation rather than a
formality:

- Its `CMakeLists.txt:88-94` links `slideio`, `slideio-converter`,
  `slideio-transformer`, and `slideio-core` — never `slideio-base`.
- It stages binaries by glob (`file(GLOB SLIDEIO_BIN_FILES
  $ENV{SLIDEIO_INSTALL_DIR}/bin/*.*)`, and `setup.py:155-157` packages
  `*.dll`/`*.pyd` or `*.so`/`*.dylib`), so one fewer shared library needs no
  manifest edit.
- It greps clean for both `slideio/base` and `slideio-base`.

`docs-src/` also requires no changes: the Doxygen `INPUT` is the whole
`./src/slideio` tree rather than a per-module list, and `docs-src/` has zero
references to `base`.

## 6. Verification

In order. Steps 1-2 gate commit 1; the rest gate commit 2.

**6.1 Build.** `python install.py -a conan`, then
`python install.py -a install -c all`. Both configurations clean.

**6.2 Tests.** All eight suites green:

```
slideio_tests  slideio_converter_tests  slideio_transformer_tests
slideio_ndpi_tests  slideio_vsi_tests  slideio_pke_tests
slideio_ometiff_tests  slideio_phtiff_tests
```

`slideio_tests` carries the logging tests (`src/tests/main/test_logging.cpp`),
which are the direct check on the §2 link-time-singleton requirement: they
exercise `setLogThreshold` and `logThresholdPtr` across a module boundary and
fail if the logging state has been duplicated.

**6.3 Grep assertions.** Four searches returning zero hits across the
repository, excluding `build/`, `software-docs/`, and `.superpowers/` (which
legitimately retain historical references):

```
slideio/base      SLIDEIO_BASE      BASE_LIB_NAME      slideio-base
```

**6.4 Artifacts.** `build/*/bin/` contains no `slideio-base.*`. A test install
tree has no `include/slideio/base/` directory, and does contain the six
relocated headers under `include/slideio/core/`.

**6.5 Downstream.** `slideio-python` builds and imports against the new install
tree. §5.4 predicts this needs no changes; this step is what makes it a fact
rather than a prediction.

**6.6 Known verification gap.** The five `src/single_tests/*/CMakeLists.txt`
edits are **inspection-verified only**. `src/CMakeLists.txt:4` keeps
`add_subdirectory(single_tests)` commented out, so no compiler checks them. This
is stated rather than glossed: if `single_tests` is ever re-enabled, those five
files are the first place to look.

## 7. Documentation updates

### 7.1 `CLAUDE.md`

Three edits:

1. The module-hierarchy list loses the **base** bullet, and **core** is
   redescribed as the bottom layer — it now owns exceptions, enums, the logging
   seam, and the geometry types (`Rect`, `Size`, `Range`, `Resolution`)
   alongside `CVScene`, `CVSlide`, `ImageDriver`, and `LevelInfo`.
2. The source-layout tree drops the `base/` entry.
3. The "Dependencies (managed via Conan)" list gains `spdlog`. This list is
   already stale — it still reads `glog` after the logging replacement — so the
   correction is `glog` out, `spdlog` in, with a note that spdlog is static and
   linked `PRIVATE` into `slideio-core` alone.

### 7.2 `TECH_DEBT.md`

One new numbered entry, plus its table-of-contents line: `slideio-core` is the
only module whose Windows export-control compile definition is `SLIDEIO_CORE`
rather than `SLIDEIO_<MODULE>_API`. The other fifteen modules all follow the
`_API` convention.

Renaming it is deliberately excluded from this merge. It would touch
`src/slideio/core/CMakeLists.txt:50` and
`src/slideio/core/slideio_core_def.hpp` for zero functional change, inside a
change set whose reviewability depends on the mechanical commit staying
mechanical. But this merge is the moment the inconsistency becomes visible —
eight declarations move onto that macro — so the observation is recorded rather
than lost.

### 7.3 `BREAKING_CHANGES.md`

One entry under the existing `## v2.10.0` heading, covering three distinct
failure modes. They are listed separately because they fail at different stages
and need different fixes.

1. **Include paths moved.** `slideio/base/*.hpp → slideio/core/*.hpp` for the
   six installed headers that survive: `rect.hpp`, `size.hpp`, `range.hpp`,
   `resolution.hpp`, `slideio_enums.hpp`, `slideio_structs.hpp`. Fails at
   preprocessing with a file-not-found; the fix is a path swap.
2. **`base.hpp` and `slideio_base_def.hpp` no longer exist.** Also
   preprocess-time, but the fix is not a path swap: `base.hpp` consumers need
   the two real headers, and `SLIDEIO_BASE_EXPORTS` users need
   `SLIDEIO_CORE_EXPORTS`.
3. **`slideio-base` is no longer built, installed, or shipped.** A consumer whose
   build script links or copies `slideio-base` / `libslideio-base.so` /
   `libslideio-base.dylib` / `slideio-base.dll` (or `slideio-base_d` in debug)
   fails at link or load time with no source-level hint. The migration is to
   drop it: `slideio-core` now provides those symbols, and every consumer that
   linked base also linked core.

Following the file's habit of recording platform-dependent reachability: unlike
the `NDPITIFFMessageHandler` entry, this one is broadly reachable everywhere. On
Windows the moved declarations carried `SLIDEIO_BASE_EXPORTS` and so were in
`slideio-base`'s import library; on Linux and macOS default visibility left them
linkable. There is no platform on which an out-of-tree caller was unable to
reach them.

## 8. Out of scope

- Renaming `SLIDEIO_CORE` to `SLIDEIO_CORE_API` (§7.2 — recorded as tech debt).
- Fixing `test_tiffconverter.cpp:15`'s bare `rect.inl` include (§5.3).
- Any reorganisation of `src/slideio/core/tools/`, which is untouched.
- Installing `exceptions.hpp`, `log.hpp`, or `logcontract.hpp`, which were not
  public before and are not made public here.
- Re-enabling `src/single_tests` (§6.6).
