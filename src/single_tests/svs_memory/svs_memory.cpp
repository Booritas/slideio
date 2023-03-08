#include <iostream>
#include "tests/testlib/testtools.hpp"
#include "slideio/slideio/slideio.hpp"
using namespace slideio;

void test()
{
    const std::string filePath = TestTools::getFullTestImagePath("scn", "openslide/Leica-1.scn");
    auto slide = openSlide(filePath,"SCN");
    std::cout << "Done" << std::endl;
}

int main()
{
    test();
    return 0;
    
}
