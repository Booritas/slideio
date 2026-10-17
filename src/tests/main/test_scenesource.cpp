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

#include "slideio/slideio/sourcecvscene.hpp"
#include "slideio/slideio/sceneinternal.hpp"
#include "slideio/slideio/scene.hpp"

namespace {
    SceneMetadata describedMetadata()
    {
        SceneMetadata m;
        m.name = "computed";
        m.reportedFilePath = "pipeline://nuclei";
        m.rect = Rect(0, 0, 400, 300);
        m.channelDataTypes = {DataType::DT_Byte, DataType::DT_Byte, DataType::DT_Byte};
        m.channelNames = {"haematoxylin", "eosin", "residual"};
        m.channelSignificantBits = {8, 8, 8};
        m.resolution = Resolution(2.5e-7, 2.5e-7);
        m.magnification = 40.0;
        m.numZSlices = 4;
        m.numTFrames = 5;
        m.zSliceResolution = 2.5e-6;
        m.tFrameResolution = 0.25;
        m.levels.emplace_back(0, Size(400, 300), 1.0,  40.0, Size(256, 256));
        m.levels.emplace_back(1, Size(200, 150), 0.5,  20.0, Size(256, 256));
        return m;
    }

    std::shared_ptr<Scene> sceneOver(const std::shared_ptr<TestSceneSource>& src,
                                     const SceneMetadata& m)
    {
        return SceneInternal::createScene(std::make_shared<SourceCVScene>(src, m));
    }
}

TEST(SourceCVScene, reportsEveryMetadataFieldVerbatim)
{
    auto src = std::make_shared<TestSceneSource>();
    auto scene = sceneOver(src, describedMetadata());

    EXPECT_EQ("computed", scene->getName());
    EXPECT_EQ("pipeline://nuclei", scene->getFilePath());
    EXPECT_EQ(std::make_tuple(0, 0, 400, 300), scene->getRect());
    EXPECT_EQ(3, scene->getNumChannels());
    EXPECT_EQ(DataType::DT_Byte, scene->getChannelDataType(1));
    EXPECT_EQ("eosin", scene->getChannelName(1));
    EXPECT_EQ(8, scene->getChannelSignificantBits(2));
    EXPECT_EQ(std::make_tuple(2.5e-7, 2.5e-7), scene->getResolution());
    EXPECT_DOUBLE_EQ(40.0, scene->getMagnification());
    EXPECT_EQ(4, scene->getNumZSlices());
    EXPECT_EQ(5, scene->getNumTFrames());
    EXPECT_DOUBLE_EQ(2.5e-6, scene->getZSliceResolution());
    EXPECT_DOUBLE_EQ(0.25, scene->getTFrameResolution());
    EXPECT_EQ(Compression::Uncompressed, scene->getCompression());
}

TEST(SourceCVScene, exposesTheDeclaredPyramid)
{
    auto src = std::make_shared<TestSceneSource>();
    auto scene = sceneOver(src, describedMetadata());

    ASSERT_EQ(2, scene->getNumZoomLevels());
    const LevelInfo* level1 = scene->getLevelInfo(1);
    ASSERT_NE(nullptr, level1);
    EXPECT_EQ(200, level1->getSize().width);
    EXPECT_DOUBLE_EQ(0.5, level1->getScale());
    EXPECT_EQ(256, level1->getTileSize().width);
}

TEST(SourceCVScene, levelInfoPointersSurviveTheCallerMetadataGoingAway)
{
    auto src = std::make_shared<TestSceneSource>();
    std::shared_ptr<Scene> scene;
    {
        SceneMetadata m = describedMetadata();
        scene = sceneOver(src, m);
    }   // m destroyed
    const LevelInfo* level0 = scene->getLevelInfo(0);
    ASSERT_NE(nullptr, level0);
    EXPECT_EQ(400, level0->getSize().width);
}
