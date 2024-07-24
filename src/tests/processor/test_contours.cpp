#include <algorithm>
#include <gtest/gtest.h>
#include <slideio/processor/contourmanager.hpp>

using namespace slideio;

TEST(ContourManager, addContour) {
    ContourManager contourManager;
    std::vector<int32_t> contour;
    constexpr int contourSize = 10;
    contour.resize(contourSize);
    int count = 0;
    std::for_each(contour.begin(), contour.end(), [&count](int32_t& val) { val = ++count; });
    int32_t* ptr = contourManager.addContour(contour.data(), count);
    std::vector<int32_t> result(ptr,ptr+count);
    EXPECT_EQ(contour,result);
}