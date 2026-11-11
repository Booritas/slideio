// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#pragma once

#include "slideio/drivers/lif/lif_api_def.hpp"
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace slideio
{
    class FileReader;

    namespace lif
    {
        class XmlDescription;

        /// One memory block of a LIF file: an identifier and the extent of its payload.
        struct MemoryBlock
        {
            std::string id;
            uint64_t offset = 0;      // payload start
            uint64_t length = 0;      // payload length
            bool truncated = false;   // declared payload runs past EOF
            int occurrences = 1;      // >1: the id is ambiguous
        };

        /// The file-level structure of a Leica LIF file: the XML description, as
        /// text and parsed, and the index of memory blocks. Holds no file handle;
        /// payloads are read by whoever owns the FileReader.
        class SLIDEIO_LIF_EXPORTS Container
        {
        public:
            /// Throws RuntimeError "LIF: ..." when the file is not a readable LIF.
            static Container read(const FileReader& file);
            /// The XML description, UTF-8.
            const std::string& xml() const { return m_xml; }
            /// The XML description parsed, the one parse of this open.
            const XmlDescription& description() const { return *m_description; }
            /// LMSDataContainerHeader@Version, 0 if absent.
            int version() const { return m_version; }
            /// Width of a block's length field: 4 or 8 bytes.
            int lengthFieldWidth() const { return m_lengthFieldWidth; }
            /// The first block with this id, nullptr if there is none.
            const MemoryBlock* findBlock(const std::string& id) const;
            /// Every block in file order, empty ones included.
            const std::vector<MemoryBlock>& blocks() const { return m_blocks; }

        private:
            std::string m_xml;
            std::shared_ptr<const XmlDescription> m_description;
            int m_version = 0;
            int m_lengthFieldWidth = 8;
            std::vector<MemoryBlock> m_blocks;
        };
    }
}
