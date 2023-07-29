#include <gtest/gtest.h>
#include "tests/testlib/testtools.hpp"
#include <string>
#include "slideio/drivers/vsi/vsiimagedriver.hpp"
#include "slideio/drivers/vsi/vsiscene.hpp"
#include "slideio/drivers/vsi/vsislide.hpp"


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

TEST(VSIImageDriver, openFile2)
{
    std::string filePath = TestTools::getFullTestImagePath("vsi", "OS-1/OS-1.vsi");
    VSISlide slide(filePath);
    ASSERT_EQ(1, slide.getNumExternalFiles());
}
