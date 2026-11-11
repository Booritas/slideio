// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#include "slideio/drivers/lif/lifxml.hpp"
#include "slideio/drivers/lif/liftimestamps.hpp"
#include "slideio/core/exceptions.hpp"
#include "slideio/core/log.hpp"

#include <tinyxml2.h>
#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <set>

using namespace slideio;
using namespace slideio::lif;
using tinyxml2::XMLElement;

namespace
{
    bool is(const XMLElement* e, const char* name)
    {
        return e && std::strcmp(e->Value(), name) == 0;
    }

    const XMLElement* parentElement(const XMLElement* e)
    {
        const tinyxml2::XMLNode* p = e->Parent();
        return p ? p->ToElement() : nullptr;
    }

    std::string attr(const XMLElement* e, const char* name)
    {
        const char* v = e->Attribute(name);
        return v ? v : "";
    }

    double toDouble(const char* text, double fallback = 0)
    {
        if (!text || !*text) {
            return fallback;
        }
        char* end = nullptr;
        const double v = std::strtod(text, &end);
        return end == text ? fallback : v;
    }

    // Image elements in document order, skipping LDM_Block_Sequential_Master subtrees.
    void collectImages(const XMLElement* node, std::vector<const XMLElement*>& out)
    {
        for (const XMLElement* c = node->FirstChildElement(); c; c = c->NextSiblingElement()) {
            if (is(c, "LDM_Block_Sequential_Master")) {
                continue;
            }
            if (is(c, "Image")) {
                out.push_back(c);
            }
            collectImages(c, out);
        }
    }

    // First descendant satisfying pred, not entering sequential-master subtrees.
    template <class Pred>
    const XMLElement* findDescendant(const XMLElement* node, Pred pred)
    {
        for (const XMLElement* c = node->FirstChildElement(); c; c = c->NextSiblingElement()) {
            if (is(c, "LDM_Block_Sequential_Master")) {
                continue;
            }
            if (pred(c)) {
                return c;
            }
            if (const XMLElement* r = findDescendant(c, pred)) {
                return r;
            }
        }
        return nullptr;
    }

    std::string imagePath(const XMLElement* owner)
    {
        std::vector<std::string> names;
        for (const XMLElement* e = owner; e; e = parentElement(e)) {
            if (is(e, "Element")) {
                names.push_back(attr(e, "Name"));
            }
        }
        // names runs from the image outwards; the last one is the experiment.
        // An image that is itself the outermost element keeps its own name.
        if (names.size() > 1) {
            names.pop_back();
        }
        std::string path;
        for (auto it = names.rbegin(); it != names.rend(); ++it) {
            if (!path.empty()) {
                path += "/";
            }
            path += *it;
        }
        return path;
    }

    void readDimensions(const XMLElement* image, ImageDescription& d)
    {
        const XMLElement* dims = findDescendant(image, [](const XMLElement* e) { return is(e, "Dimensions"); });
        if (!dims) {
            return;
        }
        for (const XMLElement* e = dims->FirstChildElement("DimensionDescription"); e;
             e = e->NextSiblingElement("DimensionDescription")) {
            Dimension dim;
            dim.dimId = e->IntAttribute("DimID", 0);
            dim.count = e->Int64Attribute("NumberOfElements", 0);
            dim.bytesInc = e->Int64Attribute("BytesInc", 0);
            dim.origin = toDouble(e->Attribute("Origin"));
            dim.length = toDouble(e->Attribute("Length"));
            dim.unit = attr(e, "Unit");
            d.dimensions.push_back(dim);
        }
    }

    void readChannels(const XMLElement* image, ImageDescription& d)
    {
        const XMLElement* chans = findDescendant(image, [](const XMLElement* e) { return is(e, "Channels"); });
        if (!chans) {
            return;
        }
        for (const XMLElement* e = chans->FirstChildElement("ChannelDescription"); e;
             e = e->NextSiblingElement("ChannelDescription")) {
            Channel ch;
            ch.dataType = e->IntAttribute("DataType", -1);
            ch.resolution = e->IntAttribute("Resolution", 0);
            ch.bytesInc = e->Int64Attribute("BytesInc", 0);
            ch.channelTag = e->IntAttribute("ChannelTag", -1);
            for (const tinyxml2::XMLAttribute* a = e->FirstAttribute(); a; a = a->Next()) {
                ch.attributes.emplace_back(a->Name(), a->Value());
            }
            d.channels.push_back(std::move(ch));
        }
    }

    // HardwareSetting attachment, direct ATL*SettingDefinition child. The
    // definition nested in LDM_Block_Sequential_Master is deliberately not looked at.
    void readMagnification(const XMLElement* image, ImageDescription& d)
    {
        for (const XMLElement* att = image->FirstChildElement("Attachment"); att;
             att = att->NextSiblingElement("Attachment")) {
            if (attr(att, "Name") != "HardwareSetting") {
                continue;
            }
            for (const XMLElement* e = att->FirstChildElement(); e; e = e->NextSiblingElement()) {
                if ((is(e, "ATLConfocalSettingDefinition") || is(e, "ATLCameraSettingDefinition"))
                    && e->Attribute("Magnification")) {
                    d.magnification = toDouble(e->Attribute("Magnification"));
                    return;
                }
            }
        }
    }

    void readDelay(const XMLElement* image, ImageDescription& d)
    {
        const XMLElement* rec = findDescendant(image, [](const XMLElement* e) {
            return is(e, "ScannerSettingRecord") && attr(e, "Identifier") == "nDelayTime_s";
        });
        if (rec) {
            d.delayTimeSeconds = toDouble(rec->Attribute("Variant"));
        }
    }

    void readTiles(const XMLElement* image, ImageDescription& d)
    {
        for (const XMLElement* att = image->FirstChildElement("Attachment"); att;
             att = att->NextSiblingElement("Attachment")) {
            if (attr(att, "Name") != "TileScanInfo") {
                continue;
            }
            int index = 0;
            for (const XMLElement* t = att->FirstChildElement("Tile"); t; t = t->NextSiblingElement("Tile")) {
                d.tileFieldAttributes.emplace_back(
                    std::to_string(index++),
                    "FieldX=" + attr(t, "FieldX") + ";FieldY=" + attr(t, "FieldY") +
                    ";PosX=" + attr(t, "PosX") + ";PosY=" + attr(t, "PosY"));
            }
            return;
        }
    }

    // The owner serialised without its child Elements: the Children list is emptied.
    std::string rawXmlOf(const XMLElement* owner)
    {
        tinyxml2::XMLDocument doc;
        XMLElement* copy = owner->DeepClone(&doc)->ToElement();
        doc.InsertEndChild(copy);
        if (XMLElement* children = copy->FirstChildElement("Children")) {
            children->DeleteChildren();
        }
        tinyxml2::XMLPrinter printer(nullptr, true);
        copy->Accept(&printer);
        return printer.CStr();
    }
}

std::vector<ImageDescription> lif::parseImages(const std::string& xml)
{
    tinyxml2::XMLDocument doc;
    if (doc.Parse(xml.c_str(), xml.size()) != tinyxml2::XML_SUCCESS) {
        RAISE_RUNTIME_ERROR << "LIF: malformed XML description: " << doc.ErrorStr();
    }
    std::vector<const XMLElement*> images;
    if (const XMLElement* root = doc.RootElement()) {
        collectImages(root, images);
    }

    std::vector<ImageDescription> result;
    std::set<std::string> seen;
    for (const XMLElement* image : images) {
        const XMLElement* data = parentElement(image);
        const XMLElement* owner = data ? parentElement(data) : nullptr;
        if (!owner || is(owner, "ProcessingHistory") || !is(owner, "Element")) {
            continue;
        }
        const XMLElement* memory = owner->FirstChildElement("Memory");
        if (!memory) {
            continue;
        }
        const std::string id = attr(memory, "MemoryBlockID");
        const int64_t size = memory->Int64Attribute("Size", -1);
        if (id.empty() || size == 0) {
            continue;   // a container, not an image
        }
        if (!seen.insert(id).second) {
            SLIDEIO_LOG(INFO) << "LIF: image \"" << imagePath(owner) << "\" repeats memory block \""
                << id << "\"; the first description is used";
            continue;
        }
        ImageDescription d;
        d.path = imagePath(owner);
        d.memoryId = id;
        d.declaredSize = size;
        readDimensions(image, d);
        readChannels(image, d);
        d.timestampTicks = parseTimestampTicks(image);
        readDelay(image, d);
        readMagnification(image, d);
        readTiles(image, d);
        d.rawXml = rawXmlOf(owner);
        result.push_back(std::move(d));
    }
    return result;
}
