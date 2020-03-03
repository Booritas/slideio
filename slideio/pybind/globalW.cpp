#include "globalW.h"
#include <slideio/slideio.hpp>

std::shared_ptr<SlideW> openSlideW(const std::string& path, const std::string& driver)
{
    std::shared_ptr<slideio::Slide> slide = slideio::openSlide(path, driver);
    std::shared_ptr<SlideW> wrapper(new SlideW(slide));
    return wrapper;
}

std::vector<cv::String> getDriverIDsW()
{
    return slideio::getDriverIDs();
}
