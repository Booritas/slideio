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
#include "slideio/core/exceptions.hpp"

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

namespace {
    SceneMetadata byteMetadata(int channels = 3)
    {
        SceneMetadata m;
        m.rect = Rect(0, 0, 400, 300);
        m.channelDataTypes.assign(channels, DataType::DT_Byte);
        m.levels.emplace_back(0, Size(400, 300), 1.0, 20.0, Size(256, 256));
        return m;
    }
}

TEST(SourceCVSceneRead, deliversTheSourcePixels)
{
    auto src = std::make_shared<TestSceneSource>();
    auto scene = sceneOver(src, byteMetadata());

    const std::tuple<int,int,int,int> rect(10, 20, 4, 3);
    std::vector<uint8_t> buffer(4 * 3 * 3);
    scene->readBlock(rect, buffer.data(), buffer.size());

    for (int row = 0; row < 3; ++row) {
        for (int col = 0; col < 4; ++col) {
            for (int c = 0; c < 3; ++c) {
                const size_t i = (static_cast<size_t>(row) * 4 + col) * 3 + c;
                EXPECT_EQ(TestSceneSource::expected(10 + col, 20 + row, c), buffer[i])
                    << "at col " << col << " row " << row << " channel " << c;
            }
        }
    }
}

TEST(SourceCVSceneRead, forwardsARectStraddlingTheEdgeUnclamped)   // Review Focus 2
{
    auto src = std::make_shared<TestSceneSource>();
    auto scene = sceneOver(src, byteMetadata());     // scene is 400x300

    // Straddles the right edge: 2 columns inside, 2 outside.
    std::vector<uint8_t> buffer(4 * 2 * 3);
    ASSERT_NO_THROW(scene->readBlock(std::make_tuple(398, 0, 4, 2),
                                     buffer.data(), buffer.size()));

    // The adapter must hand the source the whole rectangle, not a clipped one:
    // filling the outside part with background is the source's documented job,
    // and a clipped rect would silently shift the pixels the caller gets back.
    ASSERT_EQ(1u, src->calls().size());
    EXPECT_EQ(398, src->calls()[0].rect.x);
    EXPECT_EQ(4, src->calls()[0].rect.width);
}

TEST(SourceCVSceneRead, forwardsARectEntirelyOutsideTheScene)   // Review Focus 2
{
    auto src = std::make_shared<TestSceneSource>();
    auto scene = sceneOver(src, byteMetadata());

    std::vector<uint8_t> buffer(2 * 2 * 3);
    EXPECT_NO_THROW(scene->readBlock(std::make_tuple(1000, 1000, 2, 2),
                                     buffer.data(), buffer.size()));
}

TEST(SourceCVSceneRead, emptyChannelSelectionMeansEveryChannelInOrder)
{
    auto src = std::make_shared<TestSceneSource>();
    auto scene = sceneOver(src, byteMetadata());

    std::vector<uint8_t> buffer(2 * 2 * 3);
    scene->readBlockChannels(std::make_tuple(0, 0, 2, 2), {}, buffer.data(), buffer.size());

    ASSERT_EQ(1u, src->calls().size());
    EXPECT_EQ(std::vector<int>({0, 1, 2}), src->calls()[0].channels);
}

TEST(SourceCVSceneRead, selectsAndOrdersTheRequestedChannels)
{
    auto src = std::make_shared<TestSceneSource>();
    auto scene = sceneOver(src, byteMetadata());

    std::vector<uint8_t> buffer(1 * 1 * 2);
    scene->readBlockChannels(std::make_tuple(5, 6, 1, 1), {2, 0}, buffer.data(), buffer.size());

    EXPECT_EQ(TestSceneSource::expected(5, 6, 2), buffer[0]);
    EXPECT_EQ(TestSceneSource::expected(5, 6, 0), buffer[1]);
}

TEST(SourceCVSceneRead, rejectsAnOutOfRangeChannelIndex)
{
    auto src = std::make_shared<TestSceneSource>();
    auto scene = sceneOver(src, byteMetadata());
    std::vector<uint8_t> buffer(16);
    EXPECT_THROW(scene->readBlockChannels(std::make_tuple(0, 0, 2, 2), {0, 9},
                                          buffer.data(), buffer.size()),
                 slideio::RuntimeError);
}

TEST(SourceCVSceneRead, rejectsChannelsOfDifferingDataTypesInOneBlock)
{
    auto src = std::make_shared<TestSceneSource>();
    SceneMetadata m = byteMetadata(2);
    m.channelDataTypes = {DataType::DT_Byte, DataType::DT_Float32};
    auto scene = sceneOver(src, m);

    std::vector<uint8_t> buffer(64);
    EXPECT_THROW(scene->readBlockChannels(std::make_tuple(0, 0, 2, 2), {0, 1},
                                          buffer.data(), buffer.size()),
                 slideio::RuntimeError);
}

TEST(SourceCVSceneRead, readsAMixedTypeSceneOneChannelAtATime)
{
    // The rejection above costs no reach: every channel of a mixed-type scene
    // is still readable, one per call, which is what keeps UR-IMG-10 true of a
    // computed scene. Only a *simultaneous* multi-channel read is restricted,
    // and that is cv::Mat's single depth, not this adapter's choice.
    auto src = std::make_shared<TestSceneSource>();
    SceneMetadata m = byteMetadata(2);
    m.channelDataTypes = {DataType::DT_Byte, DataType::DT_Float32};
    auto scene = sceneOver(src, m);

    std::vector<uint8_t> byteBuffer(2 * 2);
    EXPECT_NO_THROW(scene->readBlockChannels(std::make_tuple(0, 0, 2, 2), {0},
                                             byteBuffer.data(), byteBuffer.size()));

    std::vector<float> floatBuffer(2 * 2);
    EXPECT_NO_THROW(scene->readBlockChannels(std::make_tuple(0, 0, 2, 2), {1},
                                             floatBuffer.data(),
                                             floatBuffer.size() * sizeof(float)));
}

TEST(SourceCVSceneRead, rejectsADegenerateBlock)   // Review Focus 1
{
    auto src = std::make_shared<TestSceneSource>();
    auto scene = sceneOver(src, byteMetadata());
    std::vector<uint8_t> buffer(16);
    EXPECT_THROW(scene->readBlock(std::make_tuple(0, 0, 0, 5), buffer.data(), buffer.size()),
                 slideio::RuntimeError);
    EXPECT_THROW(scene->readBlock(std::make_tuple(0, 0, 5, -1), buffer.data(), buffer.size()),
                 slideio::RuntimeError);
    EXPECT_TRUE(src->calls().empty());
}

TEST(SourceCVSceneRead, rejectsABufferSmallerThanTheBlock)   // Review Focus 3
{
    auto src = std::make_shared<TestSceneSource>();
    auto scene = sceneOver(src, byteMetadata());
    std::vector<uint8_t> tooSmall(5);            // needs 4*3*3 = 36
    EXPECT_THROW(scene->readBlock(std::make_tuple(0, 0, 4, 3), tooSmall.data(), tooSmall.size()),
                 slideio::RuntimeError);
}

TEST(SourceCVSceneRead, aThrowingSourceLeavesTheSceneUsable)   // Review Focus 5
{
    auto src = std::make_shared<TestSceneSource>();
    auto scene = sceneOver(src, byteMetadata());
    std::vector<uint8_t> buffer(2 * 2 * 3);

    src->setThrowOnRead(true);
    EXPECT_THROW(scene->readBlock(std::make_tuple(0, 0, 2, 2), buffer.data(), buffer.size()),
                 std::exception);

    src->setThrowOnRead(false);
    EXPECT_NO_THROW(scene->readBlock(std::make_tuple(0, 0, 2, 2), buffer.data(), buffer.size()));
    EXPECT_EQ(TestSceneSource::expected(0, 0, 0), buffer[0]);
}

TEST(SourceCVSceneRead, servesA4DReadFromASourceThatImplementsOnlyReadBlock)
{
    auto src = std::make_shared<TestSceneSource>();
    SceneMetadata m = byteMetadata();
    m.numZSlices = 3;
    auto scene = sceneOver(src, m);

    std::vector<uint8_t> buffer(2 * 2 * 3 * 2);   // two z slices
    scene->read4DBlock(std::make_tuple(0, 0, 2, 2), std::make_tuple(0, 2),
                       std::make_tuple(0, 1), buffer.data(), buffer.size());

    ASSERT_EQ(2u, src->calls().size());
    EXPECT_EQ(0, src->calls()[0].zSlice);
    EXPECT_EQ(1, src->calls()[1].zSlice);
}

TEST(SourceCVSceneRead, rejectsADegenerateRectBeforeCallingTheSource)   // Review Focus 1
{
    auto src = std::make_shared<TestSceneSource>();
    auto scene = sceneOver(src, byteMetadata());
    std::vector<uint8_t> buffer(5 * 5 * 3);
    EXPECT_THROW(scene->readResampledBlock(std::make_tuple(0, 0, 0, 5), std::make_tuple(5, 5),
                                           buffer.data(), buffer.size()),
                 slideio::RuntimeError);
    EXPECT_THROW(scene->readResampledBlock(std::make_tuple(0, 0, 5, -2), std::make_tuple(5, 5),
                                           buffer.data(), buffer.size()),
                 slideio::RuntimeError);
    EXPECT_TRUE(src->calls().empty());
}

namespace {
    SceneMetadata twoLevelMetadata()
    {
        SceneMetadata m;
        m.rect = Rect(0, 0, 400, 300);
        m.channelDataTypes.assign(3, DataType::DT_Byte);
        m.levels.emplace_back(0, Size(400, 300), 1.0, 40.0, Size(256, 256));
        m.levels.emplace_back(1, Size(200, 150), 0.5, 20.0, Size(256, 256));
        return m;
    }
}

TEST(SourceCVSceneLevels, asksASourceWithAPyramidDirectly)
{
    auto src = std::make_shared<TestSceneSource>();
    src->setHasPyramid(true);
    auto scene = sceneOver(src, twoLevelMetadata());

    std::vector<uint8_t> buffer(2 * 2 * 3);
    scene->readResampledLevelBlockChannels(1, std::make_tuple(10, 10, 2, 2),
                                           std::make_tuple(2, 2), {}, buffer.data(), buffer.size());

    ASSERT_EQ(1u, src->calls().size());
    EXPECT_EQ(1, src->calls()[0].level);                  // the named level, not -1
    EXPECT_EQ(10, src->calls()[0].rect.x);                // level coordinates, unscaled
}

TEST(SourceCVSceneLevels, derivesTheReadWhenTheSourceDeclinesTheLevel)
{
    auto src = std::make_shared<TestSceneSource>();
    src->setHasPyramid(false);                            // readLevelBlock returns false
    auto scene = sceneOver(src, twoLevelMetadata());

    std::vector<uint8_t> buffer(2 * 2 * 3);
    scene->readResampledLevelBlockChannels(1, std::make_tuple(10, 10, 2, 2),
                                           std::make_tuple(2, 2), {}, buffer.data(), buffer.size());

    ASSERT_EQ(1u, src->calls().size());
    EXPECT_EQ(-1, src->calls()[0].level);                 // came through readBlock
    EXPECT_EQ(20, src->calls()[0].rect.x);                // scaled to scene coordinates by 1/0.5
}

// The two level paths clamp differently, and an implementer who assumes
// otherwise ships wrong edge tiles. CVScene::readClampedBlock is reached only
// from the base readResampledLevelBlockChannelsEx (cvscene.cpp:233): a source
// that DECLINES the level is clamped and background-filled for it, and one
// that IMPLEMENTS the level is handed the rectangle untouched. Level 1 is
// 200x150, so (190, 140, 20, 20) hangs 10 px off both edges.

TEST(SourceCVSceneLevels, aSourceThatImplementsTheLevelGetsTheRectUnclamped)
{
    auto src = std::make_shared<TestSceneSource>();
    src->setHasPyramid(true);
    auto scene = sceneOver(src, twoLevelMetadata());

    std::vector<uint8_t> buffer(20 * 20 * 3);
    scene->readResampledLevelBlockChannels(1, std::make_tuple(190, 140, 20, 20),
                                           std::make_tuple(20, 20), {},
                                           buffer.data(), buffer.size());

    ASSERT_EQ(1u, src->calls().size());
    EXPECT_EQ(190, src->calls()[0].rect.x);               // not clipped to 190..200
    EXPECT_EQ(20,  src->calls()[0].rect.width);           // the source owns the overhang
    EXPECT_EQ(20,  src->calls()[0].blockSize.width);
}

TEST(SourceCVSceneLevels, aSourceThatDeclinesTheLevelGetsAClampedSceneRect)
{
    auto src = std::make_shared<TestSceneSource>();
    src->setHasPyramid(false);
    auto scene = sceneOver(src, twoLevelMetadata());

    std::vector<uint8_t> buffer(20 * 20 * 3);
    scene->readResampledLevelBlockChannels(1, std::make_tuple(190, 140, 20, 20),
                                           std::make_tuple(20, 20), {},
                                           buffer.data(), buffer.size());

    ASSERT_EQ(1u, src->calls().size());
    EXPECT_EQ(-1,  src->calls()[0].level);                // came through readBlock
    EXPECT_EQ(380, src->calls()[0].rect.x);               // clipped, then /0.5 to scene
    EXPECT_EQ(20,  src->calls()[0].rect.width);           // 10 level px -> 20 scene px
    EXPECT_EQ(10,  src->calls()[0].blockSize.width);      // only the surviving part

    // CVScene filled the overhang itself: 255 for an 8-bit block.
    const size_t outside = (0 * 20 + 15) * 3;             // x = 15 is past the edge
    EXPECT_EQ(255, buffer[outside]);
}

TEST(SourceCVSceneLevels, bothPathsAgreeAtLevelZero)
{
    SceneMetadata m = twoLevelMetadata();

    auto direct = std::make_shared<TestSceneSource>();
    direct->setHasPyramid(true);
    auto derived = std::make_shared<TestSceneSource>();
    derived->setHasPyramid(false);

    std::vector<uint8_t> a(4 * 4 * 3), b(4 * 4 * 3);
    sceneOver(direct, m)->readResampledLevelBlockChannels(
        0, std::make_tuple(8, 8, 4, 4), std::make_tuple(4, 4), {}, a.data(), a.size());
    sceneOver(derived, m)->readResampledLevelBlockChannels(
        0, std::make_tuple(8, 8, 4, 4), std::make_tuple(4, 4), {}, b.data(), b.size());

    EXPECT_EQ(a, b);
}

TEST(SourceCVSceneLevels, rejectsAnOutOfRangeLevel)
{
    auto src = std::make_shared<TestSceneSource>();
    src->setHasPyramid(true);
    auto scene = sceneOver(src, twoLevelMetadata());

    std::vector<uint8_t> buffer(2 * 2 * 3);
    EXPECT_THROW(scene->readResampledLevelBlockChannels(
                     5, std::make_tuple(0, 0, 2, 2), std::make_tuple(2, 2), {},
                     buffer.data(), buffer.size()),
                 slideio::RuntimeError);
}

TEST(SourceCVSceneLevels, rejectsADegenerateLevelBlock)   // Review Focus 1
{
    auto src = std::make_shared<TestSceneSource>();
    src->setHasPyramid(true);
    auto scene = sceneOver(src, twoLevelMetadata());

    std::vector<uint8_t> buffer(16);
    EXPECT_THROW(scene->readResampledLevelBlockChannels(
                     0, std::make_tuple(0, 0, 4, 4), std::make_tuple(0, 4), {},
                     buffer.data(), buffer.size()),
                 slideio::RuntimeError);
}

TEST(SourceCVSceneLevels, rejectsADegenerateLevelRectBeforeCallingTheSource)   // Review Focus 1
{
    for (bool pyramid : {true, false}) {
        auto src = std::make_shared<TestSceneSource>();
        src->setHasPyramid(pyramid);
        auto scene = sceneOver(src, twoLevelMetadata());
        std::vector<uint8_t> buffer(4 * 4 * 3);
        EXPECT_THROW(scene->readResampledLevelBlockChannels(
                         1, std::make_tuple(0, 0, 0, 4), std::make_tuple(4, 4), {},
                         buffer.data(), buffer.size()),
                     slideio::RuntimeError);
        EXPECT_THROW(scene->readResampledLevelBlockChannels(
                         1, std::make_tuple(0, 0, 4, -2), std::make_tuple(4, 4), {},
                         buffer.data(), buffer.size()),
                     slideio::RuntimeError);
        EXPECT_TRUE(src->calls().empty());
    }
}
