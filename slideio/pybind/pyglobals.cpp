#include "pyglobals.hpp"
#include <slideio/slideio.hpp>

std::shared_ptr<PySlide> pyOpenSlide(const std::string& path, const std::string& driver)
{
    std::shared_ptr<slideio::CVSlide> slide = slideio::openSlide(path, driver);
    std::shared_ptr<PySlide> wrapper(new PySlide(slide));
    return wrapper;
}

std::vector<cv::String> pyGetDriverIDs()
{
    return slideio::getDriverIDs();
}
