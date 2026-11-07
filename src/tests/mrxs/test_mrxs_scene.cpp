// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
//
// Scene properties, metadata and associated images (design section 6), and the lifetime
// rules of section 3, on the committed fixtures.
#include <gtest/gtest.h>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <nlohmann/json.hpp>
#include <string>
#include <vector>
#include <opencv2/core.hpp>
#include "slideio/drivers/mrxs/mrxsimagedriver.hpp"
#include "slideio/core/cvscene.hpp"
#include "slideio/core/cvslide.hpp"
#include "slideio/core/levelinfo.hpp"
#include "slideio/core/metadata.hpp"

namespace fs = std::filesystem;
using json = nlohmann::json;

namespace
{
    const fs::path dataDir() { return fs::path(MRXS_TEST_DATA_DIR); }

    json expectedOf(const std::string& name)
    {
        std::ifstream in(dataDir() / (name + ".expected.json"));
        return json::parse(in);
    }

    std::shared_ptr<slideio::CVSlide> slideOf(const std::string& name)
    {
        slideio::MRXSImageDriver driver;
        return driver.openFile((dataDir() / (name + ".mrxs")).u8string());
    }

    std::string fileText(const fs::path& path)
    {
        std::ifstream in(path, std::ios::binary);
        return std::string(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
    }
}

TEST(MRXSScene, describesItselfAsOneRgbScene)
{
    const auto slide = slideOf("nomap-d1");
    ASSERT_EQ(1, slide->getNumScenes());
    const auto scene = slide->getScene(0);
    const json expected = expectedOf("nomap-d1");
    EXPECT_EQ("nomap-d1", scene->getName());
    EXPECT_EQ((dataDir() / "nomap-d1.mrxs").u8string(), scene->getFilePath());
    EXPECT_EQ("MRXS", scene->getDriverId());
    EXPECT_EQ(0, scene->getSceneIndex());
    EXPECT_EQ(cv::Rect(0, 0, expected["levels"][0]["width"].get<int>(), expected["levels"][0]["height"].get<int>()),
              scene->getRect());
    ASSERT_EQ(3, scene->getNumChannels());
    EXPECT_EQ("Red", scene->getChannelName(0));
    EXPECT_EQ("Green", scene->getChannelName(1));
    EXPECT_EQ("Blue", scene->getChannelName(2));
    for (int c = 0; c < 3; ++c) {
        EXPECT_EQ(slideio::DataType::DT_Byte, scene->getChannelDataType(c));
    }
    EXPECT_EQ(1, scene->getNumZSlices());
    EXPECT_EQ(1, scene->getNumTFrames());
}

TEST(MRXSScene, resolutionIsLevelZeroMppInMetresPerAxis)
{
    // nomap-d1 has anisotropic pixels: 0.25 x 0.26 micrometres.
    const auto scene = slideOf("nomap-d1")->getScene(0);
    EXPECT_DOUBLE_EQ(0.25e-6, scene->getResolution().x);
    EXPECT_DOUBLE_EQ(0.26e-6, scene->getResolution().y);
}

TEST(MRXSScene, aSavedSlidesResolutionIsNotScaledByC0Again)
{
    // saved-c0-4 stores level 0 at C0 = 4; its INI already gives that level's pixel size.
    const auto scene = slideOf("saved-c0-4")->getScene(0);
    EXPECT_DOUBLE_EQ(1.0e-6, scene->getResolution().x);
    EXPECT_DOUBLE_EQ(1.0e-6, scene->getResolution().y);
}

TEST(MRXSScene, zoomLevelsCarrySizeScaleMagnificationAndALogicalTile)
{
    for (const std::string name : {"rawmap-d2", "saved-c0-4"}) {
        SCOPED_TRACE(name);
        const auto scene = slideOf(name)->getScene(0);
        const json expected = expectedOf(name);
        ASSERT_EQ(static_cast<int>(expected["levels"].size()), scene->getNumZoomLevels());
        EXPECT_DOUBLE_EQ(20.0, scene->getMagnification());
        for (int l = 0; l < scene->getNumZoomLevels(); ++l) {
            const slideio::LevelInfo* info = scene->getZoomLevelInfo(l);
            const json& level = expected["levels"][l];
            const double d = level["downsample"].get<double>();
            EXPECT_EQ(level["width"].get<int>(), info->getSize().width);
            EXPECT_EQ(level["height"].get<int>(), info->getSize().height);
            EXPECT_DOUBLE_EQ(1.0 / d, info->getScale());
            EXPECT_DOUBLE_EQ(20.0 / d, info->getMagnification());
            EXPECT_EQ(256, info->getTileSize().width);
            EXPECT_EQ(256, info->getTileSize().height);
        }
    }
}

TEST(MRXSScene, compressionIsTheCodecOfEveryLevelOrUnknown)
{
    EXPECT_EQ(slideio::Compression::Png, slideOf("nomap-d1")->getScene(0)->getCompression());
    EXPECT_EQ(slideio::Compression::Jpeg, slideOf("zlibmap-d2-jpeg")->getScene(0)->getCompression());
    EXPECT_EQ(slideio::Compression::BMP, slideOf("bmp24")->getScene(0)->getCompression());
    EXPECT_EQ(slideio::Compression::Unknown, slideOf("mixed-codecs")->getScene(0)->getCompression());
}

TEST(MRXSScene, rawMetadataIsTheIniExactlyAsWritten)
{
    // layout-variants has a BOM, CRLF line endings and an empty-key line: all kept.
    const auto slide = slideOf("layout-variants");
    const std::string ini = fileText(dataDir() / "layout-variants" / "Slidedat.ini");
    EXPECT_EQ(ini, slide->getRawMetadata());
    EXPECT_EQ(slideio::MetadataFormat::Text, slide->getMetadataFormat());
    EXPECT_EQ(ini, slide->getScene(0)->getRawMetadata());
    EXPECT_EQ(slideio::MetadataFormat::Text, slide->getScene(0)->getMetadataFormat());
}

TEST(MRXSScene, theMetadataTreeHoldsTheIniAndTheDriversFindings)
{
    const auto slide = slideOf("rawmap-d2");
    const slideio::Metadata& tree = slide->getMetadata();
    EXPECT_EQ("00112233445566778899AABBCCDDEEFF", tree["ini"]["GENERAL"]["SLIDE_ID"].asString());
    EXPECT_EQ("PNG", tree["ini"]["LAYER_0_LEVEL_0_SECTION"]["IMAGE_FORMAT"].asString());
    const slideio::Metadata& mrxs = tree["mrxs"];
    EXPECT_EQ("raw map", mrxs["positionSource"].asString());
    EXPECT_TRUE(mrxs["positionAware"].asBool());
    EXPECT_EQ(4u, mrxs["levels"].size());
    EXPECT_EQ("PNG", mrxs["levels"][0]["format"].asString());
    EXPECT_EQ(-3, mrxs["contentBounds"][0].asInt());
    EXPECT_EQ(-2, mrxs["contentBounds"][1].asInt());
    EXPECT_NE(std::string::npos, mrxs["renderingProfile"].asString().find("RGB"));
    // The scene carries the same tree.
    EXPECT_EQ("raw map", slide->getScene(0)->getMetadata()["mrxs"]["positionSource"].asString());
}

TEST(MRXSScene, layersTheDriverIgnoresAreNamed)
{
    // layout-variants has a second hierarchical family and a tagged non-hierarchical one.
    const slideio::Metadata& ignored = slideOf("layout-variants")->getMetadata()["mrxs"]["ignoredLayers"];
    std::vector<std::string> names;
    for (size_t i = 0; i < ignored.size(); ++i) {
        names.push_back(ignored[i].asString());
    }
    EXPECT_NE(names.end(), std::find(names.begin(), names.end(), "Slide filter level"));
    EXPECT_NE(names.end(), std::find(names.begin(), names.end(), "Scan info data layer"));
}

TEST(MRXSScene, associatedImagesAreMacroLabelAndThumbnail)
{
    const auto slide = slideOf("rawmap-d2");
    const json expected = expectedOf("rawmap-d2")["associated"];
    const std::list<std::string>& names = slide->getAuxImageNames();
    EXPECT_EQ((std::list<std::string>{"Macro", "Label", "Thumbnail"}), names);
    for (const std::string& name : names) {
        SCOPED_TRACE(name);
        const auto aux = slide->getAuxImage(name);
        EXPECT_EQ(name, aux->getName());
        EXPECT_FALSE(aux->canReopenByPath());
        EXPECT_EQ(slideio::Compression::Jpeg, aux->getCompression());
        const cv::Rect rect = aux->getRect();
        EXPECT_EQ(expected[name]["width"].get<int>(), rect.width);
        EXPECT_EQ(expected[name]["height"].get<int>(), rect.height);
        cv::Mat pixels;
        aux->readBlock(rect, pixels);
        ASSERT_EQ(CV_8UC3, pixels.type());
        const cv::Scalar mean = cv::mean(pixels);
        for (int c = 0; c < 3; ++c) {
            EXPECT_NEAR(expected[name]["rgb"][c].get<double>(), mean[c], 2.0) << "channel " << c;
        }
    }
}

TEST(MRXSScene, anUnknownAssociatedImageIsAnError)
{
    const auto slide = slideOf("rawmap-d2");
    EXPECT_ANY_THROW(slide->getAuxImage("Overview"));
}

TEST(MRXSScene, aSlideWithoutAssociatedImagesHasNone)
{
    // bmp24 has no Scan data layer at all.
    const auto slide = slideOf("bmp24");
    EXPECT_EQ(0, slide->getNumAuxImages());
    EXPECT_ANY_THROW(slide->getAuxImage("Macro"));
}

TEST(MRXSScene, aSceneOutlivesItsSlide)
{
    std::shared_ptr<slideio::CVScene> scene, macro;
    {
        const auto slide = slideOf("rawmap-d2");
        scene = slide->getScene(0);
        macro = slide->getAuxImage("Macro");
    }
    cv::Mat pixels;
    EXPECT_NO_THROW(scene->readBlock(cv::Rect(0, 0, 20, 20), pixels));
    EXPECT_EQ(cv::Size(20, 20), pixels.size());
    EXPECT_NO_THROW(macro->readBlock(macro->getRect(), pixels));
}

TEST(MRXSScene, theMainSceneCanBeReopenedByPath)
{
    EXPECT_TRUE(slideOf("nomap-d1")->getScene(0)->canReopenByPath());
}

TEST(MRXSScene, onlySceneZeroExists)
{
    const auto slide = slideOf("nomap-d1");
    EXPECT_ANY_THROW(slide->getScene(1));
    EXPECT_ANY_THROW(slide->getScene(-1));
}
