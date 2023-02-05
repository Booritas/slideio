#include <iostream>
#include "slideio/drivers/svs/svsimagedriver.hpp"
#include "tests/testlib/testtools.hpp"
using namespace slideio;

void test()
{
    SVSImageDriver driver;
    const std::string filePath = TestTools::getFullTestImagePath("svs", "private/0A847635-D91B-4C99-9407-A15AE3EED72A.SVS");
    auto slide = driver.openFile(filePath.c_str());
    auto scene = slide->getScene(0);
    cv::Rect blockRect[] = {
        {1000,1000,256,256},
        {2000,2000,256,256},
        {3000,3000,256,256},
        {4000,4000,256,256},
        {5000,5000,256,256},
        {6000,6000,256,256},
        {7000,7000,256,256},
    };
    for(auto rc:blockRect) {
        cv::Mat mat;
        scene->readBlock(rc, mat);
    }
    std::cout << "Done" << std::endl;
}

int main()
{
    test();
    return 0;
    
}
