// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
//
// mrxs.crop_to_content: the crop box and the cropped scene (open options design,
// section 7), on the committed fixtures. offset-content's content starts at (27, 9),
// which no coarser downsample divides, so every level origin is non-zero.
#include <gtest/gtest.h>
#include <algorithm>
#include <filesystem>
#include <string>
#include <functional>
#include <opencv2/core.hpp>
#include "slideio/drivers/mrxs/mrxsdataset.hpp"
#include "slideio/drivers/mrxs/mrxsgeometry.hpp"
#include "slideio/drivers/mrxs/mrxsimagedriver.hpp"
#include "slideio/slideio/slideio.hpp"
#include "slideio/slideio/sceneinternal.hpp"
#include "slideio/converter/converterparameters.hpp"
#include "slideio/converter/tiffconverter.hpp"
#include "slideio/core/cvscene.hpp"
#include "slideio/core/cvslide.hpp"
#include "slideio/core/exceptions.hpp"

namespace fs = std::filesystem;
using namespace slideio::mrxs;
using slideio::DriverOptions;

namespace
{
    const fs::path dataDir() { return fs::path(MRXS_TEST_DATA_DIR); }

    std::string entry(const std::string& name) { return (dataDir() / (name + ".mrxs")).u8string(); }

    std::shared_ptr<slideio::CVSlide> croppedOf(const std::string& name)
    {
        return slideio::MRXSImageDriver().openFile(entry(name), DriverOptions{{"crop_to_content", "true"}});
    }

    std::shared_ptr<slideio::CVSlide> plainOf(const std::string& name)
    {
        return slideio::MRXSImageDriver().openFile(entry(name), DriverOptions{{"crop_to_content", "false"}});
    }

    cv::Mat read(const std::shared_ptr<slideio::CVScene>& scene, const cv::Rect& rect, const cv::Size& size)
    {
        cv::Mat out;
        scene->readResampledBlockChannels(rect, size, {}, out);
        return out;
    }

    cv::Mat readLevel(const std::shared_ptr<slideio::CVScene>& scene, int level, const cv::Rect& rect)
    {
        cv::Mat out;
        scene->readResampledLevelBlockChannels(level, rect, rect.size(), {}, out);
        return out;
    }

    bool same(const cv::Mat& a, const cv::Mat& b)
    {
        return a.size() == b.size() && a.type() == b.type() && cv::norm(a, b, cv::NORM_INF) == 0;
    }

    std::string errorOf(const std::function<void()>& call)
    {
        try {
            call();
        }
        catch (const std::exception& e) {
            return e.what();
        }
        return "";
    }
}

TEST(MRXSCropBox, isTheContentBoundsWhenTheyLieInsideTheCanvas)
{
    const Geometry g = MRXSDataset::open(entry("offset-content"))->geometry;
    const auto box = computeCropBox(g);
    ASSERT_TRUE(box.has_value());
    EXPECT_EQ(cv::Rect(27, 9, 45, 45), box->level0);
    ASSERT_EQ(g.levels.size(), box->levels.size());
    EXPECT_EQ(box->level0, box->levels[0]);
    auto expectLevel = [&](size_t l, int64_t d, int x0, int y0, int64_t cx1, int64_t cy1) {
        const int x1 = static_cast<int>(std::min(cx1, g.levels[l].width));
        const int y1 = static_cast<int>(std::min(cy1, g.levels[l].height));
        EXPECT_EQ(d, g.levels[l].downsample);
        EXPECT_EQ(cv::Rect(x0, y0, x1 - x0, y1 - y0), box->levels[l]) << "level " << l;
    };
    expectLevel(1, 2, 13, 4, 36, 27);   // floor(27/2), floor(9/2), ceil(72/2), ceil(54/2)
    expectLevel(2, 4, 6, 2, 18, 14);    // floor(27/4), floor(9/4), ceil(72/4), ceil(54/4)
}

TEST(MRXSCropBox, isClippedToTheCanvas)
{
    const Geometry g = MRXSDataset::open(entry("rawmap-d2"))->geometry;    // bounds (-3, -2, 93, 70)
    const auto box = computeCropBox(g);
    ASSERT_TRUE(box.has_value());
    EXPECT_EQ(cv::Rect(0, 0, static_cast<int>(std::min<int64_t>(90, g.levels[0].width)),
                       static_cast<int>(std::min<int64_t>(68, g.levels[0].height))), box->level0);
}

TEST(MRXSCropBox, isAbsentWithoutDrawablesOrOutsideTheCanvas)
{
    Geometry g = MRXSDataset::open(entry("nomap-d1"))->geometry;
    g.bounds0.reset();
    EXPECT_FALSE(computeCropBox(g).has_value());
    g.bounds0 = cv::Rect2d(double(g.levels[0].width) + 5, 0, 10, 10);
    EXPECT_FALSE(computeCropBox(g).has_value());
}

TEST(MRXSCropBox, theDatasetCarriesOneOnlyWhenAsked)
{
    const auto plain = MRXSDataset::open(entry("offset-content"));
    EXPECT_FALSE(plain->cropRequested);
    EXPECT_FALSE(plain->crop.has_value());
    const auto cropped = MRXSDataset::open(entry("offset-content"), RuntimeOptions(), true);
    EXPECT_TRUE(cropped->cropRequested);
    ASSERT_TRUE(cropped->crop.has_value());
    EXPECT_EQ(cv::Rect(27, 9, 45, 45), cropped->crop->level0);
}

TEST(MRXSCrop, theCroppedSceneIsTheBoxStartingAtZero)
{
    EXPECT_EQ(cv::Rect(0, 0, 45, 45), croppedOf("offset-content")->getScene(0)->getRect());
}

TEST(MRXSCrop, everyLevelReportsItsBoxSize)
{
    const auto plain = plainOf("offset-content")->getScene(0);
    const auto cropped = croppedOf("offset-content")->getScene(0);
    const CropBox box = *computeCropBox(MRXSDataset::open(entry("offset-content"))->geometry);
    ASSERT_EQ(plain->getNumZoomLevels(), cropped->getNumZoomLevels());
    for (int l = 0; l < cropped->getNumZoomLevels(); ++l) {
        EXPECT_EQ(box.levels[l].width, cropped->getZoomLevelInfo(l)->getSize().width) << "level " << l;
        EXPECT_EQ(box.levels[l].height, cropped->getZoomLevelInfo(l)->getSize().height) << "level " << l;
        EXPECT_EQ(plain->getZoomLevelInfo(l)->getScale(), cropped->getZoomLevelInfo(l)->getScale());
    }
}

TEST(MRXSCrop, levelReadsEqualUncroppedReadsShiftedByTheLevelBox)
{
    const auto plain = plainOf("offset-content")->getScene(0);
    const auto cropped = croppedOf("offset-content")->getScene(0);
    const CropBox box = *computeCropBox(MRXSDataset::open(entry("offset-content"))->geometry);
    for (int l = 0; l < cropped->getNumZoomLevels(); ++l) {
        const cv::Rect whole(0, 0, box.levels[l].width, box.levels[l].height);
        for (const cv::Rect& r : {whole, cv::Rect(1, 2, 5, 4) & whole}) {
            EXPECT_TRUE(same(readLevel(cropped, l, r), readLevel(plain, l, r + box.levels[l].tl())))
                << "level " << l << " rect " << r;
        }
    }
}

TEST(MRXSCrop, scaledReadsEqualUncroppedReadsShiftedByTheCropOrigin)
{
    const auto plain = plainOf("offset-content")->getScene(0);
    const auto cropped = croppedOf("offset-content")->getScene(0);
    const cv::Point origin(27, 9);
    const std::pair<cv::Rect, cv::Size> cases[] = {
        {cv::Rect(0, 0, 45, 45), cv::Size(17, 13)},
        {cv::Rect(3, 5, 30, 20), cv::Size(11, 7)},
        {cv::Rect(0, 0, 45, 45), cv::Size(45, 45)},
        {cv::Rect(7, 1, 20, 30), cv::Size(9, 14)},
    };
    for (const auto& c : cases) {
        EXPECT_TRUE(same(read(cropped, c.first, c.second), read(plain, c.first + origin, c.second)))
            << c.first << " -> " << c.second;
    }
}

TEST(MRXSCrop, aReadOverhangingTheEdgeShowsBackgroundBeyondIt)
{
    const auto plain = plainOf("offset-content")->getScene(0);
    const auto cropped = croppedOf("offset-content")->getScene(0);
    const cv::Mat img = readLevel(cropped, 0, cv::Rect(40, 40, 10, 10));
    ASSERT_EQ(cv::Size(10, 10), img.size());
    EXPECT_TRUE(same(img(cv::Rect(0, 0, 5, 5)), readLevel(plain, 0, cv::Rect(67, 49, 5, 5))));
    for (int y = 0; y < 10; ++y) {
        for (int x = 0; x < 10; ++x) {
            if (x < 5 && y < 5) {
                continue;
            }
            const cv::Vec3b p = img.at<cv::Vec3b>(y, x);
            for (int c = 0; c < 3; ++c) {
                EXPECT_EQ(cropped->getChannelBackground(c), p[c]) << "(" << x << ", " << y << ") channel " << c;
            }
        }
    }
}

TEST(MRXSCrop, rawmapIsClippedToTheCanvas)
{
    const cv::Rect plainRect = plainOf("rawmap-d2")->getScene(0)->getRect();
    EXPECT_EQ(cv::Rect(0, 0, std::min(90, plainRect.width), std::min(68, plainRect.height)),
              croppedOf("rawmap-d2")->getScene(0)->getRect());
}

TEST(MRXSCrop, metadataCarriesTheCropOnSlideAndScene)
{
    const auto slide = croppedOf("offset-content");
    for (const slideio::Metadata& m : {slide->getMetadata(), slide->getScene(0)->getMetadata()}) {
        const slideio::Metadata crop = m["mrxs"]["crop"];
        ASSERT_EQ(4u, crop.size());
        EXPECT_EQ(27, crop[0].asInt());
        EXPECT_EQ(9, crop[1].asInt());
        EXPECT_EQ(45, crop[2].asInt());
        EXPECT_EQ(45, crop[3].asInt());
        EXPECT_EQ(27, m["mrxs"]["contentBounds"][0].asInt());   // still nominal
        EXPECT_EQ(72, m["mrxs"]["nominalSize"][0].asInt());
    }
}

TEST(MRXSCrop, noOptionIsTheSameAsTrue)
{
    slideio::MRXSImageDriver driver;
    for (const auto& slide : {driver.openFile(entry("offset-content")),
                              driver.openFile(entry("offset-content"), DriverOptions{})}) {
        EXPECT_EQ(cv::Rect(0, 0, 45, 45), slide->getScene(0)->getRect());
        EXPECT_EQ((DriverOptions{{"mrxs.crop_to_content", "true"}}), slide->getScene(0)->getOpenOptions());
    }
}

TEST(MRXSCrop, falseGivesTheFullCanvas)
{
    const auto off = plainOf("offset-content");
    EXPECT_EQ(cv::Rect(0, 0, 72, 54), off->getScene(0)->getRect());
    EXPECT_FALSE(off->getMetadata()["mrxs"].contains("crop"));
    EXPECT_FALSE(off->getScene(0)->getMetadata()["mrxs"].contains("crop"));
    EXPECT_EQ((DriverOptions{{"mrxs.crop_to_content", "false"}}), off->getScene(0)->getOpenOptions());
}

TEST(MRXSCrop, associatedImagesAreUnchanged)
{
    const auto plain = plainOf("offset-content");
    const auto cropped = croppedOf("offset-content");
    for (const char* name : {"Label", "Macro", "Thumbnail"}) {
        EXPECT_EQ(plain->getAuxImage(name)->getRect(), cropped->getAuxImage(name)->getRect()) << name;
    }
}

TEST(MRXSCrop, unknownNamesAndMalformedBooleansAreRejected)
{
    slideio::MRXSImageDriver driver;
    const std::string wrongCase = errorOf([&] {
        driver.openFile(entry("offset-content"), DriverOptions{{"Crop_To_Content", "true"}});
    });
    EXPECT_NE(std::string::npos, wrongCase.find("has no open option 'mrxs.Crop_To_Content'")) << wrongCase;
    const std::string badBool = errorOf([&] {
        driver.openFile(entry("offset-content"), DriverOptions{{"crop_to_content", "yes"}});
    });
    EXPECT_NE(std::string::npos, badBool.find("is not a boolean")) << badBool;
    EXPECT_NE(std::string::npos, badBool.find("mrxs.crop_to_content")) << badBool;
}

TEST(MRXSCrop, openSlideRoutesOptionsAndIgnoresOtherDrivers)
{
    for (const char* driver : {"MRXS", "AUTO"}) {
        const auto slide = slideio::openSlide(entry("offset-content"), driver,
                                              {{"MRXS.crop_to_content", "true"}, {"svs.anything", "1"}});
        EXPECT_EQ(std::make_tuple(0, 0, 45, 45), slide->getScene(0)->getRect()) << driver;
        const auto off = slideio::openSlide(entry("offset-content"), driver, {{"mrxs.crop_to_content", "0"}});
        EXPECT_EQ(std::make_tuple(0, 0, 72, 54), off->getScene(0)->getRect()) << driver;
    }
    EXPECT_EQ(slideio::openSlide(entry("offset-content"))->getScene(0)->getRect(),
              slideio::openSlide(entry("offset-content"), "AUTO", {})->getScene(0)->getRect());
}

TEST(MRXSCrop, openOptionsRoundTripAndTheConverterReopensCropped)
{
    const auto cropped = croppedOf("offset-content")->getScene(0);
    EXPECT_EQ((DriverOptions{{"mrxs.crop_to_content", "true"}}), cropped->getOpenOptions());
    slideio::converter::OMETIFFJp2KConverterParameters parameters;
    slideio::converter::TiffConverter converter;
    converter.createFileLayout(cropped, parameters);
    const auto clone = converter.cloneScene();
    ASSERT_NE(nullptr, clone.second);
    const auto cloneScene = slideio::SceneInternal::getCVScene(clone.second);
    EXPECT_NE(cropped, cloneScene);     // re-opened, not shared
    EXPECT_EQ(cv::Rect(0, 0, 45, 45), cloneScene->getRect());
}
