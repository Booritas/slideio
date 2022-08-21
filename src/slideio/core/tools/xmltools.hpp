// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#pragma once

#include "slideio/core/slideio_core_def.hpp"
#include <string>
#include <tinyxml2.h>
#include <vector>

#if defined(_MSC_VER)
#pragma warning( push )
#pragma warning(disable: 4251)
#endif


namespace slideio
{
    class SLIDEIO_CORE_EXPORTS XMLTools
    {
    public:
        static int childNodeTextToInt(const tinyxml2::XMLNode* xmlParent, const char* childName, int defaultValue = -1);
        static const tinyxml2::XMLElement* getElementByPath(const tinyxml2::XMLNode* parent, const std::vector<std::string>& path);
    };
}


#if defined(_MSC_VER)
#pragma warning( pop )
#endif
