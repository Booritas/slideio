#include "pyglobals.hpp"
#include <slideio/slideio.hpp>
#include <boost/format.hpp>

namespace py = pybind11;

std::shared_ptr<PySlide> pyOpenSlide(const std::string& path, const std::string& driver)
{
    std::shared_ptr<slideio::Slide> slide = slideio::openSlide(path, driver);
    if(!slide)
    {
        throw std::runtime_error(
            (boost::format("Cannot open file \"%1%\" with driver \"%2%\"") % path % driver).str());
    }
    PySlide* pySlide = new PySlide(slide);
    std::shared_ptr<PySlide> wrapper(pySlide);
    return wrapper;
}

std::vector<std::string> pyGetDriverIDs()
{
    return slideio::getDriverIDs();
}
