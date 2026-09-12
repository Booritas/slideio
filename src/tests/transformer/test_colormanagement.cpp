// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#include <gtest/gtest.h>
#include "tests/testlib/testtools.hpp"
#include "slideio/slideio/slideio.hpp"
#include "slideio/slideio/scene.hpp"
#include "slideio/core/cvscene.hpp"
#include "slideio/core/exceptions.hpp"
#include "slideio/transformer/transformer.hpp"
#include "slideio/transformer/gaussianblurfilter.hpp"

using namespace slideio;

TEST(TransformationBinding, existingFiltersNeedNoBinding)
{
    // The seven existing filters must be entirely unaffected by the new hooks.
    // A real CVScene is used (rather than a dereferenced null pointer) to prove
    // the default bindToSource() genuinely does not touch its argument, without
    // invoking undefined behaviour to do so.
    std::string path = TestTools::getTestImagePath("gdal", "img_2448x2448_3x8bit_SRC_RGB_ducks.png");
    SLIDEIO_SKIP_IF_IMAGE_MISSING(path);
    std::shared_ptr<Slide> slide = openSlide(path, "AUTO");
    std::shared_ptr<Scene> scene = slide->getScene(0);
    std::shared_ptr<CVScene> cvScene = scene->getCVScene();
    GaussianBlurFilter filter;
    ASSERT_EQ(nullptr, filter.bindToSource(*cvScene).get());
}

TEST(TransformationBinding, existingFiltersLeaveTheProfileAlone)
{
    GaussianBlurFilter filter;
    ColorProfile profile(std::vector<uint8_t>{1, 2, 3});
    const ColorProfile amended = filter.amendColorProfile(profile);
    ASSERT_EQ(profile.getData(), amended.getData());
    ASSERT_EQ(profile.getSource(), amended.getSource());
}

TEST(TransformationBinding, transformedSceneForwardsTheOriginProfile)
{
    // colors.png carries a real 672-byte GIMP sRGB profile (source Embedded).
    // Asserting the actual size rather than just equality of two numbers makes
    // this falsifiable: an unimplemented TransformerScene::getColorProfile()
    // would return the CVScene default of size 0 on both sides, which would
    // pass a bare equality check without proving anything was forwarded.
    std::string path = TestTools::getTestImagePath("gdal", "colors.png");
    SLIDEIO_SKIP_IF_IMAGE_MISSING(path);
    std::shared_ptr<Slide> slide = openSlide(path, "AUTO");
    std::shared_ptr<Scene> originScene = slide->getScene(0);
    GaussianBlurFilter filter;
    std::shared_ptr<Scene> transformed = transformScene(originScene, filter);
    // A filter that does not touch colour must not change what the scene
    // reports about its colorimetry.
    ASSERT_EQ(static_cast<size_t>(672), originScene->getColorProfile().getSize());
    ASSERT_EQ(originScene->getColorProfile().getSize(),
              transformed->getColorProfile().getSize());
}
