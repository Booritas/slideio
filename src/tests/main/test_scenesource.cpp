// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#include <gtest/gtest.h>
#include <cstring>
#include "slideio/slideio/scenemetadata.hpp"

using namespace slideio;

TEST(SceneMetadata, defaultsAreTheDocumentedOnes)
{
    SceneMetadata m;
    EXPECT_TRUE(m.name.empty());
    EXPECT_TRUE(m.reportedFilePath.empty());
    EXPECT_TRUE(m.rect.empty());
    EXPECT_TRUE(m.channelDataTypes.empty());
    EXPECT_TRUE(m.channelNames.empty());
    EXPECT_TRUE(m.channelSignificantBits.empty());
    EXPECT_DOUBLE_EQ(0.0, m.resolution.x);
    EXPECT_DOUBLE_EQ(0.0, m.resolution.y);
    EXPECT_DOUBLE_EQ(0.0, m.magnification);
    EXPECT_EQ(Compression::Uncompressed, m.compression);
    EXPECT_EQ(1, m.numZSlices);
    EXPECT_EQ(1, m.numTFrames);
    EXPECT_DOUBLE_EQ(0.0, m.zSliceResolution);
    EXPECT_DOUBLE_EQ(0.0, m.tFrameResolution);
    EXPECT_TRUE(m.levels.empty());
}

TEST(SceneMetadata, isAnAggregateCallersCanFillFieldByField)
{
    SceneMetadata m;
    m.name = "computed";
    m.rect = Rect(0, 0, 400, 300);
    m.channelDataTypes = {DataType::DT_Float32, DataType::DT_Float32};
    m.levels.emplace_back(0, Size(400, 300), 1.0, 20.0, Size(256, 256));

    EXPECT_EQ("computed", m.name);
    EXPECT_EQ(400, m.rect.width);
    EXPECT_EQ(2u, m.channelDataTypes.size());
    EXPECT_EQ(1u, m.levels.size());
    EXPECT_DOUBLE_EQ(1.0, m.levels[0].getScale());
}

#include "slideio/slideio/scenesource.hpp"
#include "tests/testlib/testscenesource.hpp"

namespace {
    // The smallest legal source: implements the one pure virtual and nothing else.
    class MinimalSource : public ISceneSource {
    public:
        void readBlock(const Rect&, const Size&, const std::vector<int>&,
                       int, int, void* buffer, size_t bufferSize) override {
            std::memset(buffer, 0, bufferSize);
        }
    };
}

TEST(ISceneSource, aMinimalSourceGetsTheDocumentedDefaults)
{
    MinimalSource s;
    EXPECT_FALSE(s.readLevelBlock(0, Rect(0,0,1,1), Size(1,1), {}, 0, 0, nullptr, 0));
    EXPECT_FALSE(s.supportsConcurrentReads());
    EXPECT_TRUE(s.getRawMetadata().empty());
    EXPECT_TRUE(s.getAuxImageNames().empty());
    EXPECT_EQ(nullptr, s.getAuxImage("anything"));
}
