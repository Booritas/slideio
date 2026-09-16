# Merging `slideio-base` into `slideio-core` — Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Dissolve `slideio-base` into `slideio-core` so that `slideio-core` becomes the bottom layer of the module hierarchy, removing an artificial boundary and the link-list, export-macro, and Conan bookkeeping it imposed.

**Architecture:** Fifteen source files move from `src/slideio/base/` to `src/slideio/core/`; the `slideio-base` CMake target, shared library, Conan tree, export macro, and installed include prefix all cease to exist. Three commits: one scriptable/mechanical, one that requires reading nine files, one documentation-only. `src/slideio/core/base.hpp` exists as a deliberate transient between commits 1 and 2 — it is what keeps the non-scriptable work in a commit of its own.

**Tech Stack:** C++17, CMake 3.10+, Conan v2, Google Test, spdlog (static), MSVC 17 2022 / GCC / Apple Clang.

**Spec:** `software-docs/specs/2026-08-23-merge-base-into-core-design.md`

## Global Constraints

- **Link-time singleton (hard requirement).** spdlog is `shared=False, fPIC=True` and must be linked `PRIVATE` into **exactly one** shared library, which after this merge is `slideio-core`. If the logging state is duplicated across libraries, `setLogThreshold` and `logThresholdPtr` stop referring to the same variable. See spec §2. `src/tests/main/test_logging.cpp` is the regression check.
- **spdlog version:** `spdlog/1.17.0` (copied verbatim from the deleted `src/slideio/base/conanfile.txt`).
- **`PUBLIC` core links must stay `PUBLIC`.** `src/slideio/imagetools/CMakeLists.txt:77-80` and `src/slideio/drivers/ndpi/CMakeLists.txt:60-63` use the keyword-less `target_link_libraries` form, which defaults to `PUBLIC`. That is what carries logging symbols out to the drivers. Remove only the `${BASE_LIB_NAME}` line from each; do not add keywords and do not reorder.
- **No compatibility shims.** No forwarding headers, no alias macros, no `BASE_LIB_NAME` alias. Spec §3.
- **This is a pure refactor.** No behavioural change is intended anywhere. Any test that changes result is a defect in the merge, not an expected outcome.
- **Every commit must configure, build, install, and test green** on both Debug and Release. `python install.py -a install -c all` covers configure + build + `cmake --install`.
- **Platform note:** the reference build for this plan is Windows/MSVC, because MSVC is the only toolchain that enforces the export-macro constraint (C2491). Linux and macOS are more permissive here and will not catch a mistake in it.

## Out of Scope — Do Not Fix These

This is a boundary removal, not a cleanup pass. Each of the following is a real
imperfection you will encounter, and each is deliberately left alone (spec §8).
Fixing any of them makes the mechanical commit non-mechanical, which is the one
property it exists to have.

- **`src/tests/converter/test_tiffconverter.cpp:15`** includes `rect.inl`
  directly, without including `rect.hpp`, relying on `Rect`'s declaration
  arriving transitively. Rewrite the path to `slideio/core/rect.inl` and change
  nothing else.
- **`SLIDEIO_CORE` vs `SLIDEIO_<MODULE>_API`.** Do not rename the compile
  definition. Task 3 Step 3 records it as tech debt instead.
- **`src/slideio/core/tools/`** is untouched. Do not flatten it, do not move the
  arriving files into it.
- **`exceptions.hpp`, `log.hpp`, `logcontract.hpp`** were not installed before
  and must not be added to any install list.
- **`src/single_tests`** stays commented out in `src/CMakeLists.txt:4`. Do not
  re-enable it to test your edits there.
- **`docs-src/`** needs no changes: the Doxygen `INPUT` is the whole
  `./src/slideio` tree rather than a per-module list, and `docs-src/` contains no
  reference to `base`. Do not add one.
- **No unrelated include tidying.** Task 2 Step 4 adds exactly seven includes to
  seven named files. Do not add "while I'm here" includes elsewhere.

## Testing Approach

There is no new behaviour to test, so there is no new test to write first. The regression harness is the eight existing suites plus a set of grep assertions that act as executable invariants for "the boundary is gone". The TDD analogue is Task 1 Step 1: establish and record a green baseline **before** touching anything, so any later failure is unambiguously attributable to this work.

Do not add new test files. Do not modify existing test assertions. `src/tests/main/test_logging.cpp` and `src/tests/main/test_exception.cpp` are edited in Task 2 for their `#include` lines only.

## File Structure

**Moved** (`git mv`, `src/slideio/base/X` → `src/slideio/core/X`), 16 files in Task 1:

| File | Note |
|---|---|
| `exceptions.hpp`, `exceptions.cpp` | `RuntimeError`, `RAISE_RUNTIME_ERROR` |
| `slideio_enums.hpp`, `slideio_enums.cpp` | `Compression`, `DataType`, `MetadataFormat` |
| `log.hpp`, `log.cpp`, `logcontract.hpp` | the logging seam; `log.cpp` is the only TU that includes spdlog |
| `slideio_structs.hpp`, `resolution.hpp` | |
| `rect.hpp`, `rect.inl`, `size.hpp`, `size.inl`, `range.hpp`, `range.inl` | `.inl` guarded by `SLIDEIO_INTERNAL_HEADER`, not installed |
| `base.hpp` | **transient** — moved in Task 1, deleted in Task 2 |

**Deleted in Task 1:** `src/slideio/base/slideio_base_def.hpp`, `src/slideio/base/CMakeLists.txt`, `src/slideio/base/conanfile.txt`, `src/slideio/base/CMakeUserPresets.json`, and `src/slideio/base/cmake/` (26 generated files). The `src/slideio/base/` directory ends Task 1 empty and gone.

**Deleted in Task 2:** `src/slideio/core/base.hpp`.

**Modified — build system** (Task 1): `CMakeLists.txt` (6 sites), `src/slideio/CMakeLists.txt`, `src/slideio/core/CMakeLists.txt`, `src/slideio/core/conanfile.txt`, plus 13 link lists listed in Task 1 Step 10.

**Modified — sources** (Task 1, scripted): 183 include lines across 11 modules; 8 `SLIDEIO_BASE_EXPORTS` declarations in the three moved headers.

**Modified — sources** (Task 2, hand-read): 9 former `base.hpp` consumers + 7 files that lose `exceptions.hpp` transitively. Exact lists in Task 2.

**Modified — docs** (Task 3): `CLAUDE.md`, `software-docs/BREAKING_CHANGES.md`, `software-docs/TECH_DEBT.md`.

---

## Task 1: Mechanical merge

Everything scriptable or grep-verifiable. This task is large and cannot be subdivided — spec §4.1 documents the three constraints that force it into one commit (the move and the include rewrite are simultaneous; the install lists fail `cmake --install`; the export-macro rename fails MSVC C2491). Steps 12-14 are the gate.

**Files:**
- Move: the 16 files in the table above, `src/slideio/base/` → `src/slideio/core/`
- Delete: `src/slideio/base/slideio_base_def.hpp`, `src/slideio/base/CMakeLists.txt`, `src/slideio/base/conanfile.txt`, `src/slideio/base/CMakeUserPresets.json`, `src/slideio/base/cmake/`
- Modify: `CMakeLists.txt`, `src/slideio/CMakeLists.txt`, `src/slideio/core/CMakeLists.txt`, `src/slideio/core/conanfile.txt`, 13 link lists
- Modify (scripted): all `src/**/*.{hpp,cpp,h,c,inl}` containing `slideio/base/` or `SLIDEIO_BASE_EXPORTS`

**Interfaces:**
- Consumes: nothing (first task).
- Produces: `SLIDEIO_CORE_EXPORTS` as the sole export macro for `RuntimeError::log`, `compressionToString`, the three `operator<<` overloads for `Compression`/`DataType`/`MetadataFormat`, and `logMessage`/`setLogThreshold`/`logThresholdPtr`. All formerly-`slideio/base/` headers are reachable as `slideio/core/<name>.hpp`. `slideio/core/base.hpp` still exists and still includes `exceptions.hpp` + `slideio_enums.hpp` — Task 2 removes it.

- [ ] **Step 1: Record the green baseline**

Do this before changing anything. If the baseline is not green, stop and report — do not start the merge on a red tree.

```bash
cd /d/Projects/slideio/slideio
git status --porcelain          # must be clean
python install.py -a install -c all 2>&1 | tail -20
```

Then run all eight suites and save the summary lines. **Note the binary path** — on Windows the Visual Studio generator produces `build/bin/<Config>/`, not `build/<Config>/bin/`:

```bash
BIN=build/bin/Release            # Windows; on Linux/macOS use build/release/bin
for t in slideio_tests slideio_converter_tests slideio_transformer_tests \
         slideio_ndpi_tests slideio_vsi_tests slideio_pke_tests \
         slideio_ometiff_tests slideio_phtiff_tests; do
  echo "=== $t ==="
  "$BIN/$t" 2>&1 | tail -3
done | tee build/baseline-tests.txt
```

`build/` is gitignored (`.gitignore:22`), so the baseline file will not be committed. Expected: every suite reports `[  PASSED  ]` with no `[  FAILED  ]` lines. Record the per-suite passed counts — Step 13 compares against them.

- [ ] **Step 2: Move the 16 files**

```bash
cd /d/Projects/slideio/slideio
for f in exceptions.hpp exceptions.cpp slideio_enums.hpp slideio_enums.cpp \
         log.hpp log.cpp logcontract.hpp slideio_structs.hpp resolution.hpp \
         rect.hpp rect.inl size.hpp size.inl range.hpp range.inl base.hpp; do
  git mv "src/slideio/base/$f" "src/slideio/core/$f"
done
git status --short
```

Expected: 16 `R` (rename) entries. No `??` untracked entries.

- [ ] **Step 3: Delete the base build files and the export-macro header**

```bash
git rm -q src/slideio/base/slideio_base_def.hpp
git rm -q src/slideio/base/CMakeLists.txt
git rm -q src/slideio/base/conanfile.txt
git rm -q src/slideio/base/CMakeUserPresets.json
git rm -q -r src/slideio/base/cmake
ls src/slideio/base 2>&1
```

Expected: the `ls` fails — `src/slideio/base` no longer exists. If it still exists, something in it was untracked; list the contents and report rather than force-deleting.

- [ ] **Step 4: Rewrite the include paths**

Three seds in this order. The first does the bulk; the second fixes the one filename that also changes; the third renames the macro.

```bash
cd /d/Projects/slideio/slideio
FILES=$(grep -rl "slideio/base/\|SLIDEIO_BASE_EXPORTS" \
        --include=*.hpp --include=*.cpp --include=*.h --include=*.c --include=*.inl src)
echo "$FILES" | wc -l          # expect 145
sed -i 's|slideio/base/|slideio/core/|g' $FILES
sed -i 's|slideio/core/slideio_base_def\.hpp|slideio/core/slideio_core_def.hpp|g' $FILES
sed -i 's/SLIDEIO_BASE_EXPORTS/SLIDEIO_CORE_EXPORTS/g' $FILES
```

- [ ] **Step 5: Verify the rewrite before building**

```bash
grep -rn "slideio/base/" --include=*.hpp --include=*.cpp --include=*.h --include=*.c --include=*.inl src | wc -l   # expect 0
grep -rn "SLIDEIO_BASE"  --include=*.hpp --include=*.cpp --include=*.h --include=*.c --include=*.inl src | wc -l   # expect 0
grep -rn "slideio/core/slideio_base_def" src | wc -l                                                              # expect 0
grep -rc "SLIDEIO_CORE_EXPORTS" src/slideio/core/exceptions.hpp src/slideio/core/logcontract.hpp src/slideio/core/slideio_enums.hpp
```

Expected: the first three are `0`. The last prints `exceptions.hpp:1`, `logcontract.hpp:3`, `slideio_enums.hpp:4` — 8 declarations total, matching spec §5.3 population B. A different count means the sed hit or missed something; investigate before continuing.

Also confirm each of those three headers now includes the core def header:

```bash
grep -n "slideio_core_def.hpp" src/slideio/core/exceptions.hpp src/slideio/core/logcontract.hpp src/slideio/core/slideio_enums.hpp
```

Expected: exactly one hit per file, and no file has two `_def.hpp` includes.

- [ ] **Step 6: Wire the moved sources into the core target**

In `src/slideio/core/CMakeLists.txt`, add the 16 moved files to `SOURCE_FILES`. Insert immediately after the `metadata_xml.cpp` line, before the closing `)`:

```cmake
   ${CMAKE_CURRENT_SOURCE_DIR}/exceptions.hpp
   ${CMAKE_CURRENT_SOURCE_DIR}/exceptions.cpp
   ${CMAKE_CURRENT_SOURCE_DIR}/slideio_enums.hpp
   ${CMAKE_CURRENT_SOURCE_DIR}/slideio_enums.cpp
   ${CMAKE_CURRENT_SOURCE_DIR}/log.hpp
   ${CMAKE_CURRENT_SOURCE_DIR}/log.cpp
   ${CMAKE_CURRENT_SOURCE_DIR}/logcontract.hpp
   ${CMAKE_CURRENT_SOURCE_DIR}/slideio_structs.hpp
   ${CMAKE_CURRENT_SOURCE_DIR}/resolution.hpp
   ${CMAKE_CURRENT_SOURCE_DIR}/rect.hpp
   ${CMAKE_CURRENT_SOURCE_DIR}/rect.inl
   ${CMAKE_CURRENT_SOURCE_DIR}/size.hpp
   ${CMAKE_CURRENT_SOURCE_DIR}/size.inl
   ${CMAKE_CURRENT_SOURCE_DIR}/range.hpp
   ${CMAKE_CURRENT_SOURCE_DIR}/range.inl
   ${CMAKE_CURRENT_SOURCE_DIR}/base.hpp
```

`base.hpp` is included here deliberately; Task 2 removes this line along with the file.

- [ ] **Step 7: Move the spdlog link into core and drop the base link**

In `src/slideio/core/CMakeLists.txt`, add alongside the other `find_package` calls:

```cmake
find_package(spdlog REQUIRED)
```

and alongside the other `target_link_libraries` calls:

```cmake
target_link_libraries(${LIBRARY_NAME} PRIVATE spdlog::spdlog)
```

`PRIVATE` is required, not stylistic — see Global Constraints.

Then delete the trailing block at lines 54-56:

```cmake
target_link_libraries(${LIBRARY_NAME} PUBLIC
   ${BASE_LIB_NAME}
)
```

Leave the `#set_target_properties(... CXX_VISIBILITY_PRESET hidden)` comment on the last line alone.

- [ ] **Step 8: Add spdlog to core's Conan requirements and regenerate**

Edit `src/slideio/core/conanfile.txt` so `[requires]` reads:

```
[requires]
sqlite3/3.44.2
opencv/4.10.0@slideio/stable
zlib/1.3.1
tinyxml2/9.0.0
icu/76.1@slideio/stable
nlohmann_json/3.11.3
spdlog/1.17.0
[options]
```

Then regenerate:

```bash
python install.py -a conan
ls src/slideio/core/cmake/ | grep -ci "spdlog\|fmt"
```

Expected: `14` — the 7 `spdlog*` and 7 `fmt*` config files that previously lived in `src/slideio/base/cmake/`.

**If the count is 0:** `core/conanfile.txt` has no `[generators]` section while the deleted `base/conanfile.txt` declared `CMakeDeps` and `CMakeToolchain` (spec §5.2 flags this as unconfirmed). Add to `core/conanfile.txt`:

```
[generators]
CMakeDeps
CMakeToolchain
```

then re-run `python install.py -a conan` and re-check. Note in the commit message which of the two paths was needed.

- [ ] **Step 9: Remove the base subdirectory from the build**

In `src/slideio/CMakeLists.txt`, delete line 1:

```cmake
add_subdirectory(base)
```

- [ ] **Step 10: Purge `BASE_LIB_NAME` from the 13 link lists**

Delete the single `${BASE_LIB_NAME}` line from each of these. All twelve already list `${CORE_LIB_NAME}`, so nothing replaces it:

```
src/slideio/imagetools/CMakeLists.txt:78
src/slideio/drivers/ndpi/CMakeLists.txt:61
src/tests/converter/CMakeLists.txt:33
src/tests/ndpi/CMakeLists.txt:29
src/tests/transformer/CMakeLists.txt:31
src/tools/converter/CMakeLists.txt:32
src/tools/tiffinspector/CMakeLists.txt:26
src/single_tests/converter/CMakeLists.txt:26
src/single_tests/jp2k/CMakeLists.txt:45
src/single_tests/memory_leaks/CMakeLists.txt:25
src/single_tests/ndpi_memory/CMakeLists.txt:25
src/single_tests/performance/CMakeLists.txt:25
```

**`src/tests/main/CMakeLists.txt:67` is different — replace, do not delete.** It is the one target that reaches core only transitively (via `slideio-test-lib`'s `PUBLIC` link at `src/tests/testlib/CMakeLists.txt:40`), and it uses `exceptions.hpp`, `log.hpp`, and `logcontract.hpp` directly:

```cmake
target_link_libraries(${TEST_NAME} ${SLIDEIO_LIB_NAME} ${CORE_LIB_NAME} ${TEST_LIB_NAME})
```

The five `src/single_tests/*` files are edited but **not compiled** — `src/CMakeLists.txt:4` keeps `add_subdirectory(single_tests)` commented out. Verify them by re-reading the diff; no build will catch a mistake there.

- [ ] **Step 11: Purge `BASE_LIB_NAME` and the base header install from the root**

Six edits in `CMakeLists.txt`:

1. Line 55 — delete `set(BASE_LIB_NAME "${LIB_NAME_PREFIX}base")`.
2. Line 76 — delete `    "lib${BASE_LIB_NAME}.dylib"` from `NAME_TOOL_LIB_LIST`.
3. Line 164 — delete `             ${BASE_LIB_NAME}` from the `install(TARGETS ... RUNTIME DESTINATION bin)` block.
4. Line 184 — delete `                 ${BASE_LIB_NAME}` from the `if(NOT WIN32) install(TARGETS ... LIBRARY DESTINATION bin)` block.
5. Line 212 — delete `        ${BASE_LIB_NAME}` from `SLIDEIO_PDB_TARGETS`.
6. Lines 234-243 — delete the whole `install(FILES ... DESTINATION include/slideio/base)` block, and extend the core block at 253-257 to:

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

Six headers in, two out (`base.hpp` and `slideio_base_def.hpp` are not installed). The `.inl` files stay uninstalled, as before. Do **not** add `exceptions.hpp`, `log.hpp`, or `logcontract.hpp` — they were not public before and are not being made public.

Line numbers shift as you edit; work bottom-up (edit 6 first, then 5, ...) or re-grep between edits.

- [ ] **Step 12: Configure, build, and install both configurations**

```bash
cd /d/Projects/slideio/slideio
python install.py -a install -c all 2>&1 | tail -30
```

Expected: clean configure, build, and `cmake --install` for Debug and Release.

Two failure modes to recognise:
- `install TARGETS given target "slideio-base" which does not exist` — Step 11 edit 3, 4, or 5 was missed.
- `file INSTALL cannot find .../slideio/base/base.hpp` — Step 11 edit 6 was missed.
- MSVC `error C2491: definition of dllimport function not allowed` — the Step 4 macro sed did not apply; re-check Step 5's counts.

- [ ] **Step 13: Run all eight test suites**

```bash
BIN=build/bin/Release            # Windows; on Linux/macOS use build/release/bin
for t in slideio_tests slideio_converter_tests slideio_transformer_tests \
         slideio_ndpi_tests slideio_vsi_tests slideio_pke_tests \
         slideio_ometiff_tests slideio_phtiff_tests; do
  echo "=== $t ==="
  "$BIN/$t" 2>&1 | tail -3
done | tee build/task1-tests.txt

diff build/baseline-tests.txt build/task1-tests.txt
```

Expected: the `diff` is empty, or differs only in timing lines. This is a pure refactor; a changed pass/fail count is a defect.

`slideio_tests` includes `test_logging.cpp`, which is the check on the Global Constraints link-time singleton. If logging tests fail here, spdlog has most likely been linked into more than one shared library — re-check Step 7's `PRIVATE`.

- [ ] **Step 14: Grep and artifact assertions**

```bash
cd /d/Projects/slideio/slideio
for pat in "slideio/base" "SLIDEIO_BASE" "BASE_LIB_NAME" "slideio-base"; do
  n=$(grep -rn "$pat" . \
      --exclude-dir=build --exclude-dir=.git --exclude-dir=software-docs \
      --exclude-dir=.superpowers | wc -l)
  echo "$pat => $n"
done
```

Expected: all four `0`. `software-docs/` and `.superpowers/` are excluded because they legitimately retain historical references; if your shell lacks `--exclude-dir`, filter with `grep -v`.

Artifacts:

```bash
ls build/bin/Release/ | grep -i "slideio-base" ; echo "exit=$?"    # expect no match, exit=1
ls build/install/release/include/slideio/base 2>&1                 # expect: no such file or directory
ls build/install/release/include/slideio/core/                      # expect the 9 installed core headers
```

The install prefix defaults to `<build-dir>/install/<config>` (`install.py:367-369`), so `build/install/release` and `build/install/debug`; adjust if you passed `-pr`. The install tree does not exist until `-a install` has run, which Step 12 does.

`src/slideio/core/base.hpp` still exists at this point and is expected — none of the four greps catches it, because `slideio/core/base.hpp` does not match `slideio/base`. Task 2 removes it.

- [ ] **Step 15: Commit**

```bash
git add -A
git commit -F - <<'EOF'
merge slideio-base into slideio-core (mechanical)

Move the 15 slideio-base sources into slideio-core, delete the slideio-base
target, library, Conan tree, and export macro, and purge BASE_LIB_NAME from
the build. slideio-core is now the bottom layer of the module hierarchy.

The include rewrite, def-header redirect, and macro rename were produced by:

  FILES=$(grep -rl "slideio/base/\|SLIDEIO_BASE_EXPORTS" \
          --include=*.hpp --include=*.cpp --include=*.h --include=*.c \
          --include=*.inl src)
  sed -i 's|slideio/base/|slideio/core/|g' $FILES
  sed -i 's|slideio/core/slideio_base_def\.hpp|slideio/core/slideio_core_def.hpp|g' $FILES
  sed -i 's/SLIDEIO_BASE_EXPORTS/SLIDEIO_CORE_EXPORTS/g' $FILES

spdlog moves with log.cpp and stays PRIVATE to exactly one shared library,
which is now slideio-core; the logging state must not be duplicated.

The install and packaging lists and the export-macro rename are in this commit
rather than the next because neither can be deferred: install.py -a install
runs cmake --install, and MSVC rejects a dllimport declaration defined in the
same DLL (C2491). See spec section 4.1.

src/slideio/core/base.hpp is a deliberate transient, removed in the next
commit; keeping it here is what lets the nine per-consumer include expansions
land in a reviewable commit of their own.

The five src/single_tests/* edits are inspection-verified only —
src/CMakeLists.txt:4 keeps that directory out of the build.

Spec: software-docs/specs/2026-08-23-merge-base-into-core-design.md
EOF
```

---

## Task 2: Delete the `base.hpp` umbrella

The only work in this merge a script cannot do. Two of the nine consumers are *headers*, so removing the umbrella from them strips `exceptions.hpp` from files that were getting it transitively — 7 such files are identified below and need explicit includes.

**Files:**
- Delete: `src/slideio/core/base.hpp`
- Modify: `src/slideio/core/CMakeLists.txt` (drop the `base.hpp` line added in Task 1 Step 6)
- Modify: 9 former `base.hpp` consumers (exact edits below)
- Modify: 7 files that lose `exceptions.hpp` transitively (exact list below)

**Interfaces:**
- Consumes: `slideio/core/exceptions.hpp` (provides `slideio::RuntimeError` and the `RAISE_RUNTIME_ERROR` macro) and `slideio/core/slideio_enums.hpp` (provides `Compression`, `DataType`, `MetadataFormat`, `compressionToString`, and the three `operator<<` overloads) — both established by Task 1.
- Produces: no `base.hpp` anywhere; every consumer of those two headers includes them directly.

- [ ] **Step 1: Delete the umbrella and its build entry**

```bash
cd /d/Projects/slideio/slideio
git rm -q src/slideio/core/base.hpp
```

Then remove this line from `src/slideio/core/CMakeLists.txt`:

```cmake
   ${CMAKE_CURRENT_SOURCE_DIR}/base.hpp
```

- [ ] **Step 2: Rewrite the two header consumers — delete, add nothing**

Both already include `slideio/core/slideio_enums.hpp` on the preceding line, and neither uses `RuntimeError` or `RAISE_RUNTIME_ERROR`. The umbrella is pure redundancy in both.

In `src/slideio/drivers/ndpi/ndpitifftools.hpp`, delete line 10:

```cpp
#include "slideio/core/base.hpp"
```

leaving lines 8-9 (`resolution.hpp`, `slideio_enums.hpp`) untouched.

In `src/slideio/imagetools/tifftools.hpp`, delete line 11:

```cpp
#include "slideio/core/base.hpp"
```

leaving lines 9-10 (`resolution.hpp`, `slideio_enums.hpp`) untouched.

- [ ] **Step 3: Rewrite the seven `.cpp` consumers**

Replace the single `#include "slideio/core/base.hpp"` line in each with exactly the includes listed. These were derived by checking each file for `RuntimeError`/`RAISE_RUNTIME_ERROR` usage and for `Compression`/`DataType`/`MetadataFormat`/`compressionToString` usage.

`src/slideio/core/tools/cvtools.cpp:7` — uses both:
```cpp
#include "slideio/core/exceptions.hpp"
#include "slideio/core/slideio_enums.hpp"
```

`src/slideio/drivers/dcm/dcmscene.cpp:8` — uses both:
```cpp
#include "slideio/core/exceptions.hpp"
#include "slideio/core/slideio_enums.hpp"
```

`src/slideio/drivers/pke/pkeslide.cpp:9` — uses both:
```cpp
#include "slideio/core/exceptions.hpp"
#include "slideio/core/slideio_enums.hpp"
```

`src/slideio/drivers/dcm/dcmslide.cpp:6` — exceptions only:
```cpp
#include "slideio/core/exceptions.hpp"
```

`src/slideio/drivers/ndpi/ndpislide.cpp:7` — exceptions only:
```cpp
#include "slideio/core/exceptions.hpp"
```

`src/tests/main/test_exception.cpp:5` — exceptions only:
```cpp
#include "slideio/core/exceptions.hpp"
```

`src/tests/main/test_logging.cpp:17` — exceptions only:
```cpp
#include "slideio/core/exceptions.hpp"
```

Change nothing else in these files. In particular do not touch any test assertion in `test_exception.cpp` or `test_logging.cpp`.

- [ ] **Step 4: Add explicit `exceptions.hpp` to the seven files that lose it**

These use `RAISE_RUNTIME_ERROR` or `RuntimeError`, do not include `exceptions.hpp` themselves, and were reaching it only through `tifftools.hpp` → `base.hpp`. Static include-graph analysis confirms each loses reachability after Step 2 and has no alternative path:

```
src/slideio/converter/converter.cpp
src/slideio/drivers/pke/pkesmallscene.cpp
src/slideio/drivers/pke/pketiledscene.cpp
src/slideio/drivers/scn/scnscene.cpp
src/slideio/drivers/svs/phtiffslide.cpp
src/slideio/drivers/svs/svsslide.cpp
src/slideio/drivers/svs/svstiledscene.cpp
```

Add to each, grouped with its other `slideio/core/` includes:

```cpp
#include "slideio/core/exceptions.hpp"
```

Five other `tifftools.hpp` consumers — `src/slideio/drivers/ome-tiff/otscene.cpp`, `src/slideio/drivers/vsi/vsifile.cpp`, `src/slideio/imagetools/smalltiffwrapper.cpp`, `src/slideio/imagetools/tifftools.cpp`, `src/tests/main/test_imagetools.cpp` — retain `exceptions.hpp` through another include path and must **not** be edited. Adding a redundant include there is churn, not safety.

The analysis ignores `#ifdef` guards, so treat the list as a strong prediction that Step 5 confirms. If the build reports an undefined `RAISE_RUNTIME_ERROR` in a file not listed here, add the same include line to it and note the addition in the commit message. If one of the seven turns out not to need it, remove it again rather than leaving it in.

- [ ] **Step 5: Build both configurations**

```bash
python install.py -a install -c all 2>&1 | tail -30
```

Expected: clean. The characteristic failure is `'RAISE_RUNTIME_ERROR': undeclared identifier` (MSVC) or `there are no arguments to 'RAISE_RUNTIME_ERROR'` (GCC/Clang) in a file missed by Step 4.

- [ ] **Step 6: Run all eight test suites**

```bash
BIN=build/bin/Release            # Windows; on Linux/macOS use build/release/bin
for t in slideio_tests slideio_converter_tests slideio_transformer_tests \
         slideio_ndpi_tests slideio_vsi_tests slideio_pke_tests \
         slideio_ometiff_tests slideio_phtiff_tests; do
  echo "=== $t ==="
  "$BIN/$t" 2>&1 | tail -3
done | tee build/task2-tests.txt

diff build/baseline-tests.txt build/task2-tests.txt
```

Expected: the `diff` is empty, or differs only in timing lines.

- [ ] **Step 7: Assert the boundary is fully gone**

```bash
cd /d/Projects/slideio/slideio
ls src/slideio/core/base.hpp 2>&1                       # expect: no such file
grep -rn "base\.hpp" src | grep -v "database\|_base" ; echo "exit=$?"   # expect no match
for pat in "slideio/base" "SLIDEIO_BASE" "BASE_LIB_NAME" "slideio-base"; do
  n=$(grep -rn "$pat" . --exclude-dir=build --exclude-dir=.git \
      --exclude-dir=software-docs --exclude-dir=.superpowers | wc -l)
  echo "$pat => $n"
done
```

Expected: the `ls` fails, no `base.hpp` reference survives, all four patterns `0`.

- [ ] **Step 8: Confirm the downstream repository still builds**

The public header set is final at this point, so this is where `slideio-python` gets checked. Spec §5.4 predicts no changes are needed — it links `slideio-core`, stages binaries by glob, and greps clean for both `slideio/base` and `slideio-base`. Confirm rather than assume:

```bash
cd /d/Projects/slideio/slideio-python
grep -rn "slideio/base\|slideio-base" . --exclude-dir=build --exclude-dir=.git \
     --exclude-dir=build-local --exclude-dir=dist ; echo "exit=$?"   # expect no match
```

Then build the bindings against the new install tree and import the module. Use whichever of the repo's documented flows is available (`SLIDEIO_INSTALL_DIR` pointing at `build/install/release`), and verify:

```bash
python -c "import slideio; print(slideio.__version__); print(slideio.get_driver_ids())"
```

Expected: the version prints and the driver id list is non-empty and unchanged — one fewer shared library in `bin/` must not change the set of drivers that load. If the import fails with a missing-DLL error naming `slideio-base`, something outside this repo still references it; report it rather than reinstating the library.

- [ ] **Step 9: Commit**

```bash
cd /d/Projects/slideio/slideio
git add -A
git commit -F - <<'EOF'
delete the base.hpp umbrella and expand its consumers

base.hpp aggregated exactly two headers, so it hid which of them any given
file actually needed. Its nine consumers now include what they use:

- cvtools.cpp, dcmscene.cpp, pkeslide.cpp     -> exceptions + slideio_enums
- dcmslide.cpp, ndpislide.cpp,
  test_exception.cpp, test_logging.cpp        -> exceptions
- ndpitifftools.hpp, tifftools.hpp            -> nothing; both already
  included slideio_enums.hpp directly and neither uses RuntimeError

Removing the umbrella from those last two headers stopped supplying
exceptions.hpp transitively to seven files that relied on it, so each now
includes it explicitly: converter.cpp, pkesmallscene.cpp, pketiledscene.cpp,
scnscene.cpp, phtiffslide.cpp, svsslide.cpp, svstiledscene.cpp. Five other
tifftools.hpp consumers keep it through another path and were left alone.

Spec: software-docs/specs/2026-08-23-merge-base-into-core-design.md
EOF
```

---

## Task 3: Documentation

No build impact; no test run needed.

**Files:**
- Modify: `CLAUDE.md`, `software-docs/BREAKING_CHANGES.md`, `software-docs/TECH_DEBT.md`

**Interfaces:**
- Consumes: the completed merge from Tasks 1 and 2.
- Produces: nothing consumed by later tasks.

- [ ] **Step 1: Update `CLAUDE.md`**

Delete the **base** bullet at line 63 and rewrite the **core** bullet at line 64 so the merged responsibility is visible:

```markdown
- **core** — Bottom layer. Fundamental types (Rect, Size, Range, Resolution, enums, exceptions, the logging seam) and the abstract base classes: `CVScene` (raster image), `CVSlide` (slide container), `ImageDriver`, `LevelInfo` (zoom pyramid)
```

In the Source Layout tree, delete the line:

```
│   ├── base/           # slideio-base
```

Replace the dependency list at line 115 — drop the stale `glog`, add `spdlog`:

```markdown
spdlog, SQLite3, OpenCV, ZLIB, tinyxml2, ICU, libtiff, libjpeg, WebP, OpenJPEG, Iconv, pole, nlohmann_json

spdlog is a static library linked `PRIVATE` into `slideio-core` alone. That is a
link-time-singleton constraint, not an ordinary dependency: the logging
threshold and sink must exist in exactly one shared library.
```

- [ ] **Step 2: Add the `BREAKING_CHANGES.md` entry**

Insert as a new `###` subsection under `## v2.10.0` (line 10), before the existing `### NDPITIFFMessageHandler is no longer copyable` entry at line 12:

```markdown
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

Unlike the `NDPITIFFMessageHandler` entry below, this one is reachable on every
platform. On Windows the affected declarations carried `SLIDEIO_BASE_EXPORTS`
and so appeared in `slideio-base`'s import library; on Linux and macOS default
visibility left them linkable. There is no platform on which an out-of-tree
caller could not reach them.
```

- [ ] **Step 3: Add the `TECH_DEBT.md` entry**

The last entry is number 12, so this is 13. Add the table-of-contents line after the entry-12 line:

```markdown
13. [`slideio-core`'s export-control define breaks the project naming convention](#13-slideio-cores-export-control-define-breaks-the-project-naming-convention)
```

and append the entry at the end of the file:

```markdown
---

## 13. `slideio-core`'s export-control define breaks the project naming convention

**File:** `src/slideio/core/CMakeLists.txt:50`, `src/slideio/core/slideio_core_def.hpp`

Every module gates its `__declspec(dllexport)` on a compile definition named
`SLIDEIO_<MODULE>_API` — `SLIDEIO_NDPI_API`, `SLIDEIO_IMAGETOOLS_API`,
`SLIDEIO_CONVERTER_API`, and twelve more. `slideio-core` alone uses
`SLIDEIO_CORE`, with no `_API` suffix.

This is cosmetic — the macro works — but it is a trap for anyone adding a module
by copying core's CMakeLists, and it defeats a grep for `SLIDEIO_.*_API` across
the build.

Noted while merging `slideio-base` into `slideio-core` (2026-08-23), which moved
eight more declarations onto `SLIDEIO_CORE_EXPORTS` and so widened the
inconsistency's reach. It was deliberately left out of that merge: renaming the
define is a two-file change with no functional effect, and it would have landed
inside a commit whose reviewability depended on staying mechanical.

**Proposed direction:** rename the compile definition to `SLIDEIO_CORE_API` in
`core/CMakeLists.txt` and update the `#if defined(...)` guard in
`slideio_core_def.hpp`. Both are private to the build — `SLIDEIO_CORE` is never
defined by consumers, only by the core target itself — so this is not a breaking
change and needs no `BREAKING_CHANGES.md` entry. Two-line diff, one full build
to verify.
```

- [ ] **Step 4: Verify the docs are internally consistent**

```bash
cd /d/Projects/slideio/slideio
grep -n "base" CLAUDE.md | grep -vi "database\|abstract base\|base class"
grep -c "slideio-base" software-docs/BREAKING_CHANGES.md    # expect >0: the entry describes it
grep -n "^13\.\|^## 13\." software-docs/TECH_DEBT.md        # expect 2 hits: ToC + entry
```

Expected: `CLAUDE.md` has no stray `slideio-base` or `slideio/base` reference; `glog` no longer appears anywhere in `CLAUDE.md`.

- [ ] **Step 5: Commit**

```bash
git add -A
git commit -F - <<'EOF'
document the slideio-base merge

- CLAUDE.md: drop the base module, describe core as the bottom layer, and
  correct the dependency list (glog was already stale; spdlog is the static
  link-time singleton in slideio-core).
- BREAKING_CHANGES.md: record the three ways the merge breaks out-of-tree
  callers — moved include paths, the two deleted headers, and the removed
  binary — and note that unlike the NDPITIFFMessageHandler entry this one is
  reachable on every platform.
- TECH_DEBT.md: add entry 13 for slideio-core's SLIDEIO_CORE define, which
  breaks the SLIDEIO_<MODULE>_API convention the other fifteen modules follow.
  Deliberately not fixed in the merge.

Spec: software-docs/specs/2026-08-23-merge-base-into-core-design.md
EOF
```

---

## Completion Criteria

The merge is done when all of the following hold:

1. `src/slideio/base/` does not exist; `src/slideio/core/base.hpp` does not exist.
2. `grep -rn` for `slideio/base`, `SLIDEIO_BASE`, `BASE_LIB_NAME`, and `slideio-base` returns nothing outside `build/`, `.git/`, `software-docs/`, and `.superpowers/`.
3. `python install.py -a install -c all` is clean on both configurations.
4. All eight test suites report the same passed counts as the Task 1 Step 1 baseline, with zero failures.
5. No `slideio-base.*` artifact in `build/*/bin/`; no `include/slideio/base/` in the install tree; the nine core headers present under `include/slideio/core/`.
6. `slideio-python` builds and imports against the new install tree with an unchanged driver-id list.
7. Three commits on the branch, in order: mechanical, umbrella deletion, documentation.
8. `CLAUDE.md`, `BREAKING_CHANGES.md`, and `TECH_DEBT.md` reflect the end state.

## Known Verification Gap

The five `src/single_tests/*/CMakeLists.txt` edits are **inspection-verified only**. `src/CMakeLists.txt:4` keeps `add_subdirectory(single_tests)` commented out, so no compiler checks them. If `single_tests` is re-enabled later, those five files are the first place to look for a stale `${BASE_LIB_NAME}`.
