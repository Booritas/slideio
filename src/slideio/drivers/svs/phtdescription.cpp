// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#include "slideio/drivers/svs/phtdescription.hpp"

#include <algorithm>
#include <cctype>
#include <cstring>
#include <sstream>

#include "slideio/base/exceptions.hpp"

using namespace slideio;

namespace
{
    const char* ATTRIBUTE_TAG = "Attribute";
    const char* ARRAY_TAG = "Array";
    const char* DATA_OBJECT_TAG = "DataObject";
    const char* OBJECT_TYPE_PROPERTY = "ObjectType";
    const char* NAME_PROPERTY = "Name";
    const char* GROUP_PROPERTY = "Group";
    const char* ELEMENT_PROPERTY = "Element";

    // Group and element ids are hexadecimal numbers that may be written
    // with either case ("0x115D" or "0x115d") depending on the scanner software.
    bool equalIgnoreCase(const char* text, const std::string& value) {
        if (text == nullptr) {
            return false;
        }
        const size_t length = std::strlen(text);
        if (length != value.size()) {
            return false;
        }
        for (size_t index = 0; index < length; ++index) {
            if (std::tolower(static_cast<unsigned char>(text[index]))
                != std::tolower(static_cast<unsigned char>(value[index]))) {
                return false;
            }
        }
        return true;
    }

    // Values of array types are stored as blank separated quoted tokens,
    // e.g. "0.000226891" "0.000226907". Quotes are not a part of the value.
    std::string trimValue(const char* text) {
        if (text == nullptr) {
            return std::string();
        }
        std::string value(text);
        const auto begin = value.find_first_not_of(" \t\r\n\"");
        if (begin == std::string::npos) {
            return std::string();
        }
        const auto end = value.find_last_not_of(" \t\r\n\"");
        return value.substr(begin, end - begin + 1);
    }

    const tinyxml2::XMLElement* findAttribute(const tinyxml2::XMLElement* element,
                                              const PHTDescription::Attribute& attribute) {
        if (element == nullptr) {
            return nullptr;
        }
        for (const tinyxml2::XMLElement* child = element->FirstChildElement(ATTRIBUTE_TAG);
             child != nullptr;
             child = child->NextSiblingElement(ATTRIBUTE_TAG)) {
            // Empty <Attribute/> elements are present in the metadata of some scanners.
            const char* name = child->Attribute(NAME_PROPERTY);
            if (name == nullptr || attribute.Name != name) {
                continue;
            }
            if (!equalIgnoreCase(child->Attribute(GROUP_PROPERTY), attribute.Group)) {
                continue;
            }
            if (!equalIgnoreCase(child->Attribute(ELEMENT_PROPERTY), attribute.Element)) {
                continue;
            }
            return child;
        }
        return nullptr;
    }

    const tinyxml2::XMLElement* getAttributeElement(const tinyxml2::XMLElement* element,
                                                    const PHTDescription::Attribute& attribute) {
        const tinyxml2::XMLElement* attributeElement = findAttribute(element, attribute);
        if (attributeElement == nullptr) {
            RAISE_RUNTIME_ERROR << "PHTDescription: attribute " << attribute.Name
                << " (" << attribute.Group << "," << attribute.Element << ") is not found.";
        }
        return attributeElement;
    }
}

PHTDescription::PHTDescription(const std::string& description) : m_doc(new tinyxml2::XMLDocument) {
    const tinyxml2::XMLError error = m_doc->Parse(description.c_str(), description.size());
    if (error != tinyxml2::XML_SUCCESS) {
        RAISE_RUNTIME_ERROR << "PHTDescription: error " << error << " by parsing of philips xml metadata.";
    }
    if (m_doc->RootElement() == nullptr) {
        RAISE_RUNTIME_ERROR << "PHTDescription: philips xml metadata does not have a root element.";
    }
}

PHTDescription::~PHTDescription() = default;

PHTDescription::PHTDescription(PHTDescription&& other) noexcept = default;

PHTDescription& PHTDescription::operator=(PHTDescription&& other) noexcept = default;

bool PHTDescription::isPhilipsDescription(const std::string& description) {
    // The cheap search comes first: it rejects the description of any other tiff flavour
    // without building a dom, and a philips description can be large (844 KB in
    // Philips-2.tiff, which embeds the macro image as base64).
    if (description.find(DP_UFS_IMPORT) == std::string::npos) {
        return false;
    }
    tinyxml2::XMLDocument doc;
    if (doc.Parse(description.c_str(), description.size()) != tinyxml2::XML_SUCCESS) {
        return false;
    }
    // A document with no root element parses without error; it is not philips metadata.
    const tinyxml2::XMLElement* root = doc.RootElement();
    if (root == nullptr || root->Name() == nullptr || std::strcmp(root->Name(), DATA_OBJECT_TAG) != 0) {
        return false;
    }
    const char* objectType = root->Attribute(OBJECT_TYPE_PROPERTY);
    return objectType != nullptr && DP_UFS_IMPORT == objectType;
}

tinyxml2::XMLElement* PHTDescription::getRoot() {
    if (!m_doc) {
        RAISE_RUNTIME_ERROR << "PHTDescription: philips xml metadata is not available.";
    }
    tinyxml2::XMLElement* root = m_doc->RootElement();
    if (root == nullptr) {
        RAISE_RUNTIME_ERROR << "PHTDescription: philips xml metadata does not have a root element.";
    }
    return root;
}

// Data objects are grouped by object type in arrays of the attributes of the parent object:
// <DataObject><Attribute Name="..." PMSVR="IDataObjectArray"><Array>
//     <DataObject ObjectType="..."/>...
std::vector<tinyxml2::XMLElement*> PHTDescription::getObjectList(const tinyxml2::XMLElement* parent,
                                                                 const std::string& name) {
    if (parent == nullptr) {
        RAISE_RUNTIME_ERROR << "PHTDescription: cannot retrieve objects '" << name << "' of an undefined parent.";
    }
    std::vector<tinyxml2::XMLElement*> objects;
    for (const tinyxml2::XMLElement* attribute = parent->FirstChildElement(ATTRIBUTE_TAG);
         attribute != nullptr;
         attribute = attribute->NextSiblingElement(ATTRIBUTE_TAG)) {
        const tinyxml2::XMLElement* array = attribute->FirstChildElement(ARRAY_TAG);
        if (array == nullptr) {
            continue;
        }
        for (const tinyxml2::XMLElement* object = array->FirstChildElement(DATA_OBJECT_TAG);
             object != nullptr;
             object = object->NextSiblingElement(DATA_OBJECT_TAG)) {
            const char* objectType = object->Attribute(OBJECT_TYPE_PROPERTY);
            if (objectType != nullptr && name == objectType) {
                objects.push_back(const_cast<tinyxml2::XMLElement*>(object));
            }
        }
    }
    return objects;
}

bool PHTDescription::hasAttribute(const tinyxml2::XMLElement* element, const Attribute& attribute) {
    return findAttribute(element, attribute) != nullptr;
}

std::string PHTDescription::getAttributeText(const tinyxml2::XMLElement* element, const Attribute& attribute) {
    return trimValue(getAttributeElement(element, attribute)->GetText());
}

int PHTDescription::getAttributeInt(const tinyxml2::XMLElement* element, const Attribute& attribute) {
    const std::string text = getAttributeText(element, attribute);
    try {
        size_t processed = 0;
        const int value = std::stoi(text, &processed);
        if (processed != text.size()) {
            RAISE_RUNTIME_ERROR << "PHTDescription: unexpected characters in the value '" << text
                << "' of the integer attribute " << attribute.Name << ".";
        }
        return value;
    }
    catch (const std::logic_error&) {
        RAISE_RUNTIME_ERROR << "PHTDescription: cannot convert the value '" << text
            << "' of the attribute " << attribute.Name << " to an integer.";
    }
}

std::vector<double> PHTDescription::getAttributeDoubleList(const tinyxml2::XMLElement* element,
                                                           const Attribute& attribute) {
    const char* text = getAttributeElement(element, attribute)->GetText();
    std::vector<double> values;
    if (text == nullptr) {
        return values;
    }
    // The value is a list of quoted doubles, e.g. "0.000226891" "0.000226907".
    std::string list(text);
    std::replace(list.begin(), list.end(), '"', ' ');
    std::istringstream stream(list);
    double value = 0.;
    while (stream >> value) {
        values.push_back(value);
    }
    if (!stream.eof()) {
        RAISE_RUNTIME_ERROR << "PHTDescription: cannot convert the value '" << text
            << "' of the attribute " << attribute.Name << " to a list of doubles.";
    }
    return values;
}
