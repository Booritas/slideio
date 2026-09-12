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
#include "slideio/transformer/colormanagement.hpp"
#include "slideio/imagetools/icctransform.hpp"

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

namespace {
    // GDALScene wraps a raw SmallImagePage* owned by the GDALSlide that created
    // it; it does not keep its parent alive on its own. A Scene handed back
    // from a one-line "openSlide(...)->getScene(0)" helper therefore turns
    // into a use-after-free the moment the Slide temporary is destroyed and a
    // caller later reads actual pixels (readBlock) -- metadata cached at scene
    // construction, like getColorProfile(), happens to survive it, which is
    // why it does not show up until a real pixel read is added. Alias the
    // returned pointer to an owner that holds both the Slide and the Scene, so
    // a copy of the returned shared_ptr keeps both alive for as long as it
    // exists -- the aliasing constructor only shares ownership with the owner
    // it is given, it does not on its own keep `scene`'s original control
    // block alive, so that owner must hold `scene` explicitly too.
    std::shared_ptr<Scene> keepSlideAlive(std::shared_ptr<Slide> slide, std::shared_ptr<Scene> scene)
    {
        auto owner = std::make_shared<std::pair<std::shared_ptr<Slide>, std::shared_ptr<Scene>>>(
            std::move(slide), scene);
        return std::shared_ptr<Scene>(owner, scene.get());
    }

    // img_2448x2448_3x8bit_SRC_RGB_ducks.png carries no ICC profile (confirmed
    // in test_gdal_driver.cpp / test_colorprofile.cpp with `identify -verbose`).
    // Tests whose whole point is the missing-profile policy need this one.
    std::shared_ptr<Scene> openUnprofiledRgbScene()
    {
        std::string path = TestTools::getTestImagePath("gdal", "img_2448x2448_3x8bit_SRC_RGB_ducks.png");
        std::shared_ptr<Slide> slide = openSlide(path, "AUTO");
        return keepSlideAlive(slide, slide->getScene(0));
    }

    // colors.png carries a real 672-byte GIMP sRGB profile, source Embedded.
    std::shared_ptr<Scene> openProfiledRgbScene()
    {
        std::string path = TestTools::getTestImagePath("gdal", "colors.png");
        std::shared_ptr<Slide> slide = openSlide(path, "AUTO");
        return keepSlideAlive(slide, slide->getScene(0));
    }
}

TEST(ColorManagement, defaultsAreTheSafeCase)
{
    ColorManagement cm;
    ASSERT_EQ(ColorTarget::sRGB, cm.getTarget());
    ASSERT_EQ(MissingProfilePolicy::AssumeSRGB, cm.getMissingProfilePolicy());
    ASSERT_EQ(RenderingIntent::RelativeColorimetric, cm.getIntent());
    ASSERT_TRUE(cm.getBlackPointCompensation());
    ASSERT_EQ(TransformationType::ColorManagement, cm.getType());
}

TEST(ColorManagement, labTargetDeclaresFloat32Channels)
{
    ColorManagement cm(ColorTarget::Lab);
    const std::vector<DataType> in{DataType::DT_Byte, DataType::DT_Byte, DataType::DT_Byte};
    const std::vector<DataType> out = cm.computeChannelDataTypes(in);
    ASSERT_EQ(3u, out.size());
    for (DataType type : out) {
        ASSERT_EQ(DataType::DT_Float32, type);
    }
}

TEST(ColorManagement, srgbTargetPreservesChannelDataTypes)
{
    ColorManagement cm(ColorTarget::sRGB);
    const std::vector<DataType> in{DataType::DT_Byte, DataType::DT_Byte, DataType::DT_Byte};
    ASSERT_EQ(in, cm.computeChannelDataTypes(in));
}

TEST(ColorManagement, unprofiledSceneIsReportedAsAssumed)
{
    std::string path = TestTools::getTestImagePath("gdal", "img_2448x2448_3x8bit_SRC_RGB_ducks.png");
    SLIDEIO_SKIP_IF_IMAGE_MISSING(path);
    std::shared_ptr<Scene> scene = openUnprofiledRgbScene();
    ASSERT_TRUE(scene->getColorProfile().isEmpty());

    ColorManagement cm(ColorTarget::Lab);
    std::shared_ptr<Scene> managed = transformScene(scene, cm);
    const ColorProfileInfo info = managed->getColorProfileInfo();
    ASSERT_EQ(ColorProfileSource::Assumed, info.source);
}

// The whole point of the missing-profile audit trail: a scene that genuinely
// embeds a profile must come out the other end reported as Embedded, not
// Assumed -- otherwise a caller cannot tell a real correction from an assumed
// one, which is the distinction the API exists to provide.
TEST(ColorManagement, profiledSceneIsReportedAsEmbedded)
{
    std::string path = TestTools::getTestImagePath("gdal", "colors.png");
    SLIDEIO_SKIP_IF_IMAGE_MISSING(path);
    std::shared_ptr<Scene> scene = openProfiledRgbScene();
    ASSERT_EQ(ColorProfileSource::Embedded, scene->getColorProfile().getSource());

    ColorManagement cm(ColorTarget::Lab);
    std::shared_ptr<Scene> managed = transformScene(scene, cm);
    const ColorProfileInfo info = managed->getColorProfileInfo();
    ASSERT_EQ(ColorProfileSource::Embedded, info.source);
}

TEST(ColorManagement, failPolicyThrowsAtBindTimeWithoutAProfile)
{
    std::string path = TestTools::getTestImagePath("gdal", "img_2448x2448_3x8bit_SRC_RGB_ducks.png");
    SLIDEIO_SKIP_IF_IMAGE_MISSING(path);
    std::shared_ptr<Scene> scene = openUnprofiledRgbScene();
    ColorManagement cm(ColorTarget::Lab);
    cm.setMissingProfilePolicy(MissingProfilePolicy::Fail);
    // Before any pixel moves, so a batch job dies on the file it cannot handle
    // rather than several thousand tiles later.
    ASSERT_THROW(transformScene(scene, cm), RuntimeError);
}

TEST(ColorManagement, passThroughIsRejectedForNonSrgbTargets)
{
    std::string path = TestTools::getTestImagePath("gdal", "img_2448x2448_3x8bit_SRC_RGB_ducks.png");
    SLIDEIO_SKIP_IF_IMAGE_MISSING(path);
    // Must be the unprofiled fixture: PassThrough is a missing-profile policy,
    // so a scene that already carries a real profile never reaches this check
    // and the assertion below would not hold.
    std::shared_ptr<Scene> scene = openUnprofiledRgbScene();
    ColorManagement cm(ColorTarget::Lab);
    cm.setMissingProfilePolicy(MissingProfilePolicy::PassThrough);
    // Otherwise output dtype would depend on whether a file happened to carry a
    // profile, and a batch would assemble tensors of two different dtypes.
    ASSERT_THROW(transformScene(scene, cm), RuntimeError);
}

TEST(ColorManagement, sourceProfileOverrideIsUsed)
{
    std::string path = TestTools::getTestImagePath("gdal", "img_2448x2448_3x8bit_SRC_RGB_ducks.png");
    SLIDEIO_SKIP_IF_IMAGE_MISSING(path);
    // Unprofiled fixture so the override is unambiguously what supplied the
    // profile, rather than shadowing one already embedded in the file.
    std::shared_ptr<Scene> scene = openUnprofiledRgbScene();
    ColorManagement cm(ColorTarget::Lab);
    cm.setMissingProfilePolicy(MissingProfilePolicy::Fail);
    ColorProfile measured = IccTransform::createSRGBProfile();
    measured.setSource(ColorProfileSource::Embedded);
    cm.setSourceProfileOverride(measured);
    // Fail policy no longer applies: an explicit profile was supplied.
    std::shared_ptr<Scene> managed = transformScene(scene, cm);
    ASSERT_EQ(ColorProfileSource::Embedded, managed->getColorProfileInfo().source);
}

// NOTE on falsifiability: because both binds here use the same fixture and
// the same config, this test cannot distinguish an independently-copied bound
// object from one that aliases `cm` itself in a way that merely survives
// (e.g. a bind that returns `this` unchanged) -- both scenes would coincidentally
// read back the same, correct-looking values either way. What it DOES catch,
// verified by mutation: bindToSource aliasing `this` directly instead of
// copying it (the realistic version of "last bind wins") -- that crashes or
// corrupts this test and several others, because `cm` is destroyed or mutated
// out from under a scene still holding a raw alias to it. See colormanagement.cpp
// bindToSource's `std::make_shared<ColorManagement>(*this)`.
TEST(ColorManagement, oneConfigObjectBindsIndependentlyToTwoScenes)
{
    std::string path = TestTools::getTestImagePath("gdal", "img_2448x2448_3x8bit_SRC_RGB_ducks.png");
    SLIDEIO_SKIP_IF_IMAGE_MISSING(path);
    ColorManagement cm(ColorTarget::Lab);
    std::shared_ptr<Scene> first = transformScene(openUnprofiledRgbScene(), cm);
    std::shared_ptr<Scene> second = transformScene(openUnprofiledRgbScene(), cm);
    // The second binding must not have disturbed the first.
    ASSERT_EQ(ColorProfileSource::Assumed, first->getColorProfileInfo().source);
    ASSERT_EQ(ColorProfileSource::Assumed, second->getColorProfileInfo().source);
    ASSERT_EQ(DataType::DT_Float32, first->getChannelDataType(0));
}

TEST(ColorManagement, labBlockIsFloatAndPlausible)
{
    std::string path = TestTools::getTestImagePath("gdal", "colors.png");
    SLIDEIO_SKIP_IF_IMAGE_MISSING(path);
    // The origin scene is kept in a named local for the whole test: the
    // readBlock() below needs its underlying image decode session, which a
    // temporary origin scene would have already released by then.
    std::shared_ptr<Scene> origin = openProfiledRgbScene();
    ColorManagement cm(ColorTarget::Lab);
    std::shared_ptr<Scene> managed = transformScene(origin, cm);
    auto rect = managed->getRect();
    const int width = std::get<2>(rect);
    const int height = std::get<3>(rect);
    std::vector<float> buffer(static_cast<size_t>(width) * height * 3);
    managed->readBlock(rect, buffer.data(), buffer.size() * sizeof(float));
    for (size_t i = 0; i < buffer.size(); i += 3) {
        ASSERT_GE(buffer[i], -0.5f);      // L in [0,100]
        ASSERT_LE(buffer[i], 100.5f);
    }
}

// ColorManagement must refuse a scene whose channels are not colorimetric RGB
// before any pixel moves. 08_18_2018_enc_1001_633.czi is a 6-channel
// fluorescence acquisition (channel names are emission wavelengths, e.g.
// "646", "655" -- see CZIImageDriver.openFileInfo in test_czi_driver.cpp):
// intensity readings with no RGB interpretation, so ICC conversion of them is
// not meaningful and must be rejected at bind time.
TEST(ColorManagement, nonRgbChannelCountIsRejectedAtBindTime)
{
    std::string path = TestTools::getTestImagePath("czi", "08_18_2018_enc_1001_633.czi");
    SLIDEIO_SKIP_IF_IMAGE_MISSING(path);
    // Kept alive in a named local: CZIScene keeps only a raw CZISlide* back to
    // its parent (cziscene.hpp), so a Slide dropped as a temporary here would
    // be a use-after-free the moment the scene is queried.
    std::shared_ptr<Slide> slide = openSlide(path, "AUTO");
    std::shared_ptr<Scene> scene = slide->getScene(0);
    ASSERT_EQ(6, scene->getNumChannels());
    ColorManagement cm;
    ASSERT_THROW(transformScene(scene, cm), RuntimeError);
}
