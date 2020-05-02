#include <gtest/gtest.h>
#include "testtools.hpp"

static const char* TEST_PATH_VARIABLE = "SLIDEIO_TEST_DATA_PATH";
static const char* PRIV_TEST_PATH_VARIABLE = "SLIDEIO_TEST_DATA_PRIV_PATH";


bool TestTools::isPrivateTestEnabled()
{
    const char* var = getenv(PRIV_TEST_PATH_VARIABLE);
    return var != nullptr;
}

std::string TestTools::getTestImageDirectory(bool priv)
{
    const char *varName = priv ? PRIV_TEST_PATH_VARIABLE : TEST_PATH_VARIABLE;
    const char* var = getenv(varName);
    if(var==nullptr)
        throw std::runtime_error(
            std::string("Undefined environment variable: " + std::string(varName)));
    std::string testDirPath(var);
    return testDirPath;
}


std::string TestTools::getTestImagePath(const std::string& subfolder, const std::string& image, bool priv)
{
    std::string imagePath(getTestImageDirectory(priv));
    if(!subfolder.empty())
        imagePath += std::string("/") + subfolder;
    imagePath += std::string("/") +  image;
    return imagePath;
}


