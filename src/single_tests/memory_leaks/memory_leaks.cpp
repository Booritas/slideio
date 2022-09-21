#include <iostream>
#include "slideio/drivers/czi/cziimagedriver.hpp"
#include "slideio/drivers/svs/svsimagedriver.hpp"
using namespace slideio;

int main()
{
    SVSImageDriver driver;
    auto slide = driver.openFile("d:/Projects/slideio/images/svs/private/0ad27f9885.svs");
    auto scene = slide->getScene(0);
    cv::Rect blockRect = { 0,0,1000,1000 };
    cv::Range zSliceRange = { 0, 1 };
    cv::Range timeFrameRange = { 0, 1 };
    std::vector<int> channelIndices = { 0 };
    cv::Mat mat;
    scene->read4DBlockChannels(blockRect, channelIndices, zSliceRange, timeFrameRange, mat);
    std::cout << "Done" << std::endl;
    return 0;
    
}