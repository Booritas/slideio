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
