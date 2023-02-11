#include <iostream>
#include "slideio/drivers/dcm/dcmimagedriver.hpp"
#include "tests/testlib/testtools.hpp"
using namespace slideio;

void test()
{
    DCMImageDriver driver;
    const std::string filePath = TestTools::getFullTestImagePath("dcm", "private/H01EBB49P-24900/H01EBB49P-24900_level-0.dcm");
    auto slide = driver.openFile(filePath.c_str());
    auto scene = slide->getScene(0);
    auto rect = scene->getRect();
    cv::Mat mat;
    scene->readBlock(rect, mat);
    // cv::Rect blockRect[] = {
    //     {1000,1000,256,256},
    //     {2000,2000,256,256},
    //     {3000,3000,256,256},
    //     {4000,4000,256,256},
    //     {5000,5000,256,256},
    //     {6000,6000,256,256},
    //     {7000,7000,256,256},
    // };
    // for(auto rc:blockRect) {
    //     cv::Mat mat;
    //     scene->readBlock(rc, mat);
    // }
    std::cout << "Done" << std::endl;
}

int main()
{
    test();
    return 0;
    
}
