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

TEST(VSIImageDriver, openFile)
{
    std::string filePath = TestTools::getTestImagePath("vsi", "vsi-multifile/vsi-ets-test-jpg2k.vsi");
    slideio::VSIImageDriver driver;
    std::shared_ptr<CVSlide> slide = driver.openFile(filePath);
    ASSERT_TRUE(slide!=nullptr);
    const int numScenes = slide->getNumScenes();
    ASSERT_EQ(1, numScenes);
    std::shared_ptr<CVScene> scene = slide->getScene(0);
    EXPECT_EQ(scene->getName(), "001 C405, C488");
    auto rect = scene->getRect();
    EXPECT_EQ(rect.width, 1645);
    EXPECT_EQ(rect.height, 1682);
    EXPECT_EQ(rect.x, 0);
    EXPECT_EQ(rect.y, 0);
    EXPECT_DOUBLE_EQ(scene->getMagnification(), 60.);
}

TEST(VSIImageDriver, openFileMultiscene)
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
    ASSERT_EQ(3, numScenes);
    for(int sceneIndex=0; sceneIndex<numScenes; ++sceneIndex) {
        std::shared_ptr<CVScene> scene = slide->getScene(sceneIndex);
        EXPECT_EQ(scene->getName(), std::get<0>(result[sceneIndex]));
        auto rect = scene->getRect();
        EXPECT_EQ(rect.width, std::get<1>(result[sceneIndex]));
        EXPECT_EQ(rect.height, std::get<2>(result[sceneIndex]));
        EXPECT_EQ(rect.x, 0);
        EXPECT_EQ(rect.y, 0);
        EXPECT_DOUBLE_EQ(scene->getMagnification(), std::get<3>(result[sceneIndex]));

    }
    const int numAuxImages = slide->getNumAuxImages();
    ASSERT_EQ(1, numAuxImages);
    auto imageNames = slide->getAuxImageNames();
    auto image = slide->getAuxImage(imageNames.front());
    EXPECT_EQ(image->getName(), "Overview");
}

TEST(VSIImageDriver, VSIFileOpen0)
{
    std::string filePath = TestTools::getFullTestImagePath("vsi", "vsi-multifile/vsi-ets-test-jpg2k.vsi");
    vsi::VSIFile vsiFile(filePath);
    EXPECT_EQ(1, vsiFile.getNumExternalFiles());
    ASSERT_EQ(1, vsiFile.getNumPyramids());
    const std::shared_ptr<vsi::Pyramid> pyramid1 = vsiFile.getPyramid(0);
    EXPECT_EQ(pyramid1->name, "001 C405, C488");
    EXPECT_EQ(pyramid1->stackType, vsi::StackType::UNKNOWN);
}

TEST(VSIImageDriver, VSIFileOpen1)
{
    std::string filePath = TestTools::getFullTestImagePath("vsi", "OS-1/OS-1.vsi");
    vsi::VSIFile vsiFile(filePath);
    EXPECT_EQ(2, vsiFile.getNumExternalFiles());
    ASSERT_EQ(2, vsiFile.getNumPyramids());
    const std::shared_ptr<vsi::Pyramid> pyramid1 = vsiFile.getPyramid(0);
    EXPECT_EQ(pyramid1->name, "Overview");
    EXPECT_EQ(pyramid1->stackType, vsi::StackType::OVERVIEW_IMAGE);
    EXPECT_DOUBLE_EQ(pyramid1->magnification, 2.);
    const std::shared_ptr<vsi::Pyramid> pyramid2 = vsiFile.getPyramid(1);
    EXPECT_EQ(pyramid2->name, "20x");
    EXPECT_EQ(pyramid2->stackType, vsi::StackType::DEFAULT_IMAGE);
    EXPECT_DOUBLE_EQ(pyramid2->magnification, 20.);
}

TEST(VSIImageDriver, VSIFileOpen2)
{
    std::string filePath = TestTools::getFullTestImagePath("vsi", "Zenodo/Abdominal/G1M16_ABD_HE_B6.vsi");
    vsi::VSIFile vsiFile(filePath);
    EXPECT_EQ(4, vsiFile.getNumExternalFiles());
    ASSERT_EQ(4, vsiFile.getNumPyramids());
    const std::shared_ptr<vsi::Pyramid> pyramid1 = vsiFile.getPyramid(0);
    EXPECT_EQ(pyramid1->name, "Overview");
    EXPECT_EQ(pyramid1->stackType, vsi::StackType::OVERVIEW_IMAGE);
    EXPECT_DOUBLE_EQ(pyramid1->magnification, 2.);
    const std::shared_ptr<vsi::Pyramid> pyramid2 = vsiFile.getPyramid(1);
    EXPECT_EQ(pyramid2->name, "40x_01");
    EXPECT_EQ(pyramid2->stackType, vsi::StackType::DEFAULT_IMAGE);
    EXPECT_DOUBLE_EQ(pyramid2->magnification, 40.);
    const std::shared_ptr<vsi::Pyramid> pyramid3 = vsiFile.getPyramid(2);
    EXPECT_EQ(pyramid3->name, "40x_02");
    EXPECT_EQ(pyramid3->stackType, vsi::StackType::DEFAULT_IMAGE);
    EXPECT_DOUBLE_EQ(pyramid3->magnification, 40.);
    const std::shared_ptr<vsi::Pyramid> pyramid4 = vsiFile.getPyramid(3);
    EXPECT_EQ(pyramid4->name, "40x_03");
    EXPECT_EQ(pyramid4->stackType, vsi::StackType::DEFAULT_IMAGE);
    EXPECT_DOUBLE_EQ(pyramid4->magnification, 40.);
}

