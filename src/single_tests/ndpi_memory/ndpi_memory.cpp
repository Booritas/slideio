#define _CRTDBG_MAP_ALLOC
#include <stdlib.h>
#if defined(WIN32)
#include <crtdbg.h>
#endif

#include <iostream>
#include "slideio/drivers/ndpi/ndpiimagedriver.hpp"
#include "tests/testlib/testtools.hpp"
#include <memory>
using namespace slideio;

void test()
{
    google::InitGoogleLogging("slideio");
    FLAGS_minloglevel = 0;
    FLAGS_logtostderr = true;
    {
        NDPIImageDriver driver;
        const std::string filePath = TestTools::getFullTestImagePath("hamamatsu", "DM0014 - 2020-04-02 10.25.21.ndpi");
        std::shared_ptr<slideio::CVSlide> slide = driver.openFile(filePath.c_str());
        std::shared_ptr<CVScene>  scene = slide->getScene(0);
         cv::Rect rectScene = scene->getRect();
         const int blockWidth = 2000;
         const int blockHeight = 1000;
         cv::Rect blockRect = rectScene;//{ rectScene.width/2,rectScene.height/2,blockWidth,blockHeight };
         blockRect.x = 0;
         blockRect.y = 0;
         const double scale = 500./double(rectScene.width);
         cv::Size blockSize(std::lround(blockRect.width * scale), std::lround(blockRect.height * scale));
         std::vector<int> channelIndices = { 0, 1, 2 };
         cv::Mat mat;
         scene->readResampledBlockChannels(blockRect, blockSize, channelIndices, mat);
         int channels = scene->getNumChannels();
         std::cout << "Channels:" << channels << std::endl;
         std::string channel = scene->getChannelName(0);
         std::cout << "Channel 0: " << channel << std::endl;
    }
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
