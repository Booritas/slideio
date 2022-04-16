// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#include "czitools.hpp"
#include "slideio/ext/exceptions.hpp"
#include <tinyxml2.h>

int CZITools::channelCountFromPixelType(const tinyxml2::XMLElement* xmlPixelType)
{
    std::string pixelType = xmlPixelType->GetText();
    int channelCount = 0;
    if (pixelType.find("Gray") == 0) {
        channelCount = 1;
    }
    else if (pixelType.find("Bgra") == 0) {
        channelCount = 4;
    }
    else if (pixelType.find("Bgr") == 0) {
        channelCount = 3;
    }
    else
    {
        RAISE_RUNTIME_ERROR << "CZIImageDriver: unknown pixel type:" << pixelType;
    }
    return channelCount;
}

