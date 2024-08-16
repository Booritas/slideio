#include <gtest/gtest.h>
#include "tests/testlib/testtools.hpp"
#include <string>
#include <tinyxml2.h>
#include <opencv2/imgproc.hpp>

#include "slideio/core/tools/tools.hpp"
#include "slideio/drivers/ome-tiff/otimagedriver.hpp"
#include "slideio/drivers/ome-tiff/otsmallscene.hpp"


namespace slideio
{
    class Slide;
}

using namespace slideio;


TEST(OTImageDriver, openMultifileSlide) {
    std::string filePath = TestTools::getFullTestImagePath("ometiff", "Multifile/multifile-Z1.ome.tiff");
    slideio::ometiff::OTImageDriver driver;
    std::shared_ptr<CVSlide> slide = driver.openFile(filePath);
    ASSERT_TRUE(slide != nullptr);
}

TEST(OTImageDriver, openMultiResolutionSlide) {
    std::string filePath = TestTools::getFullTestImagePath("ometiff", "Subresolutions/Leica-2.ome.tiff");
    slideio::ometiff::OTImageDriver driver;
    std::shared_ptr<CVSlide> slide = driver.openFile(filePath);
    ASSERT_TRUE(slide != nullptr);
}
