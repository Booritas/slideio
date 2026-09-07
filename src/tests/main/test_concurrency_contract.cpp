// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#include <gtest/gtest.h>
#include "tests/testlib/testscene.hpp"

// TestScene is the in-tree fake CVScene used by the tile-composer tests. It does
// not override supportsConcurrentReads, so it must report the safe default.
TEST(ConcurrencyContract, defaultsToSerialised) {
    TestScene scene;
    EXPECT_FALSE(scene.supportsConcurrentReads());
}

namespace
{
    class ConcurrentTestScene : public TestScene
    {
    public:
        bool supportsConcurrentReads() const override { return true; }
    };
}

TEST(ConcurrencyContract, driverCanOptIn) {
    ConcurrentTestScene scene;
    EXPECT_TRUE(scene.supportsConcurrentReads());
}
