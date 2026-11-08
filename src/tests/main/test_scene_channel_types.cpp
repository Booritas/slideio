// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
//
// A block is one cv::Mat and a cv::Mat carries one depth, so a read whose requested
// channels differ in data type cannot be satisfied. CVScene rejects such a request on
// every read path, before a driver is reached (issue #90).
#include <gtest/gtest.h>
#include <opencv2/core.hpp>
#include <vector>
#include "slideio/core/cvscene.hpp"
#include "slideio/core/exceptions.hpp"
#include "slideio/slideio/scene.hpp"
#include "slideio/slideio/sceneinternal.hpp"

using namespace slideio;

namespace
{
    // A 10x10 scene whose channels 0 and 1 are bytes and channel 2 is float.
    // Its read types the block by channel 0 whatever was requested, as a driver
    // that never consults blockDataType() would: only the base class can catch
    // a mixed request.
    class MixedTypeScene : public CVScene
    {
    public:
        MixedTypeScene()
        {
            m_levels.emplace_back(0, Size(10, 10), 1.0, 20.0, Size(10, 10));
        }
        std::string getFilePath() const override { return "test"; }
        int getSceneIndex() const override { return 0; }
        const std::string& getDriverId() const override { static const std::string id = "test"; return id; }
        std::string getName() const override { return "test"; }
        cv::Rect getRect() const override { return {0, 0, 10, 10}; }
        int getNumChannels() const override { return 3; }
        DataType getChannelDataType(int channel) const override
        {
            return channel == 2 ? DataType::DT_Float32 : DataType::DT_Byte;
        }
        Resolution getResolution() const override { return {0, 0}; }
        double getMagnification() const override { return 0; }
        Compression getCompression() const override { return Compression::Uncompressed; }
        void readResampledBlockChannelsEx(const cv::Rect&, const cv::Size& blockSize,
                                          const std::vector<int>& channels, int, int,
                                          cv::OutputArray output) override
        {
            const int n = channels.empty() ? getNumChannels() : static_cast<int>(channels.size());
            const int depth = channels.empty() || getChannelDataType(channels.front()) == DataType::DT_Byte
                ? CV_8U : CV_32F;
            output.create(blockSize, CV_MAKETYPE(depth, n));
            output.setTo(1);
        }
    };

    const cv::Rect blockRect(0, 0, 10, 10);
    const cv::Size blockSize(5, 5);
    const std::vector<int> mixed{0, 2};
    const std::vector<int> all;
}

TEST(SceneChannelTypes, blockDataTypeRejectsMixedChannels)
{
    MixedTypeScene scene;
    EXPECT_THROW(scene.blockDataType(mixed), RuntimeError);
    EXPECT_THROW(scene.blockDataType(all), RuntimeError);
}

TEST(SceneChannelTypes, blockDataTypeAcceptsChannelsOfOneType)
{
    MixedTypeScene scene;
    EXPECT_EQ(DataType::DT_Byte, scene.blockDataType({1, 0}));
    EXPECT_EQ(DataType::DT_Float32, scene.blockDataType({2, 2}));
}

TEST(SceneChannelTypes, blockDataTypeLeavesOutOfRangeChannelsToTheRead)
{
    // The read rejects an out-of-range channel with its own message; typing the
    // block must not pre-empt it with a type mismatch it has no type for.
    MixedTypeScene scene;
    EXPECT_EQ(DataType::DT_Byte, scene.blockDataType({0, 7}));
    EXPECT_EQ(DataType::DT_Byte, scene.blockDataType({-1}));
}

TEST(SceneChannelTypes, aResampledReadRejectsMixedChannels)
{
    MixedTypeScene scene;
    cv::Mat block;
    EXPECT_THROW(scene.readBlockChannels(blockRect, mixed, block), RuntimeError);
    EXPECT_THROW(scene.readResampledBlockChannels(blockRect, blockSize, mixed, block), RuntimeError);
}

TEST(SceneChannelTypes, aReadOfAllChannelsOfAMixedSceneIsRejected)
{
    MixedTypeScene scene;
    cv::Mat block;
    EXPECT_THROW(scene.readBlock(blockRect, block), RuntimeError);
}

TEST(SceneChannelTypes, a4DReadRejectsMixedChannels)
{
    MixedTypeScene scene;
    cv::Mat block;
    EXPECT_THROW(scene.readResampled4DBlockChannels(blockRect, blockSize, mixed,
                                                    cv::Range(0, 1), cv::Range(0, 1), block),
                 RuntimeError);
}

TEST(SceneChannelTypes, aLevelReadRejectsMixedChannels)
{
    MixedTypeScene scene;
    cv::Mat block;
    EXPECT_THROW(scene.readResampledLevelBlockChannels(0, blockRect, blockSize, mixed, block),
                 RuntimeError);
    EXPECT_THROW(scene.readResampledLevel4DBlockChannels(0, blockRect, blockSize, mixed,
                                                         cv::Range(0, 1), cv::Range(0, 1), block),
                 RuntimeError);
}

TEST(SceneChannelTypes, aReadOfOneTypeSucceeds)
{
    MixedTypeScene scene;
    cv::Mat block;
    scene.readResampledBlockChannels(blockRect, blockSize, {2}, block);
    EXPECT_EQ(CV_32FC1, block.type());
    scene.readResampledBlockChannels(blockRect, blockSize, {0, 1}, block);
    EXPECT_EQ(CV_8UC2, block.type());
}

TEST(SceneChannelTypes, aBufferReadThroughTheSceneRejectsMixedChannels)
{
    auto scene = SceneInternal::createScene(std::make_shared<MixedTypeScene>());
    std::vector<float> buffer(10 * 10 * 2);
    const size_t bufferSize = buffer.size() * sizeof(float);
    const auto rect = std::make_tuple(0, 0, 10, 10);
    EXPECT_THROW(scene->readBlockChannels(rect, mixed, buffer.data(), bufferSize), RuntimeError);
    EXPECT_THROW(scene->readResampledBlockChannels(rect, std::make_tuple(10, 10), mixed,
                                                   buffer.data(), bufferSize),
                 RuntimeError);
    EXPECT_THROW(scene->read4DBlockChannels(rect, mixed, std::make_tuple(0, 1), std::make_tuple(0, 1),
                                            buffer.data(), bufferSize),
                 RuntimeError);
    EXPECT_THROW(scene->readResampledLevelBlockChannels(0, rect, std::make_tuple(10, 10), mixed,
                                                        buffer.data(), bufferSize),
                 RuntimeError);
}
