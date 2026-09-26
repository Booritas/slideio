// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#include <gtest/gtest.h>
#include <memory>
#include "tests/testlib/testscene.hpp"
#include "slideio/core/cvscene.hpp"
#include "slideio/slideio/scene.hpp"
#include "slideio/transformer/transformer.hpp"
#include "slideio/transformer/colortransformation.hpp"

using namespace slideio;

namespace
{
    // An origin whose per-scene metadata is all distinguishable from the CVScene
    // defaults, so a getter that is not forwarded reports 0/false and is caught.
    std::shared_ptr<TestScene> describedOrigin()
    {
        auto origin = std::make_shared<TestScene>();
        origin->setRect(cv::Rect(0, 0, 400, 300));
        origin->setNumChannels(3);
        origin->setChannelDataType(DataType::DT_UInt16);
        origin->setNumZSlices(4);
        origin->setNumTFrames(5);
        origin->setZSliceResolution(2.5e-6);
        origin->setTFrameResolution(0.25);
        origin->setChannelSignificantBits({12, 14, 10});
        origin->setHasPlaneTimestamps(true);
        origin->setAcquisitionTime(1622473135LL);
        return origin;
    }

    std::shared_ptr<Scene> grayTransformOf(const std::shared_ptr<TestScene>& origin)
    {
        ColorTransformation gray;
        gray.setColorSpace(ColorSpace::GRAY);
        return transformScene(std::make_shared<Scene>(origin), gray);
    }
}

TEST(TransformerSceneForwarding, resolutionsComeFromTheOrigin)
{
    auto origin = describedOrigin();
    auto transformed = grayTransformOf(origin);
    EXPECT_DOUBLE_EQ(transformed->getZSliceResolution(), origin->getZSliceResolution());
    EXPECT_DOUBLE_EQ(transformed->getTFrameResolution(), origin->getTFrameResolution());
}

TEST(TransformerSceneForwarding, channelSignificantBitsComeFromTheOrigin)
{
    auto origin = describedOrigin();
    auto transformed = grayTransformOf(origin);
    // Distinct per channel, so forwarding the index matters, not just the call.
    EXPECT_EQ(transformed->getChannelSignificantBits(0), 12);
    EXPECT_EQ(transformed->getChannelSignificantBits(1), 14);
    EXPECT_EQ(transformed->getChannelSignificantBits(2), 10);
}

TEST(TransformerSceneForwarding, planeTimestampsComeFromTheOrigin)
{
    auto origin = describedOrigin();
    auto transformed = grayTransformOf(origin);
    EXPECT_TRUE(transformed->hasPlaneTimestamps());
    // The origin encodes the indices as t*100 + c*10 + z, so a wrapper that
    // reorders or drops an argument reports the wrong number.
    EXPECT_DOUBLE_EQ(transformed->getPlaneTimestamp(3, 2, 1), 321.);
    EXPECT_DOUBLE_EQ(transformed->getPlaneTimestamp(1, 0, 2), 102.);
}

TEST(TransformerSceneForwarding, acquisitionTimeComesFromTheOrigin)
{
    auto origin = describedOrigin();
    auto transformed = grayTransformOf(origin);
    EXPECT_EQ(transformed->getAcquisitionTime(), 1622473135LL);
}
