// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.org/license.html.
#include "slideio/slideio/slideio.hpp"
#include "slideio/imagetools/cvtools.hpp"
#include "slideio/slideio/imagedrivermanager.hpp"

using namespace slideio;


std::shared_ptr<slideio::Slide> slideio::openSlide(const std::string& path, const std::string& driver)
{
    std::shared_ptr<CVSlide> cvSlide = ImageDriverManager::openSlide(path, driver);
    std::shared_ptr<Slide> slide(new Slide(cvSlide));
    return slide;
}

std::vector<std::string> slideio::getDriverIDs()
{
    return ImageDriverManager::getDriverIDs();
}
