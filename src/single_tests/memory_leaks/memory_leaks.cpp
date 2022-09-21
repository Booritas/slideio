#include <iostream>
#include "slideio/drivers/czi/cziimagedriver.hpp"
using namespace slideio;

int main()
{
    CZIImageDriver driver;
    auto slide = driver.openFile("/src/images/czi/30-10-2020_NothingRecognized-15986.czi");
    auto scene = slide->getScene(0);
    cv::Rect blockRect = { 0,0,1000,1000 };
    cv::Range zSliceRange = { 0, 3 };
    cv::Range timeFrameRange = { 0, 1 };
    std::vector<int> channelIndices = { 0 };
    cv::Mat mat;
    scene->read4DBlockChannels(blockRect, channelIndices, zSliceRange, timeFrameRange, mat);
    std::cout << "Done" << std::endl;
    return 0;
    
}