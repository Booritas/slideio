#include <gtest/gtest.h>
#include "tests/testlib/testtools.hpp"
#include <string>
#include "slideio/drivers/vsi/vsiimagedriver.hpp"
#include "slideio/drivers/vsi/vsiscene.hpp"
#include "slideio/drivers/vsi/vsislide.hpp"
#include "slideio/drivers/vsi/vsifile.hpp"


namespace slideio
{
    class Slide;
}

using namespace slideio;

TEST(VSIImageDriver, openFileWithoutExternalFiles)
{
    std::string filePath = TestTools::getFullTestImagePath("vsi",
        "Zenodo/Q6VM49JF/Figure-1-ultrasound-raw-data"
        "/SPECTRUM_#201_2016-06-14_Jiangtao Liu/1286FL9057GDF8RGDX257R2GLHZ.vsi");
    slideio::VSIImageDriver driver;
    std::shared_ptr<CVSlide> slide = driver.openFile(filePath);
    ASSERT_TRUE(slide!=nullptr);
    const int numScenes = slide->getNumScenes();
    ASSERT_EQ(1, numScenes);
    //std::shared_ptr<CVScene> scene = slide->getScene(0);
    //EXPECT_EQ(scene->getName(), "001 C405, C488");
    //auto rect = scene->getRect();
    //EXPECT_EQ(rect.width, 1645);
    //EXPECT_EQ(rect.height, 1682);
    //EXPECT_EQ(rect.x, 0);
    //EXPECT_EQ(rect.y, 0);
    //EXPECT_DOUBLE_EQ(scene->getMagnification(), 60.);
}

TEST(VSIImageDriver, openFileWithExternalFiles)
{
    std::tuple<std::string,int,int, double> result[] = {
        {"40x_01", 14749,20874,40},
        {"40x_02", 15596,19403,40},
        {"40x_03", 16240,18759,40},
    };
    std::string filePath = TestTools::getFullTestImagePath("vsi", "Zenodo/Abdominal/G1M16_ABD_HE_B6.vsi");
    slideio::VSIImageDriver driver;
    std::shared_ptr<CVSlide> slide = driver.openFile(filePath);
    ASSERT_TRUE(slide != nullptr);
    const int numScenes = slide->getNumScenes();
    ASSERT_EQ(4, numScenes);
    //for(int sceneIndex=0; sceneIndex<numScenes; ++sceneIndex) {
    //    std::shared_ptr<CVScene> scene = slide->getScene(sceneIndex);
    //    EXPECT_EQ(scene->getName(), std::get<0>(result[sceneIndex]));
    //    auto rect = scene->getRect();
    //    EXPECT_EQ(rect.width, std::get<1>(result[sceneIndex]));
    //    EXPECT_EQ(rect.height, std::get<2>(result[sceneIndex]));
    //    EXPECT_EQ(rect.x, 0);
    //    EXPECT_EQ(rect.y, 0);
    //    EXPECT_DOUBLE_EQ(scene->getMagnification(), std::get<3>(result[sceneIndex]));

    //}
    //const int numAuxImages = slide->getNumAuxImages();
    //ASSERT_EQ(1, numAuxImages);
    //auto imageNames = slide->getAuxImageNames();
    //auto image = slide->getAuxImage(imageNames.front());
    //EXPECT_EQ(image->getName(), "Overview");
}


TEST(VSIImageDriver, VSIFileOpenWithExternalFiles)
{
    std::string filePath = TestTools::getFullTestImagePath("vsi", "Zenodo/Abdominal/G1M16_ABD_HE_B6.vsi");
    vsi::VSIFile vsiFile(filePath);
    EXPECT_EQ(4, vsiFile.getNumExternalFiles());
}

TEST(VSIImageDriver, VSIFileOpenWithOutExternalFiles)
{
    std::string filePath = TestTools::getFullTestImagePath("vsi", 
        "Zenodo/Q6VM49JF/Figure-1-ultrasound-raw-data"
                "/SPECTRUM_#201_2016-06-14_Jiangtao Liu/1286FL9057GDF8RGDX257R2GLHZ.vsi");
    vsi::VSIFile vsiFile(filePath);
    EXPECT_EQ(0, vsiFile.getNumExternalFiles());
}

