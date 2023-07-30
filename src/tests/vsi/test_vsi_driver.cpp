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

TEST(VSIImageDriver, VSIFileOpen)
{
    std::string filePath = TestTools::getFullTestImagePath("vsi", "OS-1/OS-1.vsi");
    vsi::VSIFile vsiFile(filePath);
    EXPECT_EQ(2, vsiFile.getNumExternalFiles());
    ASSERT_EQ(2, vsiFile.getNumPyramids());
    const vsi::Pyramid& pyramid1 = vsiFile.getPyramid(0);
    EXPECT_EQ(pyramid1.name, "Overview");
    EXPECT_EQ(pyramid1.stackType, vsi::StackType::OVERVIEW_IMAGE);
    const vsi::Pyramid& pyramid2 = vsiFile.getPyramid(1);
    EXPECT_EQ(pyramid2.name, "20x");
    EXPECT_EQ(pyramid2.stackType, vsi::StackType::DEFAULT_IMAGE);
}
