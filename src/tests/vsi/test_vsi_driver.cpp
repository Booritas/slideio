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
    std::string filePath = TestTools::getFullTestImagePath("vsi", "Zenodo/Abdominal/G1M16_ABD_HE_B6.vsi");
    vsi::VSIFile vsiFile;
    vsiFile.read(filePath);
    ASSERT_EQ(1, vsiFile.getNumExternalFiles());
}
