#define _CRTDBG_MAP_ALLOC
#include <stdlib.h>
#include <crtdbg.h>

#include <iostream>
#include "slideio/drivers/ndpi/ndpiimagedriver.hpp"
#include "tests/testlib/testtools.hpp"
using namespace slideio;

void test()
{
    google::InitGoogleLogging("slideio");
    FLAGS_minloglevel = 0;
    FLAGS_logtostderr = true;

    NDPIImageDriver driver;
    const std::string filePath = TestTools::getFullTestImagePath("hamamatsu", "2017-02-27 15.39.33.ndpi");
    auto slide = driver.openFile(filePath.c_str());
    auto scene = slide->getScene(0);
    cv::Rect rectScene = scene->getRect();
    const int blockWidth = 2000;
    const int blockHeight = 1000;
    cv::Rect blockRect = { rectScene.width/2,rectScene.height/2,blockWidth,blockHeight };
    const double scale = 0.6;
    cv::Size blockSize(std::lround(blockWidth * scale), std::lround(blockHeight * scale));
    std::vector<int> channelIndices = { 0, 1, 2 };
    cv::Mat mat;
    scene->readResampledBlock(blockRect, blockSize, mat);
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
