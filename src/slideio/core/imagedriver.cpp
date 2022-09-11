// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#include "slideio/core/imagedriver.hpp"
#include "slideio/core/tools/tools.hpp"

bool slideio::ImageDriver::canOpenFile(const std::string& filePath) const
{
    return slideio::Tools::matchPattern(filePath, getFileSpecs());
}
