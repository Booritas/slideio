// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#pragma once
#include "slideio/drivers/czi/czi_api_def.hpp"

namespace tinyxml2
{
	class XMLElement;

}

class SLIDEIO_CZI_EXPORTS CZITools
{
public:
	static int channelCountFromPixelType(const tinyxml2::XMLElement* xmlPixelType);
};

