#include <gtest/gtest.h>
#include "testtools.hpp"

std::string TestTools::getTestImageDirectory()
{
    const char* var = getenv("SLIDEIO_TEST_DATA_PATH");
    if(var==nullptr)
        throw std::runtime_error("Undefined environment variable SLIDEIO_TEST_DATA_PATH");
    std::string testDirPath(var);
    return testDirPath;
}

std::string TestTools::getTestImagePath(const std::string& subfolder, const std::string& image)
{
    std::string imagePath(getTestImageDirectory());
    if(!subfolder.empty())
        imagePath += std::string("/") + subfolder;
    imagePath += std::string("/") +  image;
    return imagePath;
}