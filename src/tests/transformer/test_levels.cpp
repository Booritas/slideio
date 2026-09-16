// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#include <gtest/gtest.h>
#include <memory>
#include <opencv2/imgproc.hpp>
#include "tests/testlib/testscene.hpp"
#include "slideio/core/cvscene.hpp"
#include "slideio/core/levelinfo.hpp"
#include "slideio/slideio/scene.hpp"
#include "slideio/transformer/transformer.hpp"
#include "slideio/transformer/colortransformation.hpp"
#include "slideio/transformer/gaussianblurfilter.hpp"

using namespace slideio;

namespace
{
    // A three-level pyramid over the TestScene fake. Nothing here needs a test
    // image: TestScene renders each pixel from the scene coordinate it came
    // from, so a level read has a checkable value at every position.
    std::shared_ptr<TestScene> pyramidOrigin()
    {
        auto origin = std::make_shared<TestScene>();
        origin->setRect(cv::Rect(0, 0, 400, 300));
        origin->setNumChannels(3);
        origin->setChannelDataType(DataType::DT_Byte);
        origin->setRenderCoordinates(true);
        origin->addLevel(LevelInfo(0, {400, 300}, 1.0, 20.0, {256, 256}));
        origin->addLevel(LevelInfo(1, {200, 150}, 0.5, 10.0, {256, 256}));
        origin->addLevel(LevelInfo(2, {100, 75}, 0.25, 5.0, {256, 256}));
        return origin;
    }

    std::shared_ptr<Scene> grayTransformOf(const std::shared_ptr<Scene>& origin)
    {
        ColorTransformation gray;
        gray.setColorSpace(ColorSpace::GRAY);
        return transformScene(origin, gray);
    }
}

TEST(TransformerSceneLevels, aTransformedSceneReportsItsOriginsPyramid)
{
    auto origin = pyramidOrigin();
    auto originScene = std::make_shared<Scene>(origin);
    auto transformed = grayTransformOf(originScene);
    std::shared_ptr<CVScene> cvTransformed = transformed->getCVScene();

    ASSERT_EQ(cvTransformed->getNumZoomLevels(), origin->getNumZoomLevels());
    for (int level = 0; level < origin->getNumZoomLevels(); ++level) {
        const LevelInfo* mine = cvTransformed->getZoomLevelInfo(level);
        const LevelInfo* theirs = origin->getZoomLevelInfo(level);
        EXPECT_EQ(mine->getSize().width, theirs->getSize().width) << "level " << level;
        EXPECT_EQ(mine->getSize().height, theirs->getSize().height) << "level " << level;
        EXPECT_DOUBLE_EQ(mine->getScale(), theirs->getScale()) << "level " << level;
        EXPECT_DOUBLE_EQ(mine->getMagnification(), theirs->getMagnification()) << "level " << level;
    }
}

TEST(TransformerSceneLevels, aLevelReadAppliesTheTransformToThatLevelsRaster)
{
    auto origin = pyramidOrigin();
    auto originScene = std::make_shared<Scene>(origin);
    auto transformed = grayTransformOf(originScene);
    std::shared_ptr<CVScene> cvTransformed = transformed->getCVScene();

    // Read the whole of level 1 from both. The transform is a colour
    // conversion with no kernel, so the transformed result must be exactly the
    // grey conversion of the origin's own level raster -- which is what pins
    // that the transform ran at the level's resolution rather than at level 0.
    const int level = 1;
    const cv::Rect levelRect(0, 0, 200, 150);
    const cv::Size blockSize(200, 150);

    cv::Mat originRaster;
    origin->readResampledLevelBlockChannels(level, levelRect, blockSize, {}, originRaster);

    cv::Mat transformedRaster;
    cvTransformed->readResampledLevelBlockChannels(level, levelRect, blockSize, {}, transformedRaster);

    ASSERT_EQ(transformedRaster.size(), blockSize);
    ASSERT_EQ(transformedRaster.channels(), 1);

    cv::Mat expected;
    cv::cvtColor(originRaster, expected, cv::COLOR_RGB2GRAY);
    ASSERT_EQ(expected.size(), transformedRaster.size());
    EXPECT_EQ(cv::countNonZero(expected != transformedRaster), 0);
}

TEST(TransformerSceneLevels, aLevelReadIsTakenFromTheRequestedLevelNotResampledFromAnother)
{
    // The whole point of the level api is that it addresses one named level.
    // TestScene records the block requests it served, so this asserts the
    // origin was asked at the requested level's geometry rather than at some
    // scale the origin then had to choose a level for.
    auto origin = pyramidOrigin();
    auto originScene = std::make_shared<Scene>(origin);
    auto transformed = grayTransformOf(originScene);
    std::shared_ptr<CVScene> cvTransformed = transformed->getCVScene();

    origin->clearRequests();
    cv::Mat raster;
    cvTransformed->readResampledLevelBlockChannels(2, cv::Rect(0, 0, 100, 75),
                                                  cv::Size(100, 75), {}, raster);

    ASSERT_FALSE(origin->requests().empty());
    // Level 2 is a quarter scale, so a full-level request reaches the origin as
    // the whole scene rect rendered into a 100x75 block.
    const auto& first = origin->requests().front();
    EXPECT_EQ(first.size.width, 100);
    EXPECT_EQ(first.size.height, 75);
    EXPECT_EQ(first.rect.width, 400);
    EXPECT_EQ(first.rect.height, 300);
}

TEST(TransformerSceneLevels, aLevelReadAgreesWithTheEquivalentScaledRead)
{
    // The semantic choice, asserted rather than only described: a
    // transformation's kernel is in the pixels of the resolution being read.
    // Level 1 is half scale, so reading the whole of level 1 must give the same
    // raster as reading the whole scene scaled to half -- which holds only if
    // the blur ran at the output resolution in both cases. Were the kernel
    // defined in level-0 pixels these two would differ.
    auto origin = pyramidOrigin();
    auto originScene = std::make_shared<Scene>(origin);

    GaussianBlurFilter blur;
    blur.setKernelSizeX(5);
    blur.setKernelSizeY(5);
    std::shared_ptr<Scene> transformed = transformScene(originScene, blur);
    std::shared_ptr<CVScene> cvTransformed = transformed->getCVScene();

    cv::Mat viaLevel;
    cvTransformed->readResampledLevelBlockChannels(1, cv::Rect(0, 0, 200, 150),
                                                   cv::Size(200, 150), {}, viaLevel);

    cv::Mat viaScale;
    cvTransformed->readResampledBlockChannels(cv::Rect(0, 0, 400, 300),
                                              cv::Size(200, 150), {}, viaScale);

    ASSERT_EQ(viaLevel.size(), viaScale.size());
    ASSERT_EQ(viaLevel.type(), viaScale.type());
    EXPECT_EQ(cv::countNonZero(viaLevel.reshape(1) != viaScale.reshape(1)), 0);
}

// ---------------------------------------------------------------------------
// Rectangles that fall partly or wholly outside the level.
//
// CVScene::readResampledLevelBlockChannelsEx promises a background-filled block
// for the part of the request the level does not cover, and it promises not to
// throw on a degenerate rectangle. An override has to keep both promises; the
// first version of this one cropped the transformed block with the *requested*
// rectangle rather than the clipped one, which is an invalid ROI the moment any
// of the request lies outside the level.
//
// Each case compares against the origin read the same way and converted by
// hand. That works for the background too: the background is 255 in every
// channel, and grey of white is white, so a correct implementation agrees with
// the origin everywhere rather than only inside the valid region.
// ---------------------------------------------------------------------------

namespace
{
    void expectMatchesOriginThroughGrey(int level, const cv::Rect& levelRect, const cv::Size& blockSize)
    {
        auto origin = pyramidOrigin();
        auto originScene = std::make_shared<Scene>(origin);
        auto transformed = grayTransformOf(originScene);
        std::shared_ptr<CVScene> cvTransformed = transformed->getCVScene();

        cv::Mat raster;
        ASSERT_NO_THROW(
            cvTransformed->readResampledLevelBlockChannels(level, levelRect, blockSize, {}, raster));
        ASSERT_EQ(raster.size(), blockSize);
        ASSERT_EQ(raster.channels(), 1);

        cv::Mat originRaster;
        origin->readResampledLevelBlockChannels(level, levelRect, blockSize, {}, originRaster);
        cv::Mat expected;
        cv::cvtColor(originRaster, expected, cv::COLOR_RGB2GRAY);

        ASSERT_EQ(expected.size(), raster.size());
        EXPECT_EQ(cv::countNonZero(expected != raster), 0);
    }
}

TEST(TransformerSceneLevels, aLevelRectStartingLeftOfTheLevelIsBackgroundFilled)
{
    expectMatchesOriginThroughGrey(1, cv::Rect(-10, 0, 100, 100), cv::Size(100, 100));
}

TEST(TransformerSceneLevels, aLevelRectStartingAboveTheLevelIsBackgroundFilled)
{
    expectMatchesOriginThroughGrey(1, cv::Rect(0, -20, 100, 100), cv::Size(100, 100));
}

TEST(TransformerSceneLevels, aLevelRectRunningPastTheRightAndBottomIsBackgroundFilled)
{
    // Level 1 is 200x150, so this overruns both edges.
    expectMatchesOriginThroughGrey(1, cv::Rect(150, 100, 100, 100), cv::Size(100, 100));
}

TEST(TransformerSceneLevels, aLevelRectLargerThanTheLevelOnEverySideIsBackgroundFilled)
{
    expectMatchesOriginThroughGrey(1, cv::Rect(-50, -50, 400, 400), cv::Size(200, 200));
}

TEST(TransformerSceneLevels, anEmptyLevelRectReturnsBackgroundRatherThanThrowing)
{
    // Zero width and zero height both reach the scale division in
    // computeInflatedRectParams, which is why the guard has to come first.
    expectMatchesOriginThroughGrey(1, cv::Rect(0, 0, 0, 100), cv::Size(100, 100));
    expectMatchesOriginThroughGrey(1, cv::Rect(0, 0, 100, 0), cv::Size(100, 100));
}

TEST(TransformerSceneLevels, aLevelRectEntirelyOutsideTheLevelIsAllBackground)
{
    expectMatchesOriginThroughGrey(1, cv::Rect(1000, 1000, 50, 50), cv::Size(50, 50));
}

TEST(TransformerSceneLevels, anOutOfBoundsLevelRectIsSafeForAKernelTransformToo)
{
    // The cases above use a colour conversion, whose inflation value is zero.
    // A filter with a kernel inflates the rectangle before reading, so the
    // clipping and the halo interact; this is the case where getting the order
    // wrong is easiest.
    auto origin = pyramidOrigin();
    auto originScene = std::make_shared<Scene>(origin);
    GaussianBlurFilter blur;
    blur.setKernelSizeX(9);
    blur.setKernelSizeY(9);
    std::shared_ptr<Scene> transformed = transformScene(originScene, blur);
    std::shared_ptr<CVScene> cvTransformed = transformed->getCVScene();

    const cv::Rect cases[] = {
        cv::Rect(-20, -20, 100, 100),      // off the top left
        cv::Rect(170, 120, 100, 100),      // past the right and bottom
        cv::Rect(-50, -50, 400, 400),      // larger than the level every way
        cv::Rect(1000, 1000, 40, 40),      // entirely outside
    };
    for (const cv::Rect& levelRect : cases) {
        cv::Mat raster;
        ASSERT_NO_THROW(
            cvTransformed->readResampledLevelBlockChannels(1, levelRect, cv::Size(100, 100), {}, raster))
            << "levelRect " << levelRect;
        EXPECT_EQ(raster.size(), cv::Size(100, 100)) << "levelRect " << levelRect;
        EXPECT_EQ(raster.channels(), 3) << "levelRect " << levelRect;
    }
}
