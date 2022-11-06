#if defined(WIN32)
#define _CRTDBG_MAP_ALLOC
#include <stdlib.h>
#include <crtdbg.h>
#endif

#include <iostream>
#include "slideio/drivers/czi/cziimagedriver.hpp"
#include "tests/testlib/testtools.hpp"
using namespace slideio;

void test()
{
    CZIImageDriver driver;
    const std::string filePath = TestTools::getFullTestImagePath("czi", "30-10-2020_NothingRecognized-15986.czi");
    auto slide = driver.openFile(filePath.c_str());
    auto scene = slide->getScene(0);
    cv::Rect blockRect = { 0,0,1000,1000 };
    cv::Range zSliceRange = { 0, 3 };
    cv::Range timeFrameRange = { 0, 1 };
    std::vector<int> channelIndices = { 0 };
    cv::Mat mat;
    scene->read4DBlockChannels(blockRect, channelIndices, zSliceRange, timeFrameRange, mat);
    std::cout << "Done" << std::endl;
}

int main()
{
    test();
#if defined(WIN32)
    _CrtDumpMemoryLeaks();
#endif
    return 0;
    
}
