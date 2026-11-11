// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#include "slideio/drivers/lif/lifimagedriver.hpp"
#include "slideio/drivers/lif/lifslide.hpp"

std::string slideio::LIFImageDriver::getID() const
{
    return "LIF";
}

std::shared_ptr<slideio::CVSlide> slideio::LIFImageDriver::openFile(const std::string& filePath)
{
    return LIFSlide::openFile(filePath);
}

std::string slideio::LIFImageDriver::getFileSpecs() const
{
    return "*.lif";
}
