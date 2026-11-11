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
    // What may precede an XML declaration: whitespace and a UTF-8 byte order mark.
    const char* const kLeadingSpace = " \t\r\n\xEF\xBB\xBF";

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

    bool isSettingDefinition(const XMLElement* e)
    {
        return (is(e, "ATLConfocalSettingDefinition") || is(e, "ATLCameraSettingDefinition"))
            && e->Attribute("Magnification");
    }

    // First ATL*SettingDefinition carrying @Magnification anywhere below `node`, in
    // document order. Unlike findDescendant it does enter LDM_Block_Sequential_Master:
    // it is only used inside an LDM_Block_Sequential, where the master is one more copy
    // of the hardware setting, not a sequence image.
    const XMLElement* findSettingDefinition(const XMLElement* node)
    {
        for (const XMLElement* c = node->FirstChildElement(); c; c = c->NextSiblingElement()) {
            if (isSettingDefinition(c)) {
                return c;
            }
            if (const XMLElement* r = findSettingDefinition(c)) {
                return r;
            }
        }
        return nullptr;
    }

    // Magnification of the objective, from the image's hardware setting attachment.
    // Two shapes occur in the corpus:
    //  - Attachment Name="HardwareSetting" with a direct ATL*SettingDefinition child
    //    (AMR1, FRET, PR2729, FRAP, Project007, Intro). The direct child wins; the copy
    //    nested in LDM_Block_Sequential_Master is not looked at.
    //  - Attachment Name="HardwareSettingList" > HardwareSetting > LDM_Block_Sequential >
    //    LDM_Block_Sequential_List / _Master > ATLConfocalSettingDefinition, with no direct
    //    definition (IT4_27_02_15.lif). Then the first definition under an
    //    LDM_Block_Sequential is used. Assumption: the sequences of one acquisition share
    //    the objective, so every definition there carries the same Magnification (true
    //    of every image in IT4).
    void readMagnification(const XMLElement* image, ImageDescription& d)
    {
        for (const XMLElement* att = image->FirstChildElement("Attachment"); att;
             att = att->NextSiblingElement("Attachment")) {
            const std::string name = attr(att, "Name");
            if (name != "HardwareSetting" && name != "HardwareSettingList") {
                continue;
            }
            for (const XMLElement* e = att->FirstChildElement(); e; e = e->NextSiblingElement()) {
                if (isSettingDefinition(e)) {
                    d.magnification = toDouble(e->Attribute("Magnification"));
                    return;
                }
            }
            const XMLElement* sequential = findDescendant(att, [](const XMLElement* e) {
                return is(e, "LDM_Block_Sequential");
            });
            if (sequential) {
                if (const XMLElement* e = findSettingDefinition(sequential)) {
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

XmlDescription::XmlDescription(const std::string& xml) : m_doc(new tinyxml2::XMLDocument)
{
    // tinyxml2 stops at the first NUL, so whatever follows one is never parsed;
    // cut it here, or the wrapped retry below would lose its closing tag to it.
    const std::string text = xml.substr(0, xml.find('\0'));
    if (m_doc->Parse(text.c_str(), text.size()) == tinyxml2::XML_SUCCESS) {
        for (const XMLElement* e = m_doc->FirstChildElement(); e; e = e->NextSiblingElement()) {
            m_topLevel.push_back(e);
        }
        if (m_topLevel.empty()) {
            RAISE_RUNTIME_ERROR << "LIF: malformed XML description: no element";
        }
        return;
    }
    const std::string error = m_doc->ErrorStr();
    // tinyxml2 refuses a declaration anywhere but at the start of the document, so
    // it cannot go inside the wrapper.
    size_t body = text.find_first_not_of(kLeadingSpace);
    if (body != std::string::npos && text.compare(body, 5, "<?xml") == 0) {
        const size_t close = text.find("?>", body);
        body = close == std::string::npos ? body : close + 2;
    }
    const std::string wrapped = "<LEICA>" + (body == std::string::npos ? std::string() : text.substr(body))
        + "</LEICA>";
    m_doc->Clear();
    if (m_doc->Parse(wrapped.c_str(), wrapped.size()) == tinyxml2::XML_SUCCESS) {
        m_wrapped = true;
        for (const XMLElement* e = m_doc->RootElement()->FirstChildElement(); e; e = e->NextSiblingElement()) {
            m_topLevel.push_back(e);
        }
    }
    if (m_topLevel.empty()) {
        // The original text's diagnosis: the wrapper's would point into text the
        // file does not hold.
        RAISE_RUNTIME_ERROR << "LIF: malformed XML description: " << error;
    }
}

XmlDescription::~XmlDescription() = default;

std::vector<ImageDescription> lif::parseImages(const std::string& xml)
{
    return parseImages(XmlDescription(xml));
}

std::vector<ImageDescription> lif::parseImages(const XmlDescription& xml)
{
    std::vector<const XMLElement*> images;
    for (const XMLElement* top : xml.topLevel()) {
        collectImages(top, images);
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
        d.rawXml = rawXmlOf(owner);
        result.push_back(std::move(d));
    }
    return result;
}
