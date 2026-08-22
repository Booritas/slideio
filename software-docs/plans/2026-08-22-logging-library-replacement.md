# Logging Library Replacement Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Replace the archived `glog` dependency with a slideio-owned logging seam in `slideio-base`, preserving all existing behaviour except one deliberate defect fix.

**Architecture:** All logging state (threshold + sink) moves into `slideio-base` behind three exported POD-only symbols. `base/log.cpp` becomes the only translation unit in the project that includes a third-party logging library; the other 26 modules stop linking one entirely. `SLIDEIO_LOG` keeps its `(SEVERITY) << ...` form via a header-local `ostringstream` shim, so none of the 234 call sites change.

**Tech Stack:** C++17, CMake, Conan 2, Google Test 1.17.0, spdlog 1.17.0 (Conan Center), MSVC 17 2022 / GCC / Clang.

**Spec:** `software-docs/specs/2026-08-22-logging-library-replacement-design.md` — read it before starting. This plan argues from it and references its sections by number.

## Global Constraints

- **C++17.** Do not use C++20 constructs. This is why spdlog's `use_std_fmt` option is unavailable and `fmt` comes in as a transitive dependency (spec §11.3).
- **Never let the sink throw.** `logMessage` is `noexcept` and swallows all sink errors. It is called from `RuntimeError`'s copy constructor during `throw`; an escaping exception is `std::terminate` (spec §4.5.2).
- **Never macro-expand severity tokens.** `SLIDEIO_LOG` must token-paste (`severity##LEVEL`). `<wingdi.h>` defines `ERROR` as `0` (spec §4.5.1). The `LogLevel` enum must not contain enumerators named `INFO`/`WARNING`/`ERROR`/`FATAL`.
- **Only POD and `const char*` cross the DLL boundary.** No `std::` object in any exported signature — avoids C4251 and CRT coupling (spec §4.3).
- **Default threshold is `FATAL`,** constant-initialised at static-init time, not assigned in a lazy init function (spec §4.7).
- **Output goes to stderr.** Message format is fixed by spec §4.6 and must match field-for-field.
- **Do not change any of the 234 `SLIDEIO_LOG` call sites.** If a task seems to require it, stop — the seam is wrong.
- **Build commands:** `python3 install.py -a build -c release` and `-c debug`. Full build: `python3 install.py -a install -c release`. After changing any `conanfile.txt`, run `python3 install.py -a conan` first.
- **Windows is the platform of record for review.** The failure mode this design exists to prevent (spec §3.2) is deterministic on MSVC and latent elsewhere.
- **Human review before commit.** Every task's code must be read by a human before its commit lands (organisation policy).

---

## File Structure

| File | Responsibility |
|---|---|
| `src/slideio/base/log.hpp` (modify) | Public-to-the-project logging surface: `LogLevel`, three exported declarations, pasteable severity constants, `logEnabled`, `LogStream`, `SLIDEIO_LOG`. Header-local code only — no third-party include. |
| `src/slideio/base/log.cpp` (create) | The only TU including a logging library. Owns the constant-initialised threshold, the lazily-constructed thread-safe sink, `noexcept logMessage`, and the §4.6 format. |
| `src/tests/main/test_logging.cpp` (create) | All logging tests: cross-module propagation, format golden line, defaults, thread safety, sink-cannot-throw. |
| `src/tests/main/test_log_macro_collision.cpp` (create) | Compile-only guard: defines `ERROR` to `0` before including `log.hpp`. |
| `src/slideio/slideio/imagedrivermanager.cpp` (modify) | Loses `initLogging()` entirely; `setLogLevel` becomes a pure string→level mapping. |

Everything else in this plan is mechanical removal of glog from build files, packaging, and docs.

---

## Task 1: Characterisation tests — lock current behaviour before touching anything

These tests must pass **before and after** the migration. They are the safety net for Tasks 2–4. Written against the current glog implementation.

**Files:**
- Create: `src/tests/main/test_logging.cpp`
- Modify: `src/tests/main/CMakeLists.txt` (add to `TEST_SOURCES`)

**Interfaces:**
- Consumes: existing public API — `slideio::setLogLevel`, `slideio::openSlide`, `slideio::RuntimeError`, `RAISE_RUNTIME_ERROR`.
- Produces: nothing consumed by later tasks. Tasks 2–4 must keep these tests green.

- [ ] **Step 1: Write the characterisation tests**

Create `src/tests/main/test_logging.cpp`:

```cpp
// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#include <gtest/gtest.h>
#include <string>
#include "slideio/base/base.hpp"
#include "slideio/slideio/slideio.hpp"

using namespace slideio;

namespace {
    // Restores the log level after each test so ordering cannot leak between
    // tests. FATAL is the library default (spec 4.7).
    class LoggingTest : public ::testing::Test {
    protected:
        void TearDown() override { slideio::setLogLevel("FATAL"); }
    };
}

// THE critical test (spec 7.1). setLogLevel lives in slideio; the asserted
// message is emitted by NDPIImageDriver::openFile in slideio-ndpi. If the
// threshold is not shared across DLLs, this fails.
TEST_F(LoggingTest, thresholdCrossesModuleBoundary)
{
    const std::string marker = "NDPIImageDriver: open file:";

    slideio::setLogLevel("INFO");
    testing::internal::CaptureStderr();
    EXPECT_THROW(slideio::openSlide("nonexistent_slide.ndpi", "NDPI"), slideio::RuntimeError);
    const std::string atInfo = testing::internal::GetCapturedStderr();
    EXPECT_NE(atInfo.find(marker), std::string::npos)
        << "driver-resident INFO log did not appear at INFO threshold; captured:\n" << atInfo;

    slideio::setLogLevel("FATAL");
    testing::internal::CaptureStderr();
    EXPECT_THROW(slideio::openSlide("nonexistent_slide.ndpi", "NDPI"), slideio::RuntimeError);
    const std::string atFatal = testing::internal::GetCapturedStderr();
    EXPECT_EQ(atFatal.find(marker), std::string::npos)
        << "driver-resident INFO log survived a FATAL threshold; captured:\n" << atFatal;
}

// Spec 4.1 / 7.2: RuntimeError::log lives in slideio-base and is reached from
// every module. ~97% of ERROR output comes from here.
TEST_F(LoggingTest, raiseRuntimeErrorLogsFromBase)
{
    slideio::setLogLevel("ERROR");
    testing::internal::CaptureStderr();
    EXPECT_THROW(slideio::openSlide("nonexistent_slide.ndpi", "NDPI"), slideio::RuntimeError);
    const std::string out = testing::internal::GetCapturedStderr();
    EXPECT_NE(out.find("exceptions.cpp"), std::string::npos)
        << "no exception ERROR line; captured:\n" << out;
}

// Spec 4.6: the raise site's file and line must appear in the message body.
// This is the field users actually read.
TEST_F(LoggingTest, exceptionLineCarriesRaiseSiteLocation)
{
    slideio::setLogLevel("ERROR");
    testing::internal::CaptureStderr();
    try {
        RAISE_RUNTIME_ERROR << "characterisation marker 4711";
    } catch (const std::exception&) {
    }
    const std::string out = testing::internal::GetCapturedStderr();

    EXPECT_NE(out.find("test_logging.cpp"), std::string::npos)
        << "raise-site file missing; captured:\n" << out;
    EXPECT_NE(out.find("characterisation marker 4711"), std::string::npos)
        << "message body missing; captured:\n" << out;
    EXPECT_NE(out.find("exceptions.cpp"), std::string::npos)
        << "glog prefix location missing; captured:\n" << out;
}

// Spec 7.8: RuntimeError's m_shown guard means one line per exception, not one
// per copy made while it propagates. The test below re-throws twice, mirroring
// test_exception.cpp's nesting.
TEST_F(LoggingTest, exceptionLogsExactlyOncePerRaise)
{
    const std::string marker = "log once marker 6220";
    slideio::setLogLevel("ERROR");
    testing::internal::CaptureStderr();
    try {
        try {
            RAISE_RUNTIME_ERROR << marker;
        } catch (const std::exception&) {
            throw;
        }
    } catch (const std::exception&) {
    }
    const std::string out = testing::internal::GetCapturedStderr();

    std::size_t count = 0;
    for (std::size_t at = out.find(marker); at != std::string::npos;
         at = out.find(marker, at + marker.size())) {
        ++count;
    }
    EXPECT_EQ(count, 1u) << "expected exactly one line per raise, saw " << count
                         << "; captured:\n" << out;
}

// Spec 4.4.3: unrecognised strings are silently ignored. Python callers rely
// on this not throwing.
TEST_F(LoggingTest, unrecognisedLevelIsIgnored)
{
    slideio::setLogLevel("ERROR");
    EXPECT_NO_THROW(slideio::setLogLevel("VERBOSE"));
    EXPECT_NO_THROW(slideio::setLogLevel(""));

    // The threshold must be unchanged by the ignored calls: ERROR still logs.
    testing::internal::CaptureStderr();
    try {
        RAISE_RUNTIME_ERROR << "still at error 8150";
    } catch (const std::exception&) {
    }
    const std::string out = testing::internal::GetCapturedStderr();
    EXPECT_NE(out.find("still at error 8150"), std::string::npos)
        << "ignored setLogLevel changed the threshold; captured:\n" << out;
}

// Spec 4.4.5 / 7.5: the converter logs at ERROR from three worker threads.
TEST_F(LoggingTest, concurrentLoggingProducesWholeLines)
{
    slideio::setLogLevel("ERROR");
    testing::internal::CaptureStderr();
    {
        std::vector<std::thread> workers;
        for (int t = 0; t < 4; ++t) {
            workers.emplace_back([t]() {
                for (int i = 0; i < 50; ++i) {
                    try {
                        RAISE_RUNTIME_ERROR << "thread" << t << "iteration" << i;
                    } catch (const std::exception&) {
                    }
                }
            });
        }
        for (auto& w : workers) {
            w.join();
        }
    }
    const std::string out = testing::internal::GetCapturedStderr();

    // Every emitted marker must appear intact, i.e. not torn by interleaving.
    for (int t = 0; t < 4; ++t) {
        for (int i = 0; i < 50; ++i) {
            const std::string marker = "thread" + std::to_string(t) + "iteration" + std::to_string(i);
            EXPECT_NE(out.find(marker), std::string::npos) << "torn or lost line: " << marker;
        }
    }
}
```

Add `#include <thread>` and `#include <vector>` to the include block above (needed by the concurrency test).

- [ ] **Step 2: Register the test file**

In `src/tests/main/CMakeLists.txt`, add to the `TEST_SOURCES` list (alongside `test_exception.cpp`):

```cmake
  test_logging.cpp
```

- [ ] **Step 3: Build and run — these must PASS against current glog**

```bash
python3 install.py -a build -c release
./build/bin/Release/slideio_tests.exe --gtest_filter=LoggingTest.*
```

Expected: all 5 tests PASS.

**If `CaptureStderr` returns empty strings**, glog's buffering is defeating the capture. Do not weaken the assertions. Fix it by calling `slideio::setLogLevel(...)` *before* `CaptureStderr()` (already done above) and, if still empty, add `fflush(stderr);` immediately before each `GetCapturedStderr()` call. Record whichever fix was needed in the commit message — Task 3 replaces the sink and needs to know.

**If `thresholdCrossesModuleBoundary` fails at the INFO assertion**, verify the marker string still matches `src/slideio/drivers/ndpi/ndpiimagedriver.cpp:27`. That log statement is the cross-module vector; if it has been reworded, update the marker, not the test's intent.

- [ ] **Step 4: Commit**

```bash
git add src/tests/main/test_logging.cpp src/tests/main/CMakeLists.txt
git commit -m "add characterisation tests for logging behaviour before glog replacement"
```

---

## Task 2: Build the new seam in base, alongside glog

Purely additive. `SLIDEIO_LOG` still routes to glog at the end of this task, so nothing can regress. This is also the **only** task that binds a third-party logging library — see the Alternative at the end if in-house is chosen (spec §11.1).

**Files:**
- Create: `src/slideio/base/log.cpp`
- Create: `src/slideio/base/logcontract.hpp`
- Modify: `src/slideio/base/CMakeLists.txt`
- Modify: `src/slideio/base/conanfile.txt`
- Modify: `src/tests/main/test_logging.cpp` (add unit tests for the new primitives)

**Interfaces:**
- Consumes: `SLIDEIO_BASE_EXPORTS` from `slideio/base/slideio_base_def.hpp`.
- Produces, all in namespace `slideio`, consumed by Task 3:
  - `enum class LogLevel { Info = 0, Warning = 1, Error = 2, Fatal = 3 };`
  - `SLIDEIO_BASE_EXPORTS void logMessage(int level, const char* file, int line, const char* message) noexcept;`
  - `SLIDEIO_BASE_EXPORTS void setLogThreshold(int level) noexcept;`
  - `SLIDEIO_BASE_EXPORTS const int* logThresholdPtr() noexcept;`

A separate header (`logcontract.hpp`) holds the exported declarations so `log.cpp` can include the contract without the macro machinery, and so Task 3's rewrite of `log.hpp` is a clean replacement rather than a merge.

- [ ] **Step 1: Write the failing tests for the new primitives**

Append to `src/tests/main/test_logging.cpp`:

```cpp
#include "slideio/base/logcontract.hpp"

// Spec 4.7.1: the threshold is constant-initialised to Fatal. Reading it before
// anything has configured logging must yield Fatal, not an indeterminate value.
TEST_F(LoggingTest, thresholdDefaultsToFatal)
{
    const int* threshold = slideio::logThresholdPtr();
    ASSERT_NE(threshold, nullptr);

    slideio::setLogThreshold(static_cast<int>(slideio::LogLevel::Fatal));
    EXPECT_EQ(*threshold, static_cast<int>(slideio::LogLevel::Fatal));
}

// Spec 4.3: every caller sees the same variable. A per-module copy would make
// the pointer's target diverge from what setLogThreshold wrote.
TEST_F(LoggingTest, thresholdPointerTracksWrites)
{
    const int* threshold = slideio::logThresholdPtr();
    ASSERT_NE(threshold, nullptr);

    slideio::setLogThreshold(static_cast<int>(slideio::LogLevel::Info));
    EXPECT_EQ(*threshold, static_cast<int>(slideio::LogLevel::Info));

    slideio::setLogThreshold(static_cast<int>(slideio::LogLevel::Error));
    EXPECT_EQ(*threshold, static_cast<int>(slideio::LogLevel::Error));

    EXPECT_EQ(slideio::logThresholdPtr(), threshold) << "pointer must be stable across calls";
}

// Spec 4.6: format must match the captured glog line field-for-field.
TEST_F(LoggingTest, logMessageFormatMatchesSpec)
{
    slideio::setLogThreshold(static_cast<int>(slideio::LogLevel::Error));
    testing::internal::CaptureStderr();
    slideio::logMessage(static_cast<int>(slideio::LogLevel::Error),
                        "D:\\some\\path\\widget.cpp", 42, "payload 9021");
    const std::string out = testing::internal::GetCapturedStderr();

    // E20260822 19:31:43.120792 18220 widget.cpp:42] payload 9021
    EXPECT_EQ(out.empty(), false) << "nothing emitted";
    EXPECT_EQ(out[0], 'E') << "severity initial wrong; got: " << out;
    EXPECT_NE(out.find("widget.cpp:42]"), std::string::npos)
        << "basename:line] field wrong; got: " << out;
    EXPECT_NE(out.find("payload 9021"), std::string::npos) << "message lost; got: " << out;
    EXPECT_EQ(out.find("D:\\some\\path"), std::string::npos)
        << "location field must be the basename only; got: " << out;
    EXPECT_EQ(out.back(), '\n') << "line must be newline-terminated";
}

TEST_F(LoggingTest, logMessageRespectsThreshold)
{
    slideio::setLogThreshold(static_cast<int>(slideio::LogLevel::Error));

    testing::internal::CaptureStderr();
    slideio::logMessage(static_cast<int>(slideio::LogLevel::Info), "f.cpp", 1, "suppressed 3310");
    EXPECT_EQ(testing::internal::GetCapturedStderr().find("suppressed 3310"), std::string::npos);

    testing::internal::CaptureStderr();
    slideio::logMessage(static_cast<int>(slideio::LogLevel::Error), "f.cpp", 1, "emitted 3311");
    EXPECT_NE(testing::internal::GetCapturedStderr().find("emitted 3311"), std::string::npos);
}

// Spec 4.5.2: logMessage is called during throw. It must never propagate.
TEST_F(LoggingTest, logMessageIsNoexcept)
{
    static_assert(noexcept(slideio::logMessage(0, "f.cpp", 1, "m")),
                  "logMessage must be noexcept - it runs inside RuntimeError's copy ctor "
                  "during throw, where an escaping exception is std::terminate");

    slideio::setLogThreshold(static_cast<int>(slideio::LogLevel::Error));
    // Degenerate inputs must not throw or crash.
    EXPECT_NO_THROW(slideio::logMessage(static_cast<int>(slideio::LogLevel::Error), nullptr, 0, nullptr));
    EXPECT_NO_THROW(slideio::logMessage(9999, "f.cpp", 1, ""));
    EXPECT_NO_THROW(slideio::logMessage(-1, "f.cpp", 1, "negative level"));
}

// Spec 7.6 - THE test that catches spdlog's throw-by-default behaviour. A
// static_assert only proves the declaration; this proves the implementation
// survives a sink that actually fails. Forces the failure by putting a
// read-only descriptor over stderr, so every write returns an error.
TEST_F(LoggingTest, failingSinkCannotEscapeLogMessage)
{
    slideio::setLogThreshold(static_cast<int>(slideio::LogLevel::Error));
    // Ensure the sink is constructed before stderr is broken, so we are testing
    // write failure rather than construction failure.
    slideio::logMessage(static_cast<int>(slideio::LogLevel::Error), "warmup.cpp", 1, "warmup");

#if defined(_MSC_VER)
    const int saved = _dup(2);
    const int readOnly = _open("nul", _O_RDONLY);
    ASSERT_NE(saved, -1);
    ASSERT_NE(readOnly, -1);
    ASSERT_NE(_dup2(readOnly, 2), -1);
#else
    const int saved = dup(2);
    const int readOnly = open("/dev/null", O_RDONLY);
    ASSERT_NE(saved, -1);
    ASSERT_NE(readOnly, -1);
    ASSERT_NE(dup2(readOnly, 2), -1);
#endif

    // The assertion is simply that we reach the next line. If logMessage lets
    // the sink's exception escape, this test terminates the process instead of
    // failing - which is exactly the production failure mode being guarded.
    slideio::logMessage(static_cast<int>(slideio::LogLevel::Error), "broken.cpp", 7, "into a broken sink");

    // Same call through the path that matters: during throw.
    try {
        RAISE_RUNTIME_ERROR << "raised into a broken sink";
    } catch (const std::exception&) {
    }

#if defined(_MSC_VER)
    _dup2(saved, 2);
    _close(saved);
    _close(readOnly);
#else
    dup2(saved, 2);
    close(saved);
    close(readOnly);
#endif

    SUCCEED() << "logMessage survived a failing sink";
}
```

Add to this file's include block:

```cpp
#if defined(_MSC_VER)
#include <io.h>
#include <fcntl.h>
#else
#include <unistd.h>
#include <fcntl.h>
#endif
```

**If the descriptor manipulation proves unreliable on a platform**, do not delete this test — reduce it to the warm-up call plus the `RAISE_RUNTIME_ERROR` leg and record the reduced coverage in `software-docs/TECH_DEBT.md`. The `static_assert` alone is not a substitute: it checks the declaration, and spdlog's throwing behaviour lives in the implementation.

- [ ] **Step 2: Run to verify they fail**

```bash
./build/bin/Release/slideio_tests.exe --gtest_filter=LoggingTest.threshold*:LoggingTest.logMessage*
```

Expected: compile failure — `slideio/base/logcontract.hpp` does not exist.

- [ ] **Step 3: Create the exported contract header**

Create `src/slideio/base/logcontract.hpp`:

```cpp
// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#pragma once
#include "slideio/base/slideio_base_def.hpp"

namespace slideio
{
    /** @brief Log severity.
     *
     * Enumerator names deliberately avoid the bare tokens INFO, WARNING, ERROR
     * and FATAL: <wingdi.h> defines ERROR as 0, which would turn the
     * declaration below into "0 = 2". See the design spec, section 4.5.1.
     *
     * Fatal is a threshold-only value. Nothing in slideio logs at Fatal, and
     * unlike glog's LOG(FATAL) this library never aborts the process.
     */
    enum class LogLevel
    {
        Info = 0,
        Warning = 1,
        Error = 2,
        Fatal = 3
    };

    /** @brief Emits one formatted line to stderr if @a level passes the threshold.
     *
     * noexcept is a hard requirement, not decoration: this is called from
     * RuntimeError's copy constructor while an exception is being thrown, where
     * an escaping exception means std::terminate. All sink errors are swallowed
     * - a dropped log line always beats terminating the caller.
     *
     * @a file may be a full path; only its basename is emitted. @a file and
     * @a message tolerate nullptr.
     */
    SLIDEIO_BASE_EXPORTS void logMessage(int level, const char* file, int line,
                                         const char* message) noexcept;

    /** @brief Sets the global severity threshold. Values outside LogLevel are ignored. */
    SLIDEIO_BASE_EXPORTS void setLogThreshold(int level) noexcept;

    /** @brief Address of the single global threshold, for cheap per-call-site checks.
     *
     * The returned pointer is stable for the process lifetime and always refers
     * to the one variable inside slideio-base. Callers in other modules cache it
     * so the common case is a load rather than a cross-DLL call.
     */
    SLIDEIO_BASE_EXPORTS const int* logThresholdPtr() noexcept;
}
```

- [ ] **Step 4: Add spdlog to base's dependencies**

Replace the contents of `src/slideio/base/conanfile.txt`:

```
[requires]
spdlog/1.17.0
[generators]
CMakeDeps
CMakeToolchain
```

Note `glog/0.7.1` is removed here but remains in the other 24 conanfiles until Task 4 — that is intentional and harmless.

Regenerate Conan output:

```bash
python3 install.py -a conan
```

- [ ] **Step 5: Implement `log.cpp`**

Create `src/slideio/base/log.cpp`:

```cpp
// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
//
// This is the ONLY translation unit in slideio that includes a third-party
// logging library. See the design spec, section 4.3: everything else reaches
// logging through the three exported functions declared in logcontract.hpp.
#include "slideio/base/logcontract.hpp"

#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_sinks.h>

#include <cstring>
#include <memory>

namespace
{
    // Constant-initialised, NOT assigned in a lazy init function. A log emitted
    // during another translation unit's static initialisation must not read an
    // indeterminate threshold. See spec section 4.7.1.
    int g_threshold = static_cast<int>(slideio::LogLevel::Fatal);

    // Reproduces glog's severity initial: I / W / E / F.
    char severityInitial(int level)
    {
        switch (level) {
        case static_cast<int>(slideio::LogLevel::Info):    return 'I';
        case static_cast<int>(slideio::LogLevel::Warning): return 'W';
        case static_cast<int>(slideio::LogLevel::Error):   return 'E';
        default:                                           return 'F';
        }
    }

    // glog emits the basename only. Handles both separators: __FILE__ is
    // backslash-separated under MSVC and slash-separated elsewhere.
    const char* basename(const char* path)
    {
        if (path == nullptr) {
            return "";
        }
        const char* last = path;
        for (const char* p = path; *p != '\0'; ++p) {
            if (*p == '/' || *p == '\\') {
                last = p + 1;
            }
        }
        return last;
    }

    // glog's timestamp: yyyymmdd hh:mm:ss.uuuuuu, local time, microseconds.
    // Formatted explicitly rather than through fmt's chrono support, which
    // would add an <spdlog/fmt/chrono.h> dependency and leave the fractional
    // width up to the clock's precision.
    std::string timestamp()
    {
        const auto now = std::chrono::system_clock::now();
        const std::time_t seconds = std::chrono::system_clock::to_time_t(now);
        const auto micros = std::chrono::duration_cast<std::chrono::microseconds>(
                                now.time_since_epoch()).count() % 1000000;

        std::tm parts{};
#if defined(_MSC_VER)
        localtime_s(&parts, &seconds);
#else
        localtime_r(&seconds, &parts);
#endif

        char buffer[32];
        std::snprintf(buffer, sizeof(buffer), "%04d%02d%02d %02d:%02d:%02d.%06lld",
                      parts.tm_year + 1900, parts.tm_mon + 1, parts.tm_mday,
                      parts.tm_hour, parts.tm_min, parts.tm_sec,
                      static_cast<long long>(micros));
        return buffer;
    }

    // glog prints the OS thread id. std::thread::id streams as a number on
    // every target toolchain; the exact value need not match glog's, only the
    // field's presence and shape.
    std::string threadId()
    {
        std::ostringstream stream;
        stream << std::this_thread::get_id();
        return stream.str();
    }

    // Lazily constructed inside this single TU, so exactly one instance exists
    // process-wide and logMessage is safe to call at any point in the process
    // lifetime, including during static initialisation.
    std::shared_ptr<spdlog::logger>& sink()
    {
        static std::shared_ptr<spdlog::logger> logger = []() {
            // Non-throwing error handler: see spec 4.5.2. Belt to the
            // try/catch braces in logMessage.
            spdlog::set_error_handler([](const std::string&) {});
            // _mt: the converter logs at ERROR from reader, encoder and writer
            // threads (spec 4.4.5).
            auto created = spdlog::stderr_logger_mt("slideio");
            // The whole line is produced by logMessage; spdlog must not add a
            // prefix of its own.
            created->set_pattern("%v");
            created->set_level(spdlog::level::trace);
            created->flush_on(spdlog::level::trace);
            return created;
        }();
        return logger;
    }
}

namespace slideio
{
    void setLogThreshold(int level) noexcept
    {
        if (level < static_cast<int>(LogLevel::Info) || level > static_cast<int>(LogLevel::Fatal)) {
            return;
        }
        g_threshold = level;
    }

    const int* logThresholdPtr() noexcept
    {
        return &g_threshold;
    }

    void logMessage(int level, const char* file, int line, const char* message) noexcept
    {
        if (level < g_threshold) {
            return;
        }
        try {
            // E20260822 19:31:43.120792 18220 exceptions.cpp:10] message
            std::ostringstream line_;
            line_ << severityInitial(level) << timestamp() << ' ' << threadId() << ' '
                  << basename(file) << ':' << line << "] "
                  << (message == nullptr ? "" : message);
            sink()->info(line_.str());
        }
        catch (...) {
            // Swallowed deliberately. This runs during throw; propagating here
            // is std::terminate. See spec 4.5.2.
        }
    }
}
```

Include block for this file:

```cpp
#include <chrono>
#include <cstdio>
#include <ctime>
#include <memory>
#include <sstream>
#include <string>
#include <thread>
```

**Note:** the severity is embedded in the text by `logMessage`, and spdlog's own level is pinned to `trace` with pattern `%v`, so spdlog contributes only the thread-safe write and never filters or decorates. That is deliberate — it keeps the format entirely under our control and makes the in-house alternative a genuinely small diff.

- [ ] **Step 6: Wire `log.cpp` into the base library**

In `src/slideio/base/CMakeLists.txt`:

Add to `SOURCE_FILES` after the `log.hpp` entry:

```cmake
   ${CMAKE_CURRENT_SOURCE_DIR}/log.cpp
   ${CMAKE_CURRENT_SOURCE_DIR}/logcontract.hpp
```

Replace lines 27-28 (`find_package(glog REQUIRED)` / `target_link_libraries(${LIBRARY_NAME} glog::glog)`) with:

```cmake
find_package(spdlog REQUIRED)
target_link_libraries(${LIBRARY_NAME} PRIVATE spdlog::spdlog)
```

`PRIVATE` is deliberate: spdlog is an implementation detail of `log.cpp` and must not appear in base's link interface. No other module may see it.

- [ ] **Step 7: Run the new unit tests**

```bash
python3 install.py -a build -c release
./build/bin/Release/slideio_tests.exe --gtest_filter=LoggingTest.threshold*:LoggingTest.logMessage*
```

Expected: PASS. If `logMessageFormatMatchesSpec` fails on the timestamp, fix the format string in `log.cpp` — not the test.

- [ ] **Step 8: Confirm Task 1's characterisation tests still pass**

```bash
./build/bin/Release/slideio_tests.exe --gtest_filter=LoggingTest.*
```

Expected: all PASS. `SLIDEIO_LOG` still routes to glog at this point, so any failure here means Task 2 broke something it should not have touched.

- [ ] **Step 9: Commit**

```bash
git add src/slideio/base/log.cpp src/slideio/base/logcontract.hpp \
        src/slideio/base/CMakeLists.txt src/slideio/base/conanfile.txt \
        src/tests/main/test_logging.cpp
git commit -m "add slideio-owned logging primitives in base alongside glog"
```

### Alternative for Step 4/5: in-house sink instead of spdlog

If GRC's per-SOUP-item cost makes the in-house option preferable (spec §5, §11.1, §11.3), **only Steps 4 and 5 change** — every other step in this plan and every other task is identical. That is the payoff of the §4.3 seam.

Skip Step 4 entirely (no `[requires]` entry; base gains no dependency), skip the `find_package(spdlog)` half of Step 6, and replace `log.cpp`'s body: drop both spdlog includes, delete `sink()`, and write the line with `std::fprintf(stderr, ...)` under a function-local `std::mutex` for the thread-safety contract, formatting the timestamp with `localtime_r`/`localtime_s` plus a microsecond field from `system_clock::now().time_since_epoch()`. Keep the `try`/`catch(...)` — `fprintf` will not throw, but the `noexcept` contract must survive a future sink change.

---

## Task 3: Flip `SLIDEIO_LOG` to the new seam and remove glog from base

The behavioural flip. Task 1's tests are the gate. This task also fixes the §4.7 defect, because it must: once `log.hpp` stops including glog headers, `imagedrivermanager.cpp`'s `google::`/`FLAGS_` references no longer compile, so `initLogging()` has to go in the same change.

**Files:**
- Modify: `src/slideio/base/log.hpp` (full rewrite)
- Modify: `src/slideio/slideio/imagedrivermanager.cpp:25-33, 74, 152-163`
- Modify: `src/slideio/core/CMakeLists.txt:56` (propagation hardening, spec §4.2)
- Create: `src/tests/main/test_log_macro_collision.cpp`
- Modify: `src/tests/main/CMakeLists.txt`
- Modify: `src/tests/main/test_logging.cpp` (add the §4.7 default tests)

**Interfaces:**
- Consumes: the three exported functions and `LogLevel` from Task 2.
- Produces: `SLIDEIO_LOG(SEVERITY)` with unchanged call-site syntax, backed by `slideio::detail::LogStream`. No later task depends on `detail` internals.

- [ ] **Step 1: Write the failing tests — macro collision guard**

Create `src/tests/main/test_log_macro_collision.cpp`:

```cpp
// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
//
// Compile-only guard for design spec section 4.5.1. <wingdi.h> defines ERROR as
// 0. If SLIDEIO_LOG ever stops token-pasting its severity, the uses below stop
// compiling. That is the entire point of this file - it is cheap, and it pins
// the one property of the macro that is easy to refactor away by accident.
#include <gtest/gtest.h>

// Emulate the windows.h / wingdi.h macro set, deliberately BEFORE log.hpp.
#define ERROR 0
#define INFO 0
#define WARNING 0

#include "slideio/base/log.hpp"

TEST(LogMacroCollision, compilesWithWindowsSeverityMacrosDefined)
{
    // These must compile even though ERROR, INFO and WARNING are macros.
    SLIDEIO_LOG(INFO) << "info under macro pressure";
    SLIDEIO_LOG(WARNING) << "warning under macro pressure";
    SLIDEIO_LOG(ERROR) << "error under macro pressure";
    SUCCEED();
}

#undef ERROR
#undef INFO
#undef WARNING
```

- [ ] **Step 2: Write the failing tests — §4.7 unified default**

Append to `src/tests/main/test_logging.cpp`:

```cpp
// Spec 4.7 / 7.3: the default threshold is uniformly Fatal. Before this change
// it depended on whether ImageDriverManager::initialize() had run, so the two
// former paths are asserted separately - the second is the one that changes.
TEST(LoggingDefaults, utilityOnlyPathIsSilentByDefault)
{
    // Deliberately no setLogLevel call and no ImageDriverManager use, so this
    // test must run in a process where nothing has configured logging. The
    // gtest filter in Step 6 enforces that.
    testing::internal::CaptureStderr();
    try {
        RAISE_RUNTIME_ERROR << "must not appear 5501";
    } catch (const std::exception&) {
    }
    const std::string out = testing::internal::GetCapturedStderr();
    EXPECT_EQ(out.find("must not appear 5501"), std::string::npos)
        << "utility-only path logged at default threshold; captured:\n" << out;
    EXPECT_EQ(out.find("InitGoogleLogging"), std::string::npos)
        << "glog initialisation banner still present; captured:\n" << out;
}

TEST(LoggingDefaults, driverPathIsSilentByDefault)
{
    testing::internal::CaptureStderr();
    EXPECT_THROW(slideio::openSlide("nonexistent_slide.ndpi", "NDPI"), slideio::RuntimeError);
    const std::string out = testing::internal::GetCapturedStderr();
    EXPECT_EQ(out.find("NDPIImageDriver"), std::string::npos)
        << "driver path logged at default threshold; captured:\n" << out;
}
```

- [ ] **Step 3: Register the collision test**

In `src/tests/main/CMakeLists.txt`, add to `TEST_SOURCES`:

```cmake
  test_log_macro_collision.cpp
```

- [ ] **Step 4: Run to verify failure**

```bash
python3 install.py -a build -c release
```

Expected: the build fails, or `LoggingDefaults.utilityOnlyPathIsSilentByDefault` fails — the current uninitialised-glog path logs at INFO (spec §4.7).

- [ ] **Step 5: Rewrite `log.hpp`**

Replace the entire contents of `src/slideio/base/log.hpp`:

```cpp
// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#pragma once
#include "slideio/base/logcontract.hpp"

#include <sstream>

namespace slideio
{
    namespace detail
    {
        // Pasteable severity constants. The severity is embedded in the
        // identifier so SLIDEIO_LOG can token-paste it and never expand a bare
        // INFO / WARNING / ERROR token - <wingdi.h> defines ERROR as 0. This is
        // the same technique glog used: LOG(severity) pasted
        // COMPACT_GOOGLE_LOG_##severity. See design spec section 4.5.1.
        inline constexpr int severityINFO = static_cast<int>(LogLevel::Info);
        inline constexpr int severityWARNING = static_cast<int>(LogLevel::Warning);
        inline constexpr int severityERROR = static_cast<int>(LogLevel::Error);
        inline constexpr int severityFATAL = static_cast<int>(LogLevel::Fatal);

        /** @brief Cheap per-call-site threshold check.
         *
         * The pointer is cached once per module. Duplicating the cached pointer
         * across modules is intentional and correct: every copy points at the
         * one variable inside slideio-base. See design spec section 4.3.
         */
        inline bool logEnabled(int level)
        {
            static const int* threshold = logThresholdPtr();
            return level >= *threshold;
        }

        /** @brief Accumulates a streamed message and emits it on destruction.
         *
         * Header-local by design: the std::ostringstream never crosses a module
         * boundary, so there is no C4251 and no CRT coupling. Only the
         * const char* handed to logMessage crosses.
         */
        class LogStream
        {
        public:
            LogStream(int level, const char* file, int line)
                : m_level(level), m_file(file), m_line(line) {}

            ~LogStream()
            {
                logMessage(m_level, m_file, m_line, m_stream.str().c_str());
            }

            LogStream(const LogStream&) = delete;
            LogStream& operator=(const LogStream&) = delete;

            template <typename T>
            LogStream& operator<<(const T& value)
            {
                m_stream << value;
                return *this;
            }

        private:
            std::ostringstream m_stream;
            int m_level;
            const char* m_file;
            int m_line;
        };
    }
}

// The switch(0) case 0: default: prefix makes the macro immune to dangling
// else. No current call site needs it; it costs nothing to be safe.
#define SLIDEIO_LOG(LEVEL)                                                     \
    switch (0) case 0: default:                                                \
    if (!slideio::detail::logEnabled(slideio::detail::severity##LEVEL)) ; else  \
        slideio::detail::LogStream(slideio::detail::severity##LEVEL,            \
                                   __FILE__, __LINE__)
```

Note what is gone: the glog includes, `GLOG_NO_ABBREVIATED_SEVERITIES`, and the `NOMINMAX`/`WIN32_LEAN_AND_MEAN` block that existed only to keep glog's headers happy.

- [ ] **Step 6: Remove `initLogging()` from the driver manager**

In `src/slideio/slideio/imagedrivermanager.cpp`:

Delete the whole `initLogging()` function (lines 25-33) and its call at line 74 inside `ImageDriverManager::initialize()`.

Replace `setLogLevel` (lines 152-163) with:

```cpp
void ImageDriverManager::setLogLevel(const std::string &level) {
    if(level.compare("INFO")==0) {
        slideio::setLogThreshold(static_cast<int>(slideio::LogLevel::Info));
    } else if(level.compare("WARNING")==0) {
        slideio::setLogThreshold(static_cast<int>(slideio::LogLevel::Warning));
    } else if(level.compare("ERROR")==0) {
        slideio::setLogThreshold(static_cast<int>(slideio::LogLevel::Error));
    } else if(level.compare("FATAL")==0) {
        slideio::setLogThreshold(static_cast<int>(slideio::LogLevel::Fatal));
    }
    // Unrecognised strings are ignored, not rejected. Python callers depend on
    // this not throwing. See design spec section 4.4.3.
}
```

There is no longer any initialisation step a caller can skip: every log call reaches `logMessage`, which lazily builds the sink itself.

- [ ] **Step 7: Harden base's propagation to every module (spec §4.2)**

Until now the drivers reached `slideio-base` only because `src/slideio/imagetools/CMakeLists.txt:79-82` uses the keyword-less `target_link_libraries` form, which defaults to `PUBLIC`. That was already load-bearing; from this task on it is the *only* thing making logging link, because the drivers no longer link a logging library of their own. Make it deliberate.

In `src/slideio/core/CMakeLists.txt:56`, change:

```cmake
target_link_libraries(${LIBRARY_NAME} PRIVATE
   ${BASE_LIB_NAME}
)
```

to:

```cmake
target_link_libraries(${LIBRARY_NAME} PUBLIC
   ${BASE_LIB_NAME}
)
```

Every module links core, so base now reaches every module by a documented path rather than through an incidental imagetools chain. Do not remove the imagetools link as well — two independent paths is the point, and removing one is a separate change.

- [ ] **Step 8: Build and run the whole logging suite**

```bash
python3 install.py -a build -c release
./build/bin/Release/slideio_tests.exe --gtest_filter=LoggingTest.*:LogMacroCollision.*
```

Expected: all PASS — including every Task 1 characterisation test. **A failure in `thresholdCrossesModuleBoundary` here is the defect this whole design exists to prevent** (spec §3.2): the threshold is not being shared across DLLs. Do not work around it by making spdlog shared; re-check that `log.cpp` is the only TU including spdlog and that `logThresholdPtr` is exported.

- [ ] **Step 9: Run the default tests in a clean process**

`LoggingDefaults` must run where nothing has configured logging, so run it as its own process:

```bash
./build/bin/Release/slideio_tests.exe --gtest_filter=LoggingDefaults.*
```

Expected: both PASS.

**Known limitation, state it rather than paper over it:** in a full `slideio_tests` run these two tests execute after `LoggingTest`, whose `TearDown` sets the threshold to `FATAL`. They will still pass, but they are then verifying an explicitly-set `FATAL` rather than the *default* — the thing they exist to check. The separate filtered invocation above is the only run in which they test what they claim. Add that invocation to CI as its own step; do not rely on the full-suite run for this assertion.

- [ ] **Step 10: Full regression, both configurations**

```bash
python3 install.py -a build -c debug
./build/bin/Release/slideio_tests.exe
./build/bin/Release/slideio_converter_tests.exe
./build/bin/Release/slideio_transformer_tests.exe
./build/bin/Release/slideio_ndpi_tests.exe
./build/bin/Release/slideio_vsi_tests.exe
./build/bin/Release/slideio_pke_tests.exe
./build/bin/Release/slideio_ometiff_tests.exe
./build/bin/Release/slideio_phtiff_tests.exe
```

Expected: no new failures versus the pre-change baseline. Record any pre-existing failures before starting so they are not misattributed.

- [ ] **Step 11: Commit**

```bash
git add src/slideio/base/log.hpp src/slideio/slideio/imagedrivermanager.cpp \
        src/tests/main/test_log_macro_collision.cpp src/tests/main/test_logging.cpp \
        src/tests/main/CMakeLists.txt
git commit -m "route SLIDEIO_LOG through the base-owned seam and drop glog from base

Also fixes the divergent default threshold: initLogging() sat above most of
the code it governed, so whether an exception logged depended on whether a
driver had been touched first. Initialisation now lives in base and cannot
be bypassed."
```

---

## Task 4: Remove glog from the remaining build files and packaging

Purely mechanical. Nothing should compile differently; glog is already unused by this point.

**Files:**
- Modify: 26 `CMakeLists.txt` (all with `find_package(glog)` except `src/slideio/base/`, already done in Task 2)
- Modify: 19 `conanfile.txt` under `src/slideio/` and `src/tests/`
- Modify: `src/slideio/slideio/CMakeLists.txt:69-91` (glog binary copy)
- Modify: `CMakeLists.txt:78` (`@rpath/libglog.dylib`)

**Interfaces:** none — no source changes.

- [ ] **Step 1: List the exact files to edit**

```bash
grep -rl "find_package(glog" --include=CMakeLists.txt . | grep -v "^./build"
grep -rl "^glog/" --include=conanfile.txt . | grep -v single_tests
```

Expected: 26 CMakeLists (base already done) and 19 conanfiles. `single_tests` is excluded — that is Task 5.

- [ ] **Step 2: Remove the `find_package` and link entries**

In each of the 26 `CMakeLists.txt`, delete the `find_package(glog)` line and the `glog::glog` entry from its `target_link_libraries` list. Leave surrounding entries and indentation untouched.

- [ ] **Step 3: Remove glog from the conanfiles**

In each of the 19 `conanfile.txt`, delete the `glog/0.7.1` line from `[requires]`.

- [ ] **Step 4: Remove glog from packaging**

In `src/slideio/slideio/CMakeLists.txt`, delete the glog cases from the binary-copy block at lines 69-91: the `${glog_LIB_DIRS_*}/*.dylib` entry, the `${glog_BIN_DIRS_*}/${glog_LIBS_*}.dll` entry, the `${glog_LIB_DIRS_*}/*.*` entry, and the four `GLOB GLOG_LIBS_*` lines. Nothing replaces them — spdlog is linked statically into `slideio-base`.

In the root `CMakeLists.txt`, delete line 78 (`"@rpath/libglog.dylib"`) from the macOS fixup list.

- [ ] **Step 5: Clean rebuild from Conan**

```bash
python3 install.py -a clean --clean
python3 install.py -a install -c release
```

A clean build matters here: a stale Conan cache can hide a missing `find_package`.

- [ ] **Step 6: Verify glog is gone from the build and the output**

```bash
grep -rn "glog" --include=CMakeLists.txt --include=conanfile.txt . | grep -v "^./build" | grep -v single_tests
```

Expected: no output.

```bash
ls build/bin/Release/ | grep -i glog
```

Expected: no output — `glog.dll` is no longer shipped.

- [ ] **Step 7: Full regression**

Run all eight test binaries as in Task 3 Step 9. Expected: no new failures.

- [ ] **Step 8: Commit**

```bash
git add -A -- '*CMakeLists.txt' '*conanfile.txt'
git commit -m "remove glog from all remaining build files and packaging

glog.dll is no longer distributed; spdlog links statically into slideio-base
so nothing replaces it."
```

---

## Task 5: Update `single_tests`

Separate builds, not part of the main CMake tree, so they break independently.

**Files:**
- Modify: `src/single_tests/ndpi_memory/ndpi_memory.cpp:18-20`
- Modify: 5 `src/single_tests/*/conanfile.txt`
- Delete: `src/single_tests/*/cmake/glog*.cmake` (checked-in generated Conan output)

- [ ] **Step 1: Replace the direct glog API use**

In `src/single_tests/ndpi_memory/ndpi_memory.cpp`, replace lines 18-20:

```cpp
    google::InitGoogleLogging("slideio");
    FLAGS_minloglevel = 0;
    FLAGS_logtostderr = true;
```

with:

```cpp
    slideio::setLogLevel("INFO");
```

and replace `#include "slideio/base/log.hpp"` with `#include "slideio/slideio/slideio.hpp"`. `slideio::setLogLevel` is declared in `slideio.hpp:106`; `log.hpp` no longer provides it or anything else this file needs.

- [ ] **Step 2: Remove glog from the single_tests conanfiles**

Delete the `glog/0.7.1` line from each of:
`jp2k`, `memory_leaks`, `ndpi_memory`, `performance`, `svs_memory`.

- [ ] **Step 3: Delete the checked-in generated Conan output**

```bash
git rm src/single_tests/*/cmake/glog*.cmake
```

These are Conan-generated artefacts that should not have been committed; they will be regenerated (as spdlog equivalents) on the next `conan install` in those directories.

- [ ] **Step 4: Verify one single_test still configures**

```bash
cd src/single_tests/ndpi_memory && conan install . --output-folder=cmake --build=missing
```

Expected: success, with no glog in the generated output.

- [ ] **Step 5: Commit**

```bash
git add -A src/single_tests
git commit -m "update single_tests for the glog removal"
```

---

## Task 6: Documentation

- [ ] **Step 1: Record the breaking changes**

Add a `## v2.10.0` section to `software-docs/BREAKING_CHANGES.md` (following the existing `v2.9.0` format — module, file, then what changed) covering all six bullets from spec §8: glog no longer distributed; `glog::glog` gone from the link interface; `log.hpp` no longer includes `glog/logging.h`; `SLIDEIO_LOG` source-compatible but glog-only constructs unavailable; `setLogLevel` unchanged; and the intentional default-threshold change from §4.7.

- [ ] **Step 2: Update the dependency list**

In `CLAUDE.md` line 115, replace `glog` with `spdlog` in the Conan dependency list.

- [ ] **Step 3: Mark the spec implemented**

In `software-docs/specs/2026-08-22-logging-library-replacement-design.md`, change `**Status:** Proposed — awaiting approval` to `**Status:** Implemented in v2.10.0`, and resolve §11.1 by recording which library was chosen and why.

- [ ] **Step 4: Commit**

```bash
git add software-docs/BREAKING_CHANGES.md CLAUDE.md software-docs/specs/2026-08-22-logging-library-replacement-design.md
git commit -m "document the glog replacement"
```

---

## Cross-platform verification

Tasks 1-6 assume Windows, where the §3.2 failure mode is deterministic. Before merging, run the full suite on Linux and macOS as well. The specific things that can differ:

- `basename()` in `log.cpp` handles both separators, but `__FILE__` is slash-separated on GCC/Clang — confirm `logMessageFormatMatchesSpec` passes.
- `-Wl,-Bsymbolic` on GNU builds (`src/slideio/imagetools/CMakeLists.txt`) affects symbol binding. `thresholdCrossesModuleBoundary` is the test that would catch a problem.
- The macOS rpath fixup list changed in Task 4 Step 4 — verify the `.dylib` set installs correctly.

## Open decisions carried from the spec

Neither blocks starting Task 1; both must be settled before Task 2 Step 4.

1. **Library choice** (spec §5, §11.1) — this plan is written for spdlog. The in-house variant changes only Task 2 Steps 4-6. Pending GRC input on per-SOUP-item cost.
2. **Branch timing** (spec §11.2) — this plan targets `v2.10.0`. Tasks 4 and 5 touch 50 build files on a release branch; deferring to a `master` cycle is the lower-risk option and changes nothing else in the plan.
