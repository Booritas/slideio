// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#include <gtest/gtest.h>
#include "slideio/imagetools/tiffmessagehandler.hpp"
#include "slideio/imagetools/libtiff.hpp"
#include "slideio/imagetools/tiffkeeper.hpp"
#include "slideio/slideio/slideio.hpp"

// Reads the currently installed handler without disturbing it: TIFFSetErrorHandler
// returns the previous one, so setting it back restores the status quo.
static libtiff::TIFFErrorHandler currentErrorHandler() {
    libtiff::TIFFErrorHandler previous = libtiff::TIFFSetErrorHandler(nullptr);
    libtiff::TIFFSetErrorHandler(previous);
    return previous;
}

static libtiff::TIFFErrorHandler currentWarningHandler() {
    libtiff::TIFFErrorHandler previous = libtiff::TIFFSetWarningHandler(nullptr);
    libtiff::TIFFSetWarningHandler(previous);
    return previous;
}

TEST(TiffMessageHandler, installsHandlers) {
    slideio::installTiffMessageHandlers();
    EXPECT_NE(currentErrorHandler(), nullptr);
    EXPECT_NE(currentWarningHandler(), nullptr);
}

TEST(TiffMessageHandler, installIsIdempotent) {
    slideio::installTiffMessageHandlers();
    const libtiff::TIFFErrorHandler firstError = currentErrorHandler();
    const libtiff::TIFFErrorHandler firstWarning = currentWarningHandler();
    slideio::installTiffMessageHandlers();
    EXPECT_EQ(currentErrorHandler(), firstError);
    EXPECT_EQ(currentWarningHandler(), firstWarning);
}

// The regression this task exists to prevent: a TIFFKeeper's lifetime must no
// longer change the process-global handlers.
TEST(TiffMessageHandler, keeperLifetimeDoesNotChangeHandlers) {
    slideio::installTiffMessageHandlers();
    const libtiff::TIFFErrorHandler beforeError = currentErrorHandler();
    const libtiff::TIFFErrorHandler beforeWarning = currentWarningHandler();
    {
        slideio::TIFFKeeper keeper;
        EXPECT_EQ(currentErrorHandler(), beforeError);
        EXPECT_EQ(currentWarningHandler(), beforeWarning);
    }
    EXPECT_EQ(currentErrorHandler(), beforeError);
    EXPECT_EQ(currentWarningHandler(), beforeWarning);
}

// Spec §6's "diagnostics still routed" gate. Installing once instead of
// swapping per object must not lose log routing -- which was the whole
// justification for the old swap, so it is the regression to guard.
TEST(TiffMessageHandler, warningsReachTheLog) {
    slideio::installTiffMessageHandlers();
    slideio::setLogLevel("WARNING");
    testing::internal::CaptureStderr();
    libtiff::TIFFWarning("slideio-test", "canary %d", 1234);
    const std::string captured = testing::internal::GetCapturedStderr();
    slideio::setLogLevel("FATAL");
    EXPECT_NE(captured.find("canary 1234"), std::string::npos)
        << "a libtiff warning did not reach the log; captured:\n" << captured;
}
