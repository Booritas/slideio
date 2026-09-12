#include <gtest/gtest.h>
#include "slideio/core/colorprofile.hpp"

using namespace slideio;

TEST(ColorProfile, defaultIsAbsent)
{
    ColorProfile profile;
    ASSERT_TRUE(profile.isEmpty());
    ASSERT_EQ(0u, profile.getSize());
    ASSERT_EQ(ColorProfileSource::None, profile.getSource());
    ASSERT_TRUE(profile.getData().empty());
}

TEST(ColorProfile, constructedFromBytesIsEmbedded)
{
    std::vector<uint8_t> bytes{1, 2, 3, 4};
    ColorProfile profile(bytes);
    ASSERT_FALSE(profile.isEmpty());
    ASSERT_EQ(4u, profile.getSize());
    ASSERT_EQ(ColorProfileSource::Embedded, profile.getSource());
    ASSERT_EQ(bytes, profile.getData());
}

TEST(ColorProfile, emptyByteVectorStaysAbsent)
{
    // A driver that finds a zero-length tag must not claim a profile exists.
    ColorProfile profile(std::vector<uint8_t>{});
    ASSERT_TRUE(profile.isEmpty());
    ASSERT_EQ(ColorProfileSource::None, profile.getSource());
}

TEST(ColorProfile, sourceIsSettable)
{
    ColorProfile profile(std::vector<uint8_t>{1, 2, 3});
    profile.setSource(ColorProfileSource::Assumed);
    ASSERT_EQ(ColorProfileSource::Assumed, profile.getSource());
}

TEST(ColorProfileInfo, defaultIsNotPresent)
{
    ColorProfileInfo info;
    ASSERT_FALSE(info.present);
    ASSERT_EQ(ColorProfileSource::None, info.source);
    ASSERT_EQ(IccColorSpace::Unknown, info.dataSpace);
    ASSERT_EQ(0u, info.dataSize);
}

TEST(ColorProfileInfo, toStringNamesPresenceAndDescription)
{
    ColorProfileInfo info;
    info.present = true;
    info.source = ColorProfileSource::Embedded;
    info.description = "Aperio RGB";
    info.dataSpace = IccColorSpace::RGB;
    info.dataSize = 3144;
    const std::string text = info.toString();
    ASSERT_NE(std::string::npos, text.find("Aperio RGB"));
    ASSERT_NE(std::string::npos, text.find("Embedded"));
    ASSERT_NE(std::string::npos, text.find("3144"));
}

#include "slideio/slideio/slideio.hpp"
#include "slideio/slideio/scene.hpp"
#include "tests/testlib/testtools.hpp"

TEST(ColorProfile, sceneWithoutProfileReportsAbsent)
{
    // colors.png, this file's original fixture, turns out to carry a real
    // embedded ICC profile (see ColorProfile.gdalPngSceneReportsItsEmbeddedIcc
    // below) -- img_2448x2448_3x8bit_SRC_RGB_ducks.png is the genuinely
    // profile-less PNG in this corpus, confirmed with `identify -verbose`.
    std::string path = TestTools::getTestImagePath("gdal", "img_2448x2448_3x8bit_SRC_RGB_ducks.png");
    SLIDEIO_SKIP_IF_IMAGE_MISSING(path);
    std::shared_ptr<slideio::Slide> slide = slideio::openSlide(path, "AUTO");
    std::shared_ptr<slideio::Scene> scene = slide->getScene(0);
    const ColorProfile profile = scene->getColorProfile();
    ASSERT_TRUE(profile.isEmpty());
    ASSERT_EQ(ColorProfileSource::None, profile.getSource());
}

TEST(ColorProfile, sceneWithoutProfileReportsInfoAbsent)
{
    std::string path = TestTools::getTestImagePath("gdal", "img_2448x2448_3x8bit_SRC_RGB_ducks.png");
    SLIDEIO_SKIP_IF_IMAGE_MISSING(path);
    std::shared_ptr<slideio::Slide> slide = slideio::openSlide(path, "AUTO");
    std::shared_ptr<slideio::Scene> scene = slide->getScene(0);
    const ColorProfileInfo info = scene->getColorProfileInfo();
    ASSERT_FALSE(info.present);
    ASSERT_EQ(ColorProfileSource::None, info.source);
    ASSERT_NE(std::string::npos, info.toString().find("present=false"));
}

// colors.png carries a real 672-byte GIMP-built sRGB ICC profile (confirmed
// with `identify -verbose`: Profile-icc, "GIMP built-in sRGB"). Now that the
// gdal driver reads it (GDALScene::getColorProfile(), via FreeImage's
// FreeImage_GetICCProfile), this is an end-to-end assertion of the whole
// chain: FreeImage -> GDALScene -> Scene::getColorProfileInfo().
TEST(ColorProfile, gdalPngSceneReportsItsEmbeddedIcc)
{
    std::string path = TestTools::getTestImagePath("gdal", "colors.png");
    SLIDEIO_SKIP_IF_IMAGE_MISSING(path);
    std::shared_ptr<slideio::Slide> slide = slideio::openSlide(path, "AUTO");
    std::shared_ptr<slideio::Scene> scene = slide->getScene(0);
    const ColorProfile profile = scene->getColorProfile();
    ASSERT_FALSE(profile.isEmpty());
    ASSERT_EQ(672u, profile.getSize());
    ASSERT_EQ(ColorProfileSource::Embedded, profile.getSource());
    const ColorProfileInfo info = scene->getColorProfileInfo();
    ASSERT_TRUE(info.present);
    ASSERT_EQ(IccColorSpace::RGB, info.dataSpace);
}
