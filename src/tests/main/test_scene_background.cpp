// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
//
// CVScene::getChannelBackground: the value an uncovered pixel of a channel takes.
// The base class fills blocks with it in initializeSceneBlock and in readClampedBlock;
// a driver overrides it to state its own background (MRXS: IMAGE_FILL_COLOR_BGR).
#include <gtest/gtest.h>
#include <opencv2/core.hpp>
#include <vector>
#include "slideio/core/cvscene.hpp"

using namespace slideio;

namespace
{
    // A 10x10 scene with three channels of one data type. Pixels it does have are 1.
    class BackgroundScene : public CVScene
    {
    public:
        explicit BackgroundScene(DataType dt = DataType::DT_Byte) : m_dt(dt) {}
        std::string getFilePath() const override { return "test"; }
        int getSceneIndex() const override { return 0; }
        const std::string& getDriverId() const override { static const std::string id = "test"; return id; }
        std::string getName() const override { return "test"; }
        cv::Rect getRect() const override { return {0, 0, 10, 10}; }
        int getNumChannels() const override { return 3; }
        DataType getChannelDataType(int) const override { return m_dt; }
        Resolution getResolution() const override { return {0, 0}; }
        double getMagnification() const override { return 0; }
        Compression getCompression() const override { return Compression::Uncompressed; }
        void readResampledBlockChannelsEx(const cv::Rect&, const cv::Size&, const std::vector<int>&,
                                          int, int, cv::OutputArray) override {}

        // The two protected fill paths, reachable from the tests.
        void clampedRead(const cv::Rect& rect, const cv::Size& size, const std::vector<int>& channels,
                         cv::OutputArray output) const
        {
            readClampedBlock(getRect(), rect, size, channels,
                [&](const cv::Rect&, const cv::Size& partSize, cv::OutputArray part) {
                    const int n = channels.empty() ? getNumChannels() : static_cast<int>(channels.size());
                    part.create(partSize, CV_MAKETYPE(CV_8U, n));
                    part.setTo(1);
                },
                output);
        }
        void initializedBlock(const cv::Size& size, const std::vector<int>& channels, cv::OutputArray output) const
        {
            initializeSceneBlock(size, channels, output);
        }
    private:
        DataType m_dt;
    };

    // Overrides the hook with a distinct value per channel.
    class ColouredScene : public BackgroundScene
    {
    public:
        double getChannelBackground(int channel) const override
        {
            static const double values[] = {10, 20, 30};
            return values[channel];
        }
    };

    // Six channels: more than a cv::Scalar can carry.
    class WideScene : public BackgroundScene
    {
    public:
        int getNumChannels() const override { return 6; }
        double getChannelBackground(int channel) const override { return 10. * (channel + 1); }
    };

    std::vector<cv::Mat> planes(const cv::Mat& block)
    {
        std::vector<cv::Mat> out;
        cv::split(block, out);
        return out;
    }

    // Every pixel of plane equals value.
    ::testing::AssertionResult filledWith(const cv::Mat& plane, double value)
    {
        double lo = 0, hi = 0;
        cv::minMaxLoc(plane, &lo, &hi);
        if (lo == value && hi == value) {
            return ::testing::AssertionSuccess();
        }
        return ::testing::AssertionFailure() << "plane spans [" << lo << ", " << hi << "], expected " << value;
    }
}

TEST(SceneBackground, defaultIs255ForByteChannels)
{
    BackgroundScene scene(DataType::DT_Byte);
    for (int channel = 0; channel < scene.getNumChannels(); ++channel) {
        EXPECT_EQ(255., scene.getChannelBackground(channel));
    }
}

TEST(SceneBackground, defaultIsZeroForNonByteChannels)
{
    for (DataType dt : {DataType::DT_Int16, DataType::DT_UInt16, DataType::DT_Float32}) {
        BackgroundScene scene(dt);
        EXPECT_EQ(0., scene.getChannelBackground(0));
    }
}

TEST(SceneBackground, initializedBlockTakesEachChannelsBackground)
{
    ColouredScene scene;
    cv::Mat block;
    scene.initializedBlock(cv::Size(4, 3), {0, 1, 2}, block);
    ASSERT_EQ(CV_8UC3, block.type());
    const auto p = planes(block);
    EXPECT_TRUE(filledWith(p[0], 10));
    EXPECT_TRUE(filledWith(p[1], 20));
    EXPECT_TRUE(filledWith(p[2], 30));
}

TEST(SceneBackground, initializedBlockFollowsTheCallersChannelOrder)
{
    ColouredScene scene;
    cv::Mat block;
    scene.initializedBlock(cv::Size(4, 3), {2, 0}, block);
    ASSERT_EQ(CV_8UC2, block.type());
    const auto p = planes(block);
    EXPECT_TRUE(filledWith(p[0], 30));
    EXPECT_TRUE(filledWith(p[1], 10));
}

TEST(SceneBackground, emptyChannelSelectionMeansEveryChannelInOrder)
{
    ColouredScene scene;
    cv::Mat block;
    scene.initializedBlock(cv::Size(4, 3), {}, block);
    ASSERT_EQ(CV_8UC3, block.type());
    const auto p = planes(block);
    EXPECT_TRUE(filledWith(p[0], 10));
    EXPECT_TRUE(filledWith(p[1], 20));
    EXPECT_TRUE(filledWith(p[2], 30));
}

TEST(SceneBackground, aBlockWiderThanFourChannelsTakesEachChannelsBackground)
{
    WideScene scene;
    cv::Mat block;
    scene.initializedBlock(cv::Size(4, 3), {5, 4, 3, 2, 1, 0}, block);
    ASSERT_EQ(CV_8UC(6), block.type());
    const auto p = planes(block);
    for (int plane = 0; plane < 6; ++plane) {
        EXPECT_TRUE(filledWith(p[plane], 10. * (6 - plane))) << "plane " << plane;
    }
}

TEST(SceneBackground, aWideBlockWithOneBackgroundForAllChannelsIsFilled)
{
    // The uniform fast path, which every existing driver takes, on more channels
    // than a cv::Scalar holds: multichannel QPTIFF and CZI scenes read this way.
    class WideDefaultScene : public BackgroundScene
    {
    public:
        int getNumChannels() const override { return 6; }
    };
    WideDefaultScene scene;
    cv::Mat block;
    scene.initializedBlock(cv::Size(4, 3), {}, block);
    ASSERT_EQ(CV_8UC(6), block.type());
    for (const cv::Mat& plane : planes(block)) {
        EXPECT_TRUE(filledWith(plane, 255));
    }
}

TEST(SceneBackground, aClampedReadEntirelyOutsideTheSceneIsBackground)
{
    ColouredScene scene;
    cv::Mat block;
    scene.clampedRead(cv::Rect(50, 50, 4, 4), cv::Size(4, 4), {0, 1, 2}, block);
    ASSERT_EQ(CV_8UC3, block.type());
    const auto p = planes(block);
    EXPECT_TRUE(filledWith(p[0], 10));
    EXPECT_TRUE(filledWith(p[1], 20));
    EXPECT_TRUE(filledWith(p[2], 30));
}

TEST(SceneBackground, aPartlyOutsideClampedReadFillsOnlyTheUncoveredPart)
{
    // Columns 8..9 are inside the 10-wide scene, columns 10..11 are not.
    ColouredScene scene;
    cv::Mat block;
    scene.clampedRead(cv::Rect(8, 0, 4, 4), cv::Size(4, 4), {2, 0}, block);
    ASSERT_EQ(CV_8UC2, block.type());
    const auto p = planes(block);
    EXPECT_TRUE(filledWith(p[0](cv::Rect(0, 0, 2, 4)), 1));
    EXPECT_TRUE(filledWith(p[1](cv::Rect(0, 0, 2, 4)), 1));
    EXPECT_TRUE(filledWith(p[0](cv::Rect(2, 0, 2, 4)), 30));
    EXPECT_TRUE(filledWith(p[1](cv::Rect(2, 0, 2, 4)), 10));
}

TEST(SceneBackground, aSceneWithoutTheOverrideKeepsTodaysEdgeFill)
{
    BackgroundScene scene;
    cv::Mat block;
    scene.clampedRead(cv::Rect(8, 0, 4, 4), cv::Size(4, 4), {0, 1, 2}, block);
    for (const cv::Mat& plane : planes(block)) {
        EXPECT_TRUE(filledWith(plane(cv::Rect(2, 0, 2, 4)), 255));
    }
}
