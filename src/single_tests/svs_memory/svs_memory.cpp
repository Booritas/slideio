#include <iostream>
#include "slideio/drivers/dcm/dcmimagedriver.hpp"
#include "tests/testlib/testtools.hpp"
#include "slideio/slideio/slideio.hpp"
using namespace slideio;

void test()
{
    const std::string filePath = TestTools::getFullTestImagePath("svs", "S1303802-11-HE-DX1.svs");
    auto slide = slideio::openSlide(filePath,"SVS");
    std::cout << "Done" << std::endl;
}

int main()
{
    test();
    return 0;
    
}
