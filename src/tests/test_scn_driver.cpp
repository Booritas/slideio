// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#include <gtest/gtest.h>
#include "slideio/core/imagedrivermanager.hpp"
#include "slideio/drivers/scn/scnimagedriver.hpp"
#include <opencv2/imgcodecs.hpp>
#include <boost/algorithm/string/predicate.hpp>

#include "testtools.hpp"
#include "slideio/scene.hpp"

TEST(SCNImageDriver, DriverManager_getDriverIDs)
{
    std::vector<std::string> driverIds = slideio::ImageDriverManager::getDriverIDs();
    auto it = std::find(driverIds.begin(),driverIds.end(), "SCN");
    EXPECT_FALSE(it==driverIds.end());
}
TEST(SCNImageDriver, getID)
{
    slideio::SCNImageDriver driver;
    std::string id = driver.getID();
    EXPECT_EQ(id,"SCN");
}

TEST(SCNImageDriver, canOpenFile)
{
    slideio::SCNImageDriver driver;
    EXPECT_TRUE(driver.canOpenFile("c:\\abbb\\a.scn"));
    EXPECT_FALSE(driver.canOpenFile("c:\\abbb\\a.scn.tmp"));
}

TEST(SCNImageDriver, slideRawMetadata)
{
    const std::string images[] = {
        "Leica-Fluorescence-1.scn"
    };
    slideio::SCNImageDriver driver;
    for (const auto& imageName : images)
    {
        std::string filePath = TestTools::getTestImagePath("scn", imageName);
        std::shared_ptr<slideio::CVSlide> slide = driver.openFile(filePath);
        const std::string& metadata = slide->getRawMetadata();
        EXPECT_GT(metadata.length(), 0);
        const std::string header("<?xml version=\"1.0\"?>");
        EXPECT_TRUE(boost::algorithm::starts_with(metadata, header));
    }
}


TEST(SCNImageDriver, openFile)
{
    slideio::SCNImageDriver driver;
    std::string filePath = TestTools::getTestImagePath("scn","Leica-Fluorescence-1.scn");
    std::shared_ptr<slideio::CVSlide> slide = driver.openFile(filePath);
    ASSERT_TRUE(slide!=nullptr);
    int numScenes = slide->getNumScenes();
    ASSERT_EQ(numScenes, 3);
    auto scene = slide->getScene(0);
    ASSERT_FALSE(scene == nullptr);
    //auto sceneRect = scene->getRect();
    //EXPECT_EQ(sceneRect.x, 0);
    //EXPECT_EQ(sceneRect.y, 0);
    //EXPECT_EQ(sceneRect.width, 512);
    //EXPECT_EQ(sceneRect.height, 512);
    //int numChannels = scene->getNumChannels();
    //EXPECT_EQ(numChannels, 3);
    //for(int channel=0; channel<numChannels; ++channel)
    //{
    //    EXPECT_EQ(scene->getChannelDataType(channel), slideio::DataType::DT_Byte);
    //}
    //EXPECT_EQ(scene->getMagnification(), 100.);
    //slideio::Resolution res = scene->getResolution();
    //const double fileRes = 9.76783e-8;
    //EXPECT_LT((100 * std::abs(res.x - fileRes) / fileRes), 1);
    //EXPECT_LT((100 * std::abs(res.y - fileRes) / fileRes), 1);
}

