// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#pragma once

#include "slideio/drivers/lif/lif_api_def.hpp"
#include <cstdint>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#if defined(_MSC_VER)
#pragma warning( push )
#pragma warning(disable: 4251)
#endif

namespace tinyxml2
{
    class XMLDocument;
    class XMLElement;
}

namespace slideio
{
    namespace lif
    {
        /// One DimensionDescription.
        struct Dimension
        {
            int dimId = 0;
            int64_t count = 0;
            int64_t bytesInc = 0;
            double origin = 0;
            double length = 0;
            std::string unit;
        };

        /// One ChannelDescription.
        struct Channel
        {
            int dataType = -1;          // -1: attribute absent
            int resolution = 0;         // significant bits
            int64_t bytesInc = 0;
            int channelTag = -1;        // -1: absent
            /// UserDefName of the WFC scanner settings. No file of the test corpus
            /// carries it (spec 6.1), so it is not parsed and stays empty.
            std::string name;
            std::vector<std::pair<std::string, std::string>> attributes;   // every attribute, verbatim
        };

        /// What the XML says about one image.
        struct ImageDescription
        {
            std::string path;            // "Nest/CZMT": Element names, experiment omitted
            std::string memoryId;
            int64_t declaredSize = -1;   // Memory/@Size, -1 if absent
            std::vector<Dimension> dimensions;   // document order
            std::vector<Channel> channels;       // document order
            std::vector<uint64_t> timestampTicks;
            double delayTimeSeconds = 0;         // scanner nDelayTime_s, 0 if absent
            double magnification = 0;            // HardwareSetting ATL*SettingDefinition/@Magnification
            /// Owning Element without child Elements. Holds TileScanInfo whole: field j's
            /// record is its j-th Tile (spec 4.5, 6.4).
            std::string rawXml;
        };

        /// The XML description, parsed once (spec 4.2). A text that does not parse is
        /// retried inside a synthetic <LEICA> root, a leading XML declaration removed;
        /// a text that parses as it is is never wrapped, however many top-level
        /// elements it has.
        class SLIDEIO_LIF_EXPORTS XmlDescription
        {
        public:
            /// Throws RuntimeError "LIF: malformed XML description: <tinyxml2 error>".
            explicit XmlDescription(const std::string& xml);
            ~XmlDescription();
            XmlDescription(const XmlDescription&) = delete;
            XmlDescription& operator=(const XmlDescription&) = delete;
            /// The elements at the top of the document, in document order; the
            /// synthetic wrapper, when there is one, is not among them. Never empty.
            const std::vector<const tinyxml2::XMLElement*>& topLevel() const { return m_topLevel; }
            /// True when only the synthetic wrapper made the text parse.
            bool wrapped() const { return m_wrapped; }
        private:
            std::unique_ptr<tinyxml2::XMLDocument> m_doc;
            bool m_wrapped = false;
            std::vector<const tinyxml2::XMLElement*> m_topLevel;
        };

        /// Every image under every top-level element with its own memory block id, in
        /// document order (spec 4.3).
        SLIDEIO_LIF_EXPORTS std::vector<ImageDescription> parseImages(const XmlDescription& xml);
        /// Parses `xml` and does the same. Throws as XmlDescription does.
        SLIDEIO_LIF_EXPORTS std::vector<ImageDescription> parseImages(const std::string& xml);
    }
}

#if defined(_MSC_VER)
#pragma warning( pop )
#endif
