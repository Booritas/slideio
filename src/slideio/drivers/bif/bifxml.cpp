// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#include "slideio/drivers/bif/bifxml.hpp"
#include "slideio/core/exceptions.hpp"

#include <tinyxml2.h>
#include <cstring>
#include <string>

namespace slideio::bif
{
    namespace
    {
        using tinyxml2::XMLDocument;
        using tinyxml2::XMLElement;

        // Parsing stops at the first NUL: the stitching packet is padded with NUL bytes.
        tinyxml2::XMLError parseDocument(XMLDocument& doc, const std::string& xml)
        {
            const std::string text(xml.c_str());
            return doc.Parse(text.c_str(), text.size());
        }

        double readDouble(const XMLElement* element, const char* name, double defaultValue)
        {
            const char* value = element->Attribute(name);
            if (value == nullptr) {
                return defaultValue;
            }
            size_t pos = 0;
            double result = 0;
            try {
                result = std::stod(value, &pos);
            }
            catch (const std::exception&) {
                pos = 0;
            }
            if (pos == 0) {
                RAISE_RUNTIME_ERROR << "BIF: cannot parse attribute " << name << " value '" << value << "'";
            }
            return result;
        }

        int readInt(const XMLElement* element, const char* name, int defaultValue)
        {
            const char* value = element->Attribute(name);
            if (value == nullptr) {
                return defaultValue;
            }
            size_t pos = 0;
            int result = 0;
            try {
                result = std::stoi(value, &pos);
            }
            catch (const std::exception&) {
                pos = 0;
            }
            if (pos == 0) {
                RAISE_RUNTIME_ERROR << "BIF: cannot parse attribute " << name << " value '" << value << "'";
            }
            return result;
        }

        // Integer suffix of a name such as "AOI3"; -1 when there is none.
        int nameSuffix(const char* name)
        {
            const size_t length = std::strlen(name);
            size_t start = length;
            while (start > 0 && name[start - 1] >= '0' && name[start - 1] <= '9') {
                --start;
            }
            if (start == length || length - start > 9) {
                return -1;
            }
            return std::stoi(std::string(name + start));
        }

        std::string serialiseShallow(const XMLElement* element)
        {
            XMLDocument doc;
            XMLElement* clone = doc.NewElement(element->Name());
            for (const tinyxml2::XMLAttribute* attr = element->FirstAttribute(); attr; attr = attr->Next()) {
                clone->SetAttribute(attr->Name(), attr->Value());
            }
            doc.InsertEndChild(clone);
            tinyxml2::XMLPrinter printer(nullptr, true);
            clone->Accept(&printer);
            return printer.CStr();
        }

        Joint parseJoint(const XMLElement* element)
        {
            Joint joint;
            joint.joined = readInt(element, "FlagJoined", 0) != 0;
            joint.confidence = readInt(element, "Confidence", 0);
            joint.tile1 = readInt(element, "Tile1", 0);
            joint.tile2 = readInt(element, "Tile2", 0);
            joint.overlapX = readDouble(element, "OverlapX", 0);
            joint.overlapY = readDouble(element, "OverlapY", 0);
            if (joint.usable()) {
                const char* direction = element->Attribute("Direction");
                const std::string dir = direction ? direction : "";
                if (dir == "RIGHT" || dir == "LEFT") {
                    joint.axis = JointAxis::Horizontal;
                }
                else if (dir == "UP") {
                    joint.axis = JointAxis::Vertical;
                }
                else {
                    RAISE_RUNTIME_ERROR << "BIF: unsupported joint direction '" << dir << "'";
                }
            }
            return joint;
        }
    }

    AcquisitionInfo parseAcquisitionXml(const std::string& xml)
    {
        XMLDocument doc;
        const XMLElement* scan = nullptr;
        if (parseDocument(doc, xml) == tinyxml2::XML_SUCCESS) {
            const XMLElement* root = doc.RootElement();
            if (root && std::strcmp(root->Name(), "iScan") == 0) {
                scan = root;
            }
            else if (root && std::strcmp(root->Name(), "Metadata") == 0) {
                scan = root->FirstChildElement("iScan");
            }
        }
        if (scan == nullptr) {
            RAISE_RUNTIME_ERROR << "BIF: not a Ventana BIF file (no iScan metadata in IFD 0)";
        }
        AcquisitionInfo info;
        info.scanResMicrons = readDouble(scan, "ScanRes", 0);
        info.magnification = readDouble(scan, "Magnification", 0);
        info.zLayers = readInt(scan, "Z-layers", 1);
        const char* model = scan->Attribute("ScannerModel");
        if (model) {
            info.scannerModel = model;
        }
        return info;
    }

    cv::Point snakePosition(int tileNumber, int cols, int rows)
    {
        if (cols <= 0 || rows <= 0 || tileNumber < 1 || tileNumber > cols * rows) {
            RAISE_RUNTIME_ERROR << "BIF: tile number " << tileNumber << " is outside the grid of "
                << cols << "x" << rows << " tiles";
        }
        const int n = tileNumber - 1;
        const int fromBottom = n / cols;
        const int offset = n % cols;
        const int col = (fromBottom % 2 == 0) ? offset : cols - 1 - offset;
        return cv::Point(col, rows - 1 - fromBottom);
    }

    StitchInfo parseStitchXml(const std::string& xml)
    {
        XMLDocument doc;
        if (parseDocument(doc, xml) != tinyxml2::XML_SUCCESS) {
            RAISE_RUNTIME_ERROR << "BIF: cannot parse stitching XML: " << doc.ErrorStr();
        }
        const XMLElement* root = doc.RootElement();
        if (root == nullptr || std::strcmp(root->Name(), "EncodeInfo") != 0) {
            RAISE_RUNTIME_ERROR << "BIF: stitching XML does not have an EncodeInfo root";
        }
        const XMLElement* stitch = root->FirstChildElement("SlideStitchInfo");
        const XMLElement* origins = root->FirstChildElement("AoiOrigin");
        if (stitch == nullptr || origins == nullptr) {
            RAISE_RUNTIME_ERROR << "BIF: stitching XML lacks SlideStitchInfo or AoiOrigin";
        }
        std::vector<const XMLElement*> infos;
        for (const XMLElement* e = stitch->FirstChildElement("ImageInfo"); e; e = e->NextSiblingElement("ImageInfo")) {
            infos.push_back(e);
        }
        std::vector<const XMLElement*> originElements;
        for (const XMLElement* e = origins->FirstChildElement(); e; e = e->NextSiblingElement()) {
            originElements.push_back(e);
        }
        if (infos.size() != originElements.size()) {
            RAISE_RUNTIME_ERROR << "BIF: " << infos.size() << " ImageInfo elements but "
                << originElements.size() << " AoiOrigin entries";
        }

        StitchInfo result;
        for (size_t i = 0; i < infos.size(); ++i) {
            const XMLElement* info = infos[i];
            const XMLElement* origin = originElements[i];
            Aoi aoi;
            const int suffix = nameSuffix(origin->Name());
            if (info->Attribute("AOIIndex") != nullptr) {
                aoi.index = readInt(info, "AOIIndex", 0);
                if (suffix >= 0 && suffix != aoi.index) {
                    RAISE_RUNTIME_ERROR << "BIF: AOIIndex " << aoi.index << " disagrees with " << origin->Name();
                }
            }
            else {
                aoi.index = suffix >= 0 ? suffix : static_cast<int>(i);
            }
            aoi.scanned = readInt(info, "AOIScanned", 1) != 0;
            aoi.originX = readInt(origin, "OriginX", 0);
            aoi.originY = readInt(origin, "OriginY", 0);
            aoi.tileWidth = readInt(info, "Width", 0);
            aoi.tileHeight = readInt(info, "Height", 0);
            aoi.cols = readInt(info, "NumCols", 0);
            aoi.rows = readInt(info, "NumRows", 0);
            aoi.imageInfoXml = serialiseShallow(info);
            if (aoi.scanned) {
                for (const XMLElement* j = info->FirstChildElement("TileJointInfo"); j;
                    j = j->NextSiblingElement("TileJointInfo")) {
                    const Joint joint = parseJoint(j);
                    if (!joint.usable()) {
                        continue;
                    }
                    const cv::Point p1 = snakePosition(joint.tile1, aoi.cols, aoi.rows);
                    const cv::Point p2 = snakePosition(joint.tile2, aoi.cols, aoi.rows);
                    const cv::Point expected = joint.axis == JointAxis::Horizontal
                        ? cv::Point(p1.x + 1, p1.y) : cv::Point(p1.x, p1.y - 1);
                    if (p2 != expected) {
                        RAISE_RUNTIME_ERROR << "BIF: AOI " << aoi.index << " joint " << joint.tile1 << "->"
                            << joint.tile2 << " does not join neighbouring tiles";
                    }
                    aoi.joints.push_back(joint);
                }
            }
            result.aois.push_back(std::move(aoi));
        }
        return result;
    }
}
