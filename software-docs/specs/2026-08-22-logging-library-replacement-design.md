# Logging Library Replacement — Design

**Date:** 2026-08-22
**Branch:** v2.10.0
**Status:** Proposed — awaiting approval

---

## 1. Motivation

`google/glog` was **archived on 2025-06-30** and is read-only. The project is
pinned at `glog/0.7.1`, its final release. The upstream README directs users to
`ng-log` (community fork, API-compatible) or Abseil Logging (Google-maintained).

An archived dependency is not an emergency, but it is a SOUP item that will
never receive another security or correctness fix. See §9 for the regulatory
consequences, which are the reason to do this deliberately rather than
opportunistically.

## 2. Current state

### 2.1 Call-site inventory

| Item | Value |
|---|---|
| Abstraction | `src/slideio/base/log.hpp` — the entire seam is `#define SLIDEIO_LOG LOG` |
| Call sites | **234** across **40** `.cpp` files |
| Severity distribution | 159 `INFO`, 59 `WARNING`, 15 `ERROR` |
| Style | Uniformly `SLIDEIO_LOG(SEVERITY) << ...` ostream chaining |
| glog features **not** used | `CHECK*`, `VLOG`, `DLOG`, `LOG_IF`, `LOG_EVERY_N`, `RAW_LOG`, container streaming from `stl_logging.h` |
| Dangling-else exposure | None — every call site is a complete statement |
| Public API exposure | **None.** `log.hpp` is included only from `.cpp` files, never from an installed header |

### 2.2 glog runtime API — two files only

- `src/slideio/slideio/imagedrivermanager.cpp:25-33` — `initLogging()`:
  `google::InitGoogleLogging`, `FLAGS_logtostderr = true`,
  `FLAGS_minloglevel = google::GLOG_FATAL`
- `src/slideio/slideio/imagedrivermanager.cpp:152-163` — `setLogLevel()`: maps
  `"INFO"` / `"WARNING"` / `"ERROR"` / `"FATAL"` to `FLAGS_minloglevel`;
  **silently ignores** any other string
- `src/single_tests/ndpi_memory/ndpi_memory.cpp:18-20` — direct `FLAGS_*` use

### 2.3 Build-system footprint

| Item | Count |
|---|---|
| `CMakeLists.txt` with `find_package(glog)` | **27** (17 library, 10 test) |
| `conanfile.txt` with `glog/0.7.1` | **25** (10 library, 10 test, 5 single_tests) |
| Packaging: glog binary copy | `src/slideio/slideio/CMakeLists.txt:69-91` |
| Packaging: macOS rpath fixup | `CMakeLists.txt:78` (`@rpath/libglog.dylib`) |
| Checked-in generated Conan output | `src/single_tests/*/cmake/glog*.cmake` (5 dirs) |

## 3. The requirement that drives the design

**`setLogLevel` called in one module must change the log level in all modules.**

This is an existing, load-bearing guarantee, reachable through the public
`slideio::setLogLevel()` (`slideio.hpp:106`) and the Python
`slideio.set_log_level` (`slideio-python/src/pyglobals.cpp:68`).

### 3.1 Why it works today — by accident

The Conan profile builds glog **shared** (`glog_LIBRARY_TYPE_* SHARED`), and
glog exports its threshold as *data*: `GLOG_EXPORT extern int32
FLAGS_minloglevel`. One variable, one copy inside `glog.dll`, visible to every
module. Nothing in slideio's own code states or enforces this dependency.

### 3.2 Why a naive spdlog swap would break it

`spdlog/details/registry-inl.h:242`:

```cpp
SPDLOG_INLINE registry &registry::instance() {
    static registry s_instance;   // function-local static in an inline function
    return s_instance;
}
```

`spdlog::set_level()` mutates `registry::instance().global_log_level_`. Linked
header-only **or statically** into N shared libraries, this yields N independent
`s_instance` objects. MSVC does not merge function-local statics across DLL
boundaries, so `setLogLevel` in `slideio.dll` would silently fail to affect the
11 drivers.

This is **not specific to header-only builds** — a static spdlog has the same
defect. Only a *shared* spdlog reproduces glog's behaviour, which reintroduces a
shipped binary and forfeits the packaging benefit.

The failure would also be **asymmetric**: on ELF/Mach-O the inline guard symbol
is weak and the dynamic linker often collapses the copies, so it would appear to
work on Linux/macOS and fail only on Windows — until an unrelated visibility or
link change broke it elsewhere too. `src/slideio/imagetools/CMakeLists.txt`
already applies `-Wl,-Bsymbolic` on GNU builds, which is exactly the class of
flag that turns "accidentally working" into a latent, platform-specific defect.

**Conclusion: the cross-module guarantee must not be delegated to the logging
library. slideio must own the state.**

## 4. Design — a slideio-owned seam in `slideio-base`

### 4.1 Placement: `slideio-base`, not `slideio-core`

`slideio-core` was considered, since all 15 dependent modules name it on their
link lines while base does not appear there. It is not viable:

- `src/slideio/base/exceptions.cpp:9-11` logs. `RAISE_RUNTIME_ERROR` throws a
  `RuntimeError`; the `throw` copies it, and the copy constructor
  (`exceptions.hpp:23-31`) calls `log()`, guarded by `m_shown` so it fires once
  per exception. So **every raised slideio exception emits an ERROR line with
  file and line, with no log statement at the raise site**: 483
  `RAISE_RUNTIME_ERROR` sites in `src/slideio` versus 15 explicit
  `SLIDEIO_LOG(ERROR)` calls. Roughly 97% of the library's ERROR output
  originates in that one function in base — it is the primary failure audit
  trail.
- `slideio-core` links `slideio-base` (`core/CMakeLists.txt:56`). Hosting the log
  state in core would make base depend on core: a cycle.
- Three ways out of the cycle exist, and all three are worse than simply using
  base:
  - **Move `RuntimeError` to core.** Workable — base makes no use of
    `RuntimeError` outside `exceptions.*` (verified) — but it is exported as
    `SLIDEIO_BASE_EXPORTS` from `slideio/base/exceptions.hpp`, so the move
    changes the include path, the export macro, and the owning DLL. That is a
    breaking change for out-of-tree callers plus an include-line edit in every
    raising file: a larger and riskier diff than this entire migration, and it
    demotes exceptions out of the fundamental-types layer.
  - **Function-pointer indirection.** Base holds a sink pointer that core
    installs at init. No cycle, nothing moves — but exceptions can be raised very
    early, so any raised before core initialises would log nowhere. A new
    static-initialisation-order failure mode for no benefit.
  - **Delete the logging from `RuntimeError`.** Cheapest to implement, worst
    outcome: lose ~97% of ERROR output, or hand-write log lines at 483 sites that
    drift out of sync immediately.

Base needs none of these: the cycle simply does not arise.

Base is also where `log.hpp` already lives; splitting header from implementation
across layers would be gratuitous.

**Base is already linked into every module.** `imagetools/CMakeLists.txt:79-82`
uses the keyword-less `target_link_libraries` form, which defaults to `PUBLIC`,
so `${BASE_LIB_NAME}` propagates to all consumers; all 11 drivers link
imagetools (ndpi links base directly). The 483 existing `RAISE_RUNTIME_ERROR`
sites prove this resolves today — each one calls `RuntimeError::log`, a symbol
that lives in base. **No module needs a new link dependency.**

### 4.2 Hardening the propagation — one line

The propagation above is *accidental*: it depends on old-style keyword-less
linking defaulting to `PUBLIC`, through the imagetools chain. It becomes more
load-bearing after this change, because the drivers will no longer link any
logging library at all — their logging will depend entirely on base's exported
symbols arriving transitively. A future reordering of the imagetools dependency
would break logging with unhelpful link errors.

Make it deliberate, in `src/slideio/core/CMakeLists.txt:56`:

```cmake
target_link_libraries(${LIBRARY_NAME} PUBLIC ${BASE_LIB_NAME})   # was PRIVATE
```

Every module links core, so base now reaches every module by a documented path
rather than an incidental one.

### 4.3 The exported contract

`base/log.cpp` becomes **the only translation unit in the project that includes a
third-party logging library.**

Three symbols cross the DLL boundary, and only POD and `const char*` types do —
no `std::` object crosses, so there is no CRT/ABI coupling and no C4251:

```cpp
namespace slideio {
    // Enumerators deliberately avoid the bare tokens INFO/WARNING/ERROR/FATAL.
    // See §4.5.
    enum class LogLevel { Info = 0, Warning = 1, Error = 2, Fatal = 3 };

    SLIDEIO_BASE_EXPORTS void logMessage(int level, const char* file, int line,
                                         const char* message) noexcept;
    SLIDEIO_BASE_EXPORTS void setLogThreshold(int level) noexcept;
    SLIDEIO_BASE_EXPORTS const int* logThresholdPtr() noexcept;
}
```

`logMessage` is `noexcept` and that is a hard requirement, not decoration —
see §4.5.2.

`FATAL` is a **threshold-only** value: it exists so `setLogLevel("FATAL")` can
silence output, which is the current default (§4.4.1). There are zero `FATAL`
call sites among the 234, and unlike glog's `LOG(FATAL)` this design **does not
abort the process** on any severity. Logging must never terminate a caller in a
library, and no in-tree code relies on that glog behaviour.

The threshold itself is a single `int`. Reads at call sites are unsynchronised
against a concurrent `setLogThreshold`, so a level change may take effect on
other threads a few instructions late. That is benign — it costs at most one
misfiltered line at the moment of the change — and it is the same guarantee glog
gives with its non-atomic `FLAGS_minloglevel`. It is stated here rather than left
implicit because "unsynchronised" and "unspecified" are different claims, and
only the first is true.

`logThresholdPtr()` returns the address of the *single* threshold variable inside
`slideio-base`. Each module caches that pointer once, so the per-call-site check
is a load rather than a cross-DLL call:

```cpp
namespace slideio { namespace detail {

// Pasteable severity constants. The severity is embedded in the identifier so
// SLIDEIO_LOG can token-paste it and never expand a bare INFO/WARNING/ERROR
// token. See §4.5.1.
inline constexpr int severityINFO    = 0;
inline constexpr int severityWARNING = 1;
inline constexpr int severityERROR   = 2;
inline constexpr int severityFATAL   = 3;

inline bool logEnabled(int level) {
    static const int* threshold = logThresholdPtr();  // one cross-DLL call per module
    return level >= *threshold;
}

// Header-only, per-module. Never crosses a module boundary.
class LogStream {
public:
    LogStream(int level, const char* file, int line)
        : m_level(level), m_file(file), m_line(line) {}
    ~LogStream() { logMessage(m_level, m_file, m_line, m_stream.str().c_str()); }
    template <typename T>
    LogStream& operator<<(const T& value) { m_stream << value; return *this; }
private:
    std::ostringstream m_stream;
    int m_level;
    const char* m_file;
    int m_line;
};

}}  // namespace slideio::detail

#define SLIDEIO_LOG(LEVEL)                                                     \
    switch (0) case 0: default:                                                \
    if (!slideio::detail::logEnabled(slideio::detail::severity##LEVEL)) ; else  \
        slideio::detail::LogStream(slideio::detail::severity##LEVEL,            \
                                   __FILE__, __LINE__)
```

`severity##LEVEL` pastes to a single identifier such as `severityERROR`. The
operand of `##` is not macro-expanded, and the resulting identifier contains no
bare `ERROR` token to expand on rescan — this is the same technique glog uses in
`LOG(severity) COMPACT_GOOGLE_LOG_##severity` (`logging.h:476`). §4.5.1 explains
why this matters.

Per-module duplication of the cached pointer is intentional and correct: every
copy points at the same variable. The `switch (0) case 0: default:` prefix makes
the macro immune to dangling-else, even though no current call site needs it.

**All 234 call sites and all 40 `.cpp` files are unchanged.**

### 4.4 Behaviour contracts to preserve exactly

Each of these is easy to break silently, so each is asserted by a test in §7:

1. **Default threshold is `FATAL`** — effectively silent until `setLogLevel` is
   called (`imagedrivermanager.cpp:31`). Defaulting to `INFO` would flood stderr
   for every existing user. This is the *initialised* default today; §4.7 covers
   the second, divergent default that this design deliberately removes.
2. **Output goes to stderr**, matching `FLAGS_logtostderr = true`.
3. **`setLogLevel` silently ignores unrecognised strings.** Throwing instead
   would be a breaking behaviour change for Python callers.
4. **Lazy, idempotent initialisation.** Base self-initialises on first use via a
   function-local static in its single TU — one copy, therefore safe.
   `initLogging()` in `imagedrivermanager.cpp` is deleted; `setLogLevel` no
   longer needs to call it.
5. **Thread safety.** `tiffconverter.cpp:611,687,752` log at ERROR from reader,
   encoder and writer threads. The sink must be thread-safe (spdlog: an `_mt`
   logger).

### 4.5 Two hazards on the exception-logging path

Exception logging (§4.1) is the highest-traffic path through this seam — ~97% of
ERROR output — and it has two properties that ordinary log call sites do not.
Both were defects in earlier drafts of this design; both are recorded here so the
implementation does not reintroduce them.

#### 4.5.1 The `ERROR` macro collision

`<wingdi.h>` defines `ERROR` as `0`. A macro of the form
`slideio::LogLevel::LEVEL` substitutes *and macro-expands* its argument, so
`SLIDEIO_LOG(ERROR)` would become `slideio::LogLevel::0`, and an
`enum class LogLevel { ..., ERROR = 2 }` declaration would itself become
`0 = 2`. Both are hard compile errors.

This is exactly the hazard `GLOG_NO_ABBREVIATED_SEVERITIES` exists to avoid, and
`log.hpp` sets that define today. glog's own `LOG` macro is immune because it
token-pastes (`COMPACT_GOOGLE_LOG_##severity`, `logging.h:476`) and the operand
of `##` is not expanded.

Current exposure: no slideio source includes `windows.h` directly and `NOGDI` is
defined nowhere — but **`WIN32_LEAN_AND_MEAN` does not exclude `<wingdi.h>`**,
and Shlwapi, DCMTK and GDAL headers can pull `windows.h` in transitively. The
defensive `NOMINMAX` already present in `log.hpp` is evidence that it does
arrive. The design must not depend on it never arriving.

Mitigations, both required:
- `SLIDEIO_LOG` token-pastes `severity##LEVEL` (§4.3).
- `LogLevel` enumerators are `Info` / `Warning` / `Error` / `Fatal`, never the
  bare uppercase tokens.

#### 4.5.2 The sink must never throw

`RuntimeError::log` is called from the copy constructor, which executes **during
`throw`**. An exception escaping the sink there is an exception thrown while
constructing an exception, which is `std::terminate`.

glog does not throw. **spdlog throws `spdlog_ex` on sink failure by default.** A
naive swap therefore converts a benign log-write failure into a process abort at
the exact moment the library is reporting an error — a safety-relevant
regression, not merely an untidy one.

Requirements:
- `logMessage` is declared and implemented `noexcept`, swallowing all sink
  errors. A dropped log line is always preferable to terminating the caller.
- If spdlog is chosen: install `spdlog::set_error_handler` with a non-throwing
  handler, and wrap the emit in `try`/`catch(...)` regardless. The Conan
  `no_exceptions=True` option may be used as belt-and-braces but must not be
  relied on alone.
- The §7.6 test asserts a throwing sink cannot escape `logMessage`.

### 4.6 Message format — an explicit decision

The current format is undocumented in slideio but is what users' log scrapers see
today. Captured from `slideio_tests --gtest_filter=Exception.riseError` against
the v2.10.0 Release build on 2026-08-22:

```
E20260822 19:30:29.473737 18476 exceptions.cpp:10] D:\Projects\slideio\slideio\src\tests\main\test_exception.cpp:18:Error 1 sssss
```

Decomposed:

| Field | Source |
|---|---|
| `E` | severity initial |
| `20260822 19:30:29.473737` | date and time, microsecond precision |
| `18476` | thread id |
| `exceptions.cpp:10]` | glog's own location field — `__FILE__`/`__LINE__` at the **macro expansion site**, basename only. For exceptions this is always `exceptions.cpp:10` regardless of origin |
| `D:\...\test_exception.cpp:18:` | the **raise site**, absolute path, emitted by `RAISE_RUNTIME_ERROR` into the message body |
| `Error 1 sssss` | the caller's message |

**Decision: reproduce this format field-for-field** and document it in `log.hpp`
as the stable format.

Note the division of labour, because it determines what the migration can and
cannot affect: the raise-site location is assembled by `RAISE_RUNTIME_ERROR` into
`m_innerStream` *before* any logging occurs. `RuntimeError::log` receives it as a
finished string and `logMessage` passes it through verbatim. **The new design
does not participate in producing the exact code line, so it cannot regress it.**
Only the glog-owned prefix fields must be reproduced.

The redundancy between the two location fields is pre-existing and deliberately
retained: the prefix is near-useless for exceptions, but removing it would change
every line downstream consumers parse, for cosmetic gain.

### 4.7 Defect fixed: two divergent default thresholds

This is an existing defect, found while validating §4.4, and fixing it is **in
scope** for this change rather than a side effect of it.

#### The defect

Whether a raised exception is logged currently depends on whether a driver was
touched first in the same process. Verified 2026-08-22 against the v2.10.0
Release build, using the existing `Exception.riseError` test:

| Path | Current behaviour |
|---|---|
| `initLogging()` never ran | glog unconfigured, so **glog's own** defaults apply: threshold `INFO`, output to stderr, preceded by a `WARNING: Logging before InitGoogleLogging()` banner. Exception ERROR lines **do** appear |
| `initLogging()` ran | `FLAGS_minloglevel = GLOG_FATAL` → **silent** |

Evidence — the same test, twice:

```
$ slideio_tests --gtest_filter=Exception.riseError
WARNING: Logging before InitGoogleLogging() is written to STDERR
E20260822 19:31:43.120792 18220 exceptions.cpp:10] ...test_exception.cpp:18:Error 1 sssss

$ slideio_tests --gtest_filter=ImageDriverManager.*:Exception.riseError
(no exceptions.cpp line — the ImageDriverManager test initialised logging first)
```

#### Root cause

`initLogging()` lives in `imagedrivermanager.cpp` and is reachable from only two
places: `ImageDriverManager::initialize()` (line 74) and `setLogLevel()` (line
153). Any code path that logs without passing through either — a
`RAISE_RUNTIME_ERROR` from `ColorTools`, `TiffTools`, `BlockTiler`, or anything
else below the driver manager — leaves glog unconfigured and silently inherits
glog's defaults instead of slideio's. The configuration lives *above* most of the
code it is meant to govern.

#### The fix

Initialisation moves into `slideio-base`, below everything, where it cannot be
bypassed:

1. **The threshold is constant-initialised to `FATAL`** at static-init time — not
   assigned inside a lazy init function. This matters: a log emitted during
   another TU's static initialisation must not read an indeterminate threshold.
   Constant initialisation removes that hazard by construction rather than by
   ordering luck.
2. **The sink is lazily constructed** in a function-local static inside base's
   single TU (one copy, thread-safe under C++11 magic statics), created on first
   `logMessage` call. `logMessage` is therefore safe to call at any point in a
   process's life, including during static init.
3. **`initLogging()` is deleted.** Neither `ImageDriverManager::initialize()` nor
   `setLogLevel()` initialises logging any more — there is no initialisation step
   a caller can skip, because every log call goes through `logMessage`.

#### Consequences

- The default becomes uniformly `FATAL`. The order dependence disappears.
- The `WARNING: Logging before InitGoogleLogging()` banner disappears.
- Callers that today reach only lower-level utilities lose ERROR lines they
  currently see. Consumers of the public API are unaffected: `openSlide` and
  `getDriverIDs` both route through `initialize()` and were already silent.
- Recorded as an intentional behaviour change in §8; asserted by §7.3, which
  tests both former paths separately rather than assuming one implies the other.

This fix is worth having independently of the library swap — it is the reason
logging appears to work differently between test binaries today — but it cannot
be done cleanly *without* the §4.1 seam, because the initialisation has to live
in base for the bypass to become impossible.

## 5. Library choice — deliberately behind the seam

With §4 in place, the third-party library is an implementation detail of one
`.cpp` file. Evaluation, verified 2026-08-22:

| | spdlog | ng-log | Abseil Logging | In-house |
|---|---|---|---|---|
| Status | Active, v1.17.0 (2026-01) | Active, v0.8.4 (2026-08) | Active, 20260817.0 | — |
| Community | 29.5k ★ | 115 ★ | 18.1k ★ (all of Abseil) | — |
| License | MIT | BSD-3-Clause | Apache-2.0 | — |
| In Conan Center | **Yes** (1.17.0) | **No** | Yes | n/a |
| New transitive SOUP | `fmt` | none | large | none |
| Windows shared-lib risk | none (confined to base) | none | `ABSL_CONSUME_DLL` issues | none |
| Shipped binaries | none added; `glog.dll` removed | none added | Abseil libs added | none; `glog.dll` removed |

**Default recommendation: spdlog `1.17.0`**, options `header_only=False,
shared=False, fPIC=True`, linked into `slideio-base` only. It is in the package
manager already in use, MIT-licensed, and carries by far the strongest longevity
argument for a SOUP justification. Static linkage means `glog.dll` leaves the
distribution with nothing replacing it.

**Rejected: Abseil Logging.** slideio ships 15 shared libraries; Abseil's DLL
support is a documented sore spot, it offers no ABI stability under live-at-head,
and it is a very large dependency to serve three severity levels.

**Rejected as default, but close: ng-log.** Smallest source diff of any option —
an include and namespace rename, no shim, no `fmt`. Rejected only because there
is no Conan Center recipe: authoring and maintaining one is a permanent cost to
avoid a one-time ~30-line shim, and a 115-star fork is a weaker
SOUP-longevity argument.

**Open, and stronger than it first appears: in-house.** With the §4 seam, the
surface base actually needs is to format a timestamp, thread id and `file:line`
and write a line to stderr thread-safely — roughly 80–120 lines. That removes a
SOUP item outright and adds zero transitive dependencies. It trades
SOUP-qualification burden for IEC 62304 §5.5 unit-verification burden on
first-party code, and we would own any future need (file sinks, rotation,
redaction). Whether the trade is favourable depends on the per-SOUP-item cost at
Jacobian, which this design does not attempt to judge. **Recommend confirming
with GRC before implementation begins.** The seam keeps the decision reversible
at low cost either way.

## 6. Implementation scope

1. `src/slideio/base/log.hpp` — replace glog includes with the §4.3 contract.
2. `src/slideio/base/log.cpp` — **new.** The only TU including the logging
   library. Contains:
   - the threshold variable, **constant-initialised to `FATAL`** (§4.7);
   - the lazily-constructed thread-safe sink (§4.4.5, §4.7);
   - `logMessage`, `noexcept`, swallowing all sink errors (§4.5.2);
   - the §4.6 message format.
3. `src/slideio/base/CMakeLists.txt` — add `log.cpp`; replace
   `find_package(glog)` / `glog::glog` with the chosen library, or nothing.
4. `src/slideio/base/conanfile.txt` — replace `glog/0.7.1`.
5. `src/slideio/core/CMakeLists.txt:56` — `PRIVATE` → `PUBLIC` (§4.2).
6. `src/slideio/slideio/imagedrivermanager.cpp` — **delete `initLogging()`
   entirely** and both of its call sites (`initialize()` at line 74,
   `setLogLevel()` at line 153); rewrite `setLogLevel` as a plain string-to-level
   mapping onto `slideio::setLogThreshold`. This is the §4.7 fix: with
   initialisation owned by base, there is no init step left for a caller to
   bypass.
7. **26 remaining `CMakeLists.txt`** — remove `find_package(glog)` and
   `glog::glog`.
8. **24 remaining `conanfile.txt`** — remove `glog/0.7.1`.
9. `src/slideio/slideio/CMakeLists.txt:69-91` — remove glog binary copying.
10. `CMakeLists.txt:78` — remove `@rpath/libglog.dylib`.
11. `src/single_tests/ndpi_memory/ndpi_memory.cpp:18-20` — use
    `slideio::setLogLevel("INFO")`.
12. `src/single_tests/*/cmake/glog*.cmake` — regenerate or delete the checked-in
    Conan output (5 directories).
13. `software-docs/BREAKING_CHANGES.md` — new `v2.10.0` entry (§8).
14. `CLAUDE.md:115` — update the dependency list.

**Not in scope:** named per-module loggers, file or rotating sinks, structured
output, source-location capture beyond `file:line`, PHI redaction, and any change
to the 234 call sites. Each is a separate decision; §10 records why redaction in
particular deserves its own discussion.

## 7. Testing

The cross-module guarantee in §3 is **currently untested**, which is why the
header-only trap would not have been caught by construction. New tests, in
`src/tests/main`:

1. **Cross-module threshold propagation — the critical test.** Call
   `slideio::setLogLevel("INFO")`, resident in `slideio`, then trigger a log
   emitted from a *driver* module; capture stderr and assert the driver's message
   appears. Set `"FATAL"` and assert it does not. The emitting module must differ
   from the module calling `setLogLevel`, or the test does not exercise the
   boundary. This test fails against a naive header-only spdlog swap.
2. **Base-layer path.** Assert that `RAISE_RUNTIME_ERROR` raised inside a driver
   produces an ERROR line, covering `RuntimeError::log` in base.
3. **Default is silent, on both former paths (§4.7).** In a fresh process with
   no `setLogLevel` call, assert nothing is emitted — once via an operation that
   goes through `ImageDriverManager::initialize()`, and once via a
   utility-only call that does not (e.g. a `RAISE_RUNTIME_ERROR` from
   `ColorTools`). The second case is the one whose behaviour changes, so it needs
   its own assertion rather than being assumed to follow from the first.
4. **Unrecognised level is ignored.** `setLogLevel("VERBOSE")` must not throw and
   must leave the threshold unchanged.
5. **Thread safety.** Concurrent logging from several threads produces no
   interleaved or torn lines, mirroring the converter's three-thread pattern.
6. **The sink cannot throw (§4.5.2).** With a deliberately failing sink
   installed, a `RAISE_RUNTIME_ERROR` must still propagate as a `RuntimeError`
   and must not terminate the process. This is the test that would have caught
   the spdlog-throws-by-default regression.
7. **`ERROR` macro collision (§4.5.1).** A compile-only test TU that defines
   `ERROR` to `0` — emulating `<wingdi.h>` — before including `log.hpp`, then
   uses `SLIDEIO_LOG(ERROR)`. It must compile. Cheap, and it pins the one
   property of the macro that is easy to refactor away by accident.
8. **Exception-logging equivalence — golden line.** Reuse the existing
   `Exception.riseError` test (`src/tests/main/test_exception.cpp`) as the
   fixture. With the threshold at `ERROR`, a `RAISE_RUNTIME_ERROR` must emit
   **exactly one** line (not one per copy during propagation, guarding the
   `m_shown` contract), and that line must match the §4.6 format: severity
   initial, timestamp, thread id, `exceptions.cpp:<line>]`, then the raise site's
   absolute `file:line:` followed by the caller's message. Assert the raise-site
   path and line are present — that substring is the field users actually read,
   and it is the one the migration must not disturb.

Regression: `slideio_tests`, `slideio_converter_tests`,
`slideio_transformer_tests` and the five driver suites, on all three platforms.
Windows matters most — it is where the §3.2 failure mode is deterministic rather
than latent.

## 8. Breaking changes

To record in `software-docs/BREAKING_CHANGES.md` under `v2.10.0`:

- `glog.dll` / `libglog.*` is **no longer distributed**. Out-of-tree callers that
  relied on it arriving alongside slideio must vendor it themselves.
- `slideio-*` targets no longer expose `glog::glog` in their link interface.
- `slideio/base/log.hpp` no longer includes `glog/logging.h`. It is not an
  installed public header, but in-tree and vendored consumers that included it
  transitively for glog symbols will need to change.
- `SLIDEIO_LOG` remains source-compatible for the `SLIDEIO_LOG(SEVERITY) << ...`
  form. glog-only constructs (`CHECK*`, `VLOG`, `LOG_IF`, container streaming)
  are not provided; none are used in-tree.
- `slideio::setLogLevel` and `slideio.set_log_level` keep their signature and
  behaviour, including silent handling of unrecognised strings.
- **Behaviour change (minor, intentional — the §4.7 defect fix):** the default
  threshold is now uniformly `FATAL`. Previously, code paths that never reached
  `ImageDriverManager::initialize()` left glog uninitialised and therefore
  logging at `INFO` to stderr, so callers using only lower-level utilities
  (exceptions, `ColorTools`, `TiffTools`, `BlockTiler`) saw ERROR lines that are
  now suppressed until `setLogLevel` is called. Consumers of the public API are
  unaffected — `openSlide` and `getDriverIDs` both initialise, and were already
  silent. The `WARNING: Logging before InitGoogleLogging()` banner no longer
  appears.

## 9. Regulatory considerations

Flagged for GRC / Regulatory Affairs review, not resolved here:

- **SOUP change.** glog is a SOUP item under IEC 62304. Replacing it requires
  updating the SOUP list, performing the anomaly and version evaluation for the
  replacement, and re-validating. This plausibly constitutes a design change
  requiring QMS documentation under ISO 13485 design controls. The exact
  classification at Jacobian should be confirmed before implementation, not
  after.
- **The in-house option changes the burden rather than removing it** — fewer SOUP
  items, more §5.5 unit verification on first-party code. §5 leaves this open
  pending GRC input.
- **Do not treat any clause or control reference in this document as
  authoritative.** The IEC 62304 and ISO 13485 references are the author's
  understanding and should be verified with Regulatory Affairs.
- Route questions to security@jacobian.com per organisation policy.

## 10. Out of scope, but noted: PHI in logs

Existing messages include slide file paths and scene names — `converter.cpp:33`
logs a scene name and a source file path, for instance. In a clinical deployment
those can carry patient identifiers, and the INFO level is verbose (159 sites).

This is **pre-existing behaviour that the change neither introduces nor
worsens**, and it is deliberately excluded from scope so the migration stays
mechanical and reviewable. It is recorded because a logging migration is the
natural moment to decide whether the sink should be redaction-aware, and because
the §4.3 seam makes adding a redaction hook a single-file change later. Worth
raising with GRC alongside the SOUP review.

## 11. Open questions

1. **Library choice** — spdlog (default) versus in-house, pending the
   per-SOUP-item cost from GRC (§5).
2. **Branch timing** — `v2.10.0` is a release branch, and this is a cross-cutting
   build change touching 27 `CMakeLists.txt` and 25 `conanfile.txt`. "Archived"
   is not "urgent"; deferring to a `master` cycle may carry less release risk.
3. **`fmt` as a second SOUP item** — spdlog on C++17 requires `fmt`
   (`use_std_fmt` needs C++20). If SOUP items are individually costly, spdlog is
   two items and in-house is zero, which feeds directly into (1).
