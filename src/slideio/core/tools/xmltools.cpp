// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#include "slideio/core/tools/xmltools.hpp"

#include <stdexcept>
#include <vector>
using namespace tinyxml2;
using namespace slideio;

int XMLTools::childNodeTextToInt(const tinyxml2::XMLNode* xmlParent, const char* childName, int defaultValue)
{
    if (xmlParent == nullptr)
        throw std::runtime_error("XMLTools: Invalid xml document");
    const XMLElement* xmlChild = xmlParent->FirstChildElement(childName);
    int value = defaultValue;
    if (xmlChild != nullptr)
        value = xmlChild->IntText(defaultValue);
    return value;
}

const XMLElement* XMLTools::getElementByPath(const XMLNode* parent, const std::vector<std::string>& path)
{
    const XMLElement* xmlCurrentElement = nullptr;
    const XMLNode* xmlCurrentNode = parent;
    for (const auto& tag : path)
    {
        xmlCurrentElement = xmlCurrentNode->FirstChildElement(tag.c_str());
        if (xmlCurrentElement == nullptr)
        {
            return nullptr;
        }
        xmlCurrentNode = xmlCurrentElement;
    }
    return xmlCurrentElement;
}

