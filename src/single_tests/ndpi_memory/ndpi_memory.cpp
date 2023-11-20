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
        const std::string filePath = TestTools::getFullTestImagePath("hamamatsu", "openslide/CMU-1.ndpi");
        std::shared_ptr<slideio::CVSlide> slide = driver.openFile(filePath.c_str());
        std::shared_ptr<CVScene> scene = slide->getScene(0);
        cv::Rect rectScene = scene->getRect();
        const int blockWidth = 1024;
        const int blockHeight = 1024;
        int tile = 0;
        const int tiles = 20;
        cv::Rect blockRect = rectScene; //{ rectScene.width/2,rectScene.height/2,blockWidth,blockHeight };
        for (int x = 0; x < (rectScene.width-blockWidth) && tile<tiles; x += blockWidth) {
            for (int y = 0; y < (rectScene.height-blockHeight) && tile<tiles; y += blockHeight, tile++) {
                std::cout << "Tile: " << tile << std::endl;
                blockRect.x = x;
                blockRect.y = y;
                blockRect.width = blockWidth;
                blockRect.height = blockHeight;
                const double scale = 1.;// 500. / double(rectScene.width);
                cv::Size blockSize(std::lround(blockRect.width * scale), std::lround(blockRect.height * scale));
                std::vector<int> channelIndices = { 0, 1, 2 };
                cv::Mat mat;
                scene->readResampledBlockChannels(blockRect, blockSize, channelIndices, mat);
            }
        }
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
