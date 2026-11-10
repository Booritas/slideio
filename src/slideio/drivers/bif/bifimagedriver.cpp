// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#include "slideio/drivers/bif/bifimagedriver.hpp"
#include "slideio/drivers/bif/bifslide.hpp"

std::string slideio::BIFImageDriver::getID() const
{
    return "BIF";
}

std::shared_ptr<slideio::CVSlide> slideio::BIFImageDriver::openFile(const std::string& filePath)
{
    return BIFSlide::openFile(filePath);
}

std::string slideio::BIFImageDriver::getFileSpecs() const
{
    return "*.bif";
}
