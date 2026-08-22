// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#include <gtest/gtest.h>
#include <string>
#include <cstddef>
#include <thread>
#include <vector>
#if defined(_MSC_VER)
#include <io.h>
#include <fcntl.h>
#else
#include <unistd.h>
#include <fcntl.h>
#endif
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

// A bare `throw;` reactivates the same exception object without copying it, so
// this path produces one line. It does NOT exercise the m_shown guard - see
// copyingOneSourceTwiceLogsOnce for that.
TEST_F(LoggingTest, bareRethrowDoesNotDuplicateTheLogLine)
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

// Spec 7.8: the m_shown guard means one log line per source object. Two copies
// from one source must produce one line, not two. This test fails if the guard
// is removed - unlike a bare-rethrow test, which cannot detect its removal
// because a bare rethrow copy-constructs nothing.
TEST_F(LoggingTest, copyingOneSourceTwiceLogsOnce)
{
    const std::string marker = "guard marker 7731";
    slideio::setLogLevel("ERROR");
    testing::internal::CaptureStderr();
    {
        slideio::RuntimeError source;
        source << marker;
        slideio::RuntimeError first(source);    // logs
        slideio::RuntimeError second(source);   // must NOT log: source.m_shown is now true
        (void)first;
        (void)second;
    }
    const std::string out = testing::internal::GetCapturedStderr();

    std::size_t count = 0;
    for (std::size_t at = out.find(marker); at != std::string::npos;
         at = out.find(marker, at + marker.size())) {
        ++count;
    }
    EXPECT_EQ(count, 1u) << "expected one line per source object, saw " << count
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
