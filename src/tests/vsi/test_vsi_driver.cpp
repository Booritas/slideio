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
}

TEST(VSIImageDriver, VSIFileOpen0)
{
    std::string filePath = TestTools::getFullTestImagePath("vsi", "vsi-multifile/vsi-ets-test-jpg2k.vsi");
    vsi::VSIFile vsiFile(filePath);
    EXPECT_EQ(1, vsiFile.getNumExternalFiles());
    ASSERT_EQ(1, vsiFile.getNumPyramids());
    const vsi::Pyramid& pyramid1 = vsiFile.getPyramid(0);
    EXPECT_EQ(pyramid1.name, "001 C405, C488");
    EXPECT_EQ(pyramid1.stackType, vsi::StackType::UNKNOWN);
}

TEST(VSIImageDriver, VSIFileOpen1)
{
    std::string filePath = TestTools::getFullTestImagePath("vsi", "OS-1/OS-1.vsi");
    vsi::VSIFile vsiFile(filePath);
    EXPECT_EQ(2, vsiFile.getNumExternalFiles());
    ASSERT_EQ(2, vsiFile.getNumPyramids());
    const vsi::Pyramid& pyramid1 = vsiFile.getPyramid(0);
    EXPECT_EQ(pyramid1.name, "Overview");
    EXPECT_EQ(pyramid1.stackType, vsi::StackType::OVERVIEW_IMAGE);
    EXPECT_DOUBLE_EQ(pyramid1.magnification, 2.);
    const vsi::Pyramid& pyramid2 = vsiFile.getPyramid(1);
    EXPECT_EQ(pyramid2.name, "20x");
    EXPECT_EQ(pyramid2.stackType, vsi::StackType::DEFAULT_IMAGE);
    EXPECT_DOUBLE_EQ(pyramid2.magnification, 20.);
}

TEST(VSIImageDriver, VSIFileOpen2)
{
    std::string filePath = TestTools::getFullTestImagePath("vsi", "Zenodo/Abdominal/G1M16_ABD_HE_B6.vsi");
    vsi::VSIFile vsiFile(filePath);
    EXPECT_EQ(4, vsiFile.getNumExternalFiles());
    ASSERT_EQ(4, vsiFile.getNumPyramids());
    const vsi::Pyramid& pyramid1 = vsiFile.getPyramid(0);
    EXPECT_EQ(pyramid1.name, "Overview");
    EXPECT_EQ(pyramid1.stackType, vsi::StackType::OVERVIEW_IMAGE);
    EXPECT_DOUBLE_EQ(pyramid1.magnification, 2.);
    const vsi::Pyramid& pyramid2 = vsiFile.getPyramid(1);
    EXPECT_EQ(pyramid2.name, "40x_01");
    EXPECT_EQ(pyramid2.stackType, vsi::StackType::DEFAULT_IMAGE);
    EXPECT_DOUBLE_EQ(pyramid2.magnification, 40.);
    const vsi::Pyramid& pyramid3 = vsiFile.getPyramid(2);
    EXPECT_EQ(pyramid3.name, "40x_02");
    EXPECT_EQ(pyramid3.stackType, vsi::StackType::DEFAULT_IMAGE);
    EXPECT_DOUBLE_EQ(pyramid3.magnification, 40.);
    const vsi::Pyramid& pyramid4 = vsiFile.getPyramid(3);
    EXPECT_EQ(pyramid4.name, "40x_03");
    EXPECT_EQ(pyramid4.stackType, vsi::StackType::DEFAULT_IMAGE);
    EXPECT_DOUBLE_EQ(pyramid4.magnification, 40.);
}

