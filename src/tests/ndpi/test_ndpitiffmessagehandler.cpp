// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#include <gtest/gtest.h>
#include "slideio/drivers/ndpi/ndpitiffmessagehandler.hpp"
#include "slideio/drivers/ndpi/ndpitiffkeeper.hpp"
#include <tiffio.h>

static TIFFErrorHandler currentNDPIErrorHandler() {
    TIFFErrorHandler previous = TIFFSetErrorHandler(nullptr);
    TIFFSetErrorHandler(previous);
    return previous;
}

TEST(NDPITiffMessageHandler, installsHandler) {
    slideio::installNDPITiffMessageHandlers();
    EXPECT_NE(currentNDPIErrorHandler(), nullptr);
}

TEST(NDPITiffMessageHandler, keeperLifetimeDoesNotChangeHandler) {
    slideio::installNDPITiffMessageHandlers();
    const TIFFErrorHandler before = currentNDPIErrorHandler();
    {
        slideio::NDPITIFFKeeper keeper;
        EXPECT_EQ(currentNDPIErrorHandler(), before);
    }
    EXPECT_EQ(currentNDPIErrorHandler(), before);
}
