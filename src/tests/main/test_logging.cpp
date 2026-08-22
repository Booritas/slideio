// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#include <gtest/gtest.h>
#include <string>
#include <cstddef>
#include <thread>
#include <vector>
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
