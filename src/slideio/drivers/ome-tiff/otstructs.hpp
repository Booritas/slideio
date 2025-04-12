// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#pragma once
#include <memory>
#include <string>

#include "slideio/imagetools/tiffkeeper.hpp"

namespace tinyxml2
{
    class XMLElement;
    class XMLDocument;
}

namespace slideio
{
    namespace ometiff
    {
        struct ImageData
        {
            std::shared_ptr<tinyxml2::XMLDocument> doc;
            tinyxml2::XMLElement* imageXml;
            std::string imageId;
            std::string imageFilePath;
        };

		typedef std::vector<int> PlaneCoordinates;
    }
}
