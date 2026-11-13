// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#include "slideio/drivers/nd2/nd2imagedriver.hpp"
#include "slideio/drivers/nd2/nd2slide.hpp"

std::string slideio::ND2ImageDriver::getID() const
{
    return "ND2";
}

std::shared_ptr<slideio::CVSlide> slideio::ND2ImageDriver::openFile(const std::string& filePath)
{
    return ND2Slide::openFile(filePath);
}

std::string slideio::ND2ImageDriver::getFileSpecs() const
{
    return "*.nd2";
}
