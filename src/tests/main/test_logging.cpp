// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#include <gtest/gtest.h>
#include <cctype>
#include <cstddef>
#include <string>
#include <thread>
#include <vector>
#if defined(_MSC_VER)
#include <io.h>
#include <fcntl.h>
#else
#include <unistd.h>
#include <fcntl.h>
#endif
#include "slideio/core/exceptions.hpp"
#include "slideio/core/logcontract.hpp"
#include "slideio/slideio/slideio.hpp"

using namespace slideio;

namespace {
    // Restores the log level after each test so ordering cannot leak between
    // tests. FATAL is the library default (spec 4.7).
    class LoggingTest : public ::testing::Test {
    protected:
        void TearDown() override {
            slideio::setLogLevel("FATAL");
            slideio::setLogThreshold(static_cast<int>(slideio::LogLevel::Fatal));
        }
    };
}

// Captured during this TU's dynamic initialisation. g_threshold in log.cpp is
// CONSTANT-initialised, which is guaranteed to complete before any dynamic
// initialisation in any TU - so this observes the true initial value, before
// any test (or setter call) can have run. See spec 4.7.1.
const int g_thresholdAtStaticInit = *slideio::logThresholdPtr();

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

// Spec 4.1 / 7.2: RuntimeError::log lives in slideio-core and is reached from
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

// Spec 4.7.1: the threshold is constant-initialised to Fatal. Reading it
// before anything has configured logging must yield Fatal, not an
// indeterminate value. Asserting on *logThresholdPtr() here would only prove
// the setter works, since some earlier test in this binary may already have
// called setLogThreshold - so this asserts on g_thresholdAtStaticInit,
// captured once during this TU's dynamic initialisation, before any test (or
// setter call) can have run. Do not call setLogThreshold in this test.
TEST_F(LoggingTest, thresholdDefaultsToFatal)
{
    EXPECT_EQ(g_thresholdAtStaticInit, static_cast<int>(slideio::LogLevel::Fatal));

    const int* threshold = slideio::logThresholdPtr();
    ASSERT_NE(threshold, nullptr);
    EXPECT_EQ(slideio::logThresholdPtr(), threshold) << "pointer must be stable across calls";
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

// Spec 4.6: format must match the captured glog line field-for-field. Checked
// character-class by character-class (not by substring search) so that field
// widths and ordering are actually pinned - a substring search for e.g.
// "payload 9021" would still pass for a malformed date or a 2-digit fraction.
TEST_F(LoggingTest, logMessageFormatMatchesSpec)
{
    slideio::setLogThreshold(static_cast<int>(slideio::LogLevel::Error));
    testing::internal::CaptureStderr();
    slideio::logMessage(static_cast<int>(slideio::LogLevel::Error),
                        "D:\\some\\path\\widget.cpp", 42, "payload 9021");
    const std::string out = testing::internal::GetCapturedStderr();

    // E20260822 19:31:43.120792 18220 widget.cpp:42] payload 9021
    ASSERT_FALSE(out.empty()) << "nothing emitted";
    ASSERT_GE(out.size(), 26u) << "line too short to hold the fixed-width prefix; got: " << out;

    EXPECT_EQ(out[0], 'E') << "severity initial wrong; got: " << out;

    // yyyymmdd - exactly 8 digits.
    for (std::size_t i = 1; i <= 8; ++i) {
        EXPECT_TRUE(std::isdigit(static_cast<unsigned char>(out[i])))
            << "date digit at index " << i << " is not a digit; got: " << out;
    }
    EXPECT_EQ(out[9], ' ') << "missing space between date and time; got: " << out;

    // hh:mm:ss - digit digit ':' digit digit ':' digit digit.
    for (std::size_t i : {std::size_t{10}, std::size_t{11}, std::size_t{13},
                          std::size_t{14}, std::size_t{16}, std::size_t{17}}) {
        EXPECT_TRUE(std::isdigit(static_cast<unsigned char>(out[i])))
            << "time digit at index " << i << " is not a digit; got: " << out;
    }
    EXPECT_EQ(out[12], ':') << "missing ':' in time; got: " << out;
    EXPECT_EQ(out[15], ':') << "missing ':' in time; got: " << out;
    EXPECT_EQ(out[18], '.') << "missing '.' before the fraction; got: " << out;

    // Fraction must be EXACTLY 6 digits, then a space - the microsecond
    // width called out in spec 4.6, not whatever the clock's precision
    // happens to produce.
    for (std::size_t i = 19; i <= 24; ++i) {
        EXPECT_TRUE(std::isdigit(static_cast<unsigned char>(out[i])))
            << "fraction digit at index " << i << " is not a digit; got: " << out;
    }
    EXPECT_EQ(out[25], ' ')
        << "fraction must be exactly 6 digits, followed by a space; got: " << out;

    // Thread id: one or more digits, then a space. Width is unspecified, so
    // scan for it instead of indexing a fixed offset.
    std::size_t pos = 26;
    const std::size_t threadIdStart = pos;
    while (pos < out.size() && std::isdigit(static_cast<unsigned char>(out[pos]))) {
        ++pos;
    }
    EXPECT_GT(pos, threadIdStart) << "thread id field is empty; got: " << out;
    ASSERT_LT(pos, out.size()) << "line ends before the location field; got: " << out;
    EXPECT_EQ(out[pos], ' ') << "missing space after thread id; got: " << out;
    ++pos;

    EXPECT_EQ(out.compare(pos, std::string::npos, "widget.cpp:42] payload 9021\n"), 0)
        << "basename:line]/message/newline tail wrong; got: " << out;
    EXPECT_EQ(out.find("D:\\some\\path"), std::string::npos)
        << "location field must be the basename only; got: " << out;
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
    testing::internal::CaptureStderr();
    // EXPECT_NO_THROW cannot itself fail here: logMessage is noexcept, so a
    // throw would call std::terminate before EXPECT_NO_THROW's own catch
    // ever ran. The real signal these lines check for is "the process did
    // not crash" on degenerate inputs (null file/message, an out-of-range
    // level, a negative level) - not a gtest assertion failure.
    EXPECT_NO_THROW(slideio::logMessage(static_cast<int>(slideio::LogLevel::Error), nullptr, 0, nullptr));
    EXPECT_NO_THROW(slideio::logMessage(9999, "f.cpp", 1, ""));
    EXPECT_NO_THROW(slideio::logMessage(-1, "f.cpp", 1, "negative level"));
    testing::internal::GetCapturedStderr();
}

// Direct concurrency coverage for slideio::logMessage (the existing
// concurrentLoggingProducesWholeLines test only reaches logging through
// RAISE_RUNTIME_ERROR/glog). The per-call sink construction in log.cpp is
// exactly the part of the design most plausibly at risk from concurrent
// access, so this exercises it directly from multiple threads.
TEST_F(LoggingTest, logMessageIsThreadSafe)
{
    slideio::setLogThreshold(static_cast<int>(slideio::LogLevel::Error));
    testing::internal::CaptureStderr();
    {
        std::vector<std::thread> workers;
        for (int t = 0; t < 4; ++t) {
            workers.emplace_back([t]() {
                for (int i = 0; i < 50; ++i) {
                    const std::string marker =
                        "logmsg-thread" + std::to_string(t) + "-iter" + std::to_string(i);
                    slideio::logMessage(static_cast<int>(slideio::LogLevel::Error),
                                        "concurrent.cpp", t, marker.c_str());
                }
            });
        }
        for (auto& w : workers) {
            w.join();
        }
    }
    const std::string out = testing::internal::GetCapturedStderr();

    for (int t = 0; t < 4; ++t) {
        for (int i = 0; i < 50; ++i) {
            const std::string marker =
                "logmsg-thread" + std::to_string(t) + "-iter" + std::to_string(i);
            EXPECT_NE(out.find(marker), std::string::npos) << "torn or lost line: " << marker;
        }
    }
}

// Spec: setLogThreshold silently ignores out-of-range values (see the doc
// comment in logcontract.hpp) rather than clamping or asserting - callers
// (including Python, via slideio's bindings) must not be able to corrupt the
// threshold with a bad value.
TEST_F(LoggingTest, setLogThresholdIgnoresOutOfRangeValues)
{
    slideio::setLogThreshold(static_cast<int>(slideio::LogLevel::Warning));
    ASSERT_EQ(*slideio::logThresholdPtr(), static_cast<int>(slideio::LogLevel::Warning));

    slideio::setLogThreshold(-1);
    EXPECT_EQ(*slideio::logThresholdPtr(), static_cast<int>(slideio::LogLevel::Warning))
        << "an out-of-range negative value must not change the threshold";

    slideio::setLogThreshold(9999);
    EXPECT_EQ(*slideio::logThresholdPtr(), static_cast<int>(slideio::LogLevel::Warning))
        << "an out-of-range positive value must not change the threshold";
}

// Spec 7.6 - THE test that catches spdlog's throw-by-default behaviour. A
// static_assert only proves the declaration; this proves the implementation
// survives a sink that actually fails. Forces the failure by putting a
// read-only descriptor over stderr, so every write returns an error.
TEST_F(LoggingTest, failingSinkCannotEscapeLogMessage)
{
    slideio::setLogThreshold(static_cast<int>(slideio::LogLevel::Error));
    // Establishes a baseline - logMessage works normally before stderr is
    // broken - so the assertions below are actually about surviving a write
    // failure. (log.cpp builds a fresh sink on every call; there is no
    // shared construction left to warm up, unlike the cached-singleton
    // design this replaced.)
    testing::internal::CaptureStderr();
    slideio::logMessage(static_cast<int>(slideio::LogLevel::Error), "warmup.cpp", 1, "warmup");
    testing::internal::GetCapturedStderr();

#if defined(_MSC_VER)
    const int saved = _dup(2);
    const int readOnly = _open("nul", _O_RDONLY);
#else
    const int saved = dup(2);
    const int readOnly = open("/dev/null", O_RDONLY);
#endif
    ASSERT_NE(saved, -1);
    ASSERT_NE(readOnly, -1);

    // RAII guard: if an assertion below fires or anything else escapes
    // before the manual restore at the end of this test, fd 2 must not stay
    // pointed at a read-only/broken destination for the ~500 remaining tests
    // in this process - every later testing::internal::GetCapturedStderr()
    // would silently come back empty.
    struct StderrRestorer {
        int saved;
        ~StderrRestorer()
        {
#if defined(_MSC_VER)
            _dup2(saved, 2);
            _close(saved);
#else
            dup2(saved, 2);
            close(saved);
#endif
        }
    } restoreStderr{saved};

#if defined(_MSC_VER)
    ASSERT_NE(_dup2(readOnly, 2), -1);
#else
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
    _close(readOnly);
#else
    close(readOnly);
#endif

    SUCCEED() << "logMessage survived a failing sink";
    // restoreStderr's destructor runs here, putting fd 2 back.
}

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
