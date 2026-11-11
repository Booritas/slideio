// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#include "slideio/drivers/lif/lifcontainer.hpp"
#include "slideio/core/exceptions.hpp"
#include "slideio/core/log.hpp"
#include "slideio/core/tools/filereader.hpp"
#include "slideio/core/tools/tools.hpp"

#include <tinyxml2.h>
#include <algorithm>
#include <cstring>
#include <limits>
#include <map>

using namespace slideio;
using namespace slideio::lif;

namespace
{
    constexpr uint32_t kHeaderMagic = 0x70;
    constexpr uint8_t kMarker = 0x2A;
    constexpr uint64_t kXmlOffset = 13;     // 0x70, size, 0x2A, character count
    constexpr uint32_t kMaxProbeIdChars = 4096;

    // a + b, or false if it does not fit in 64 bits
    bool addChecked(uint64_t a, uint64_t b, uint64_t& out)
    {
        if (a > std::numeric_limits<uint64_t>::max() - b) {
            return false;
        }
        out = a + b;
        return true;
    }

    uint32_t readU32(const FileReader& file, uint64_t offset)
    {
        uint8_t b[4];
        file.readAt(offset, b, sizeof(b));
        return static_cast<uint32_t>(b[0]) | (static_cast<uint32_t>(b[1]) << 8) |
            (static_cast<uint32_t>(b[2]) << 16) | (static_cast<uint32_t>(b[3]) << 24);
    }

    uint64_t readU64(const FileReader& file, uint64_t offset)
    {
        uint8_t b[8];
        file.readAt(offset, b, sizeof(b));
        uint64_t v = 0;
        for (int i = 7; i >= 0; --i) {
            v = (v << 8) | b[i];
        }
        return v;
    }

    uint64_t readLength(const FileReader& file, uint64_t offset, int width)
    {
        return width == 4 ? readU32(file, offset) : readU64(file, offset);
    }

    std::string decodeUtf16(const FileReader& file, uint64_t offset, uint64_t chars)
    {
        std::vector<uint8_t> raw(static_cast<size_t>(chars * 2));
        if (!raw.empty()) {
            file.readAt(offset, raw.data(), raw.size());
        }
        std::u16string text(static_cast<size_t>(chars), u'\0');
        for (size_t i = 0; i < text.size(); ++i) {
            text[i] = static_cast<char16_t>(raw[2 * i] | (raw[2 * i + 1] << 8));
        }
        return Tools::fromUnicode16(text);
    }

    struct Record
    {
        bool valid = false;
        std::string id;
        uint64_t payloadOffset = 0;
        uint64_t length = 0;
        uint64_t end = 0;    // position after the declared payload (may pass EOF)
    };

    // Parses one block record at `pos` with the given length width. valid is
    // false when the framing does not hold. The id is decoded only if decodeId.
    Record parseRecord(const FileReader& file, uint64_t pos, int width, uint64_t maxIdChars, bool decodeId)
    {
        Record r;
        const uint64_t size = file.size();
        const uint64_t fixed = 4 + 4 + 1 + width + 1 + 4;   // up to and including the id length
        uint64_t idStart = 0;
        if (!addChecked(pos, fixed, idStart) || idStart > size) {
            return r;
        }
        if (readU32(file, pos) != kHeaderMagic) {
            return r;
        }
        uint8_t m1 = 0;
        uint8_t m2 = 0;
        file.readAt(pos + 8, &m1, 1);
        file.readAt(pos + 9 + width, &m2, 1);
        if (m1 != kMarker || m2 != kMarker) {
            return r;
        }
        r.length = readLength(file, pos + 9, width);
        const uint64_t idChars = readU32(file, pos + 10 + width);
        if (idChars == 0 || idChars >= maxIdChars) {
            return r;
        }
        uint64_t payload = 0;
        if (!addChecked(idStart, idChars * 2, payload) || payload > size) {
            return r;
        }
        r.payloadOffset = payload;
        if (!addChecked(payload, r.length, r.end)) {
            r.end = std::numeric_limits<uint64_t>::max();
        }
        if (decodeId) {
            r.id = decodeUtf16(file, idStart, idChars);
        }
        r.valid = true;
        return r;
    }

    // FD 4: is `width` a plausible length width for the first block at `pos`?
    // The identifier count must be sane, the payload inside the file, and what
    // follows must be EOF, a zero u32, or another header.
    bool probeWidth(const FileReader& file, uint64_t pos, int width)
    {
        const uint64_t size = file.size();
        const Record r = parseRecord(file, pos, width, kMaxProbeIdChars, false);
        if (!r.valid || r.end > size) {
            return false;
        }
        if (r.end == size) {
            return true;
        }
        if (r.end + 4 > size) {
            return false;
        }
        const uint32_t next = readU32(file, r.end);
        if (next == 0) {
            return true;
        }
        if (next == kHeaderMagic && r.end + 9 <= size) {
            uint8_t marker = 0;
            file.readAt(r.end + 8, &marker, 1);
            return marker == kMarker;
        }
        return false;
    }

    int chooseWidth(const FileReader& file, uint64_t pos, int version)
    {
        if (version == 1) {
            return 4;
        }
        if (version == 2) {
            return 8;
        }
        // No block at all: nothing to probe, and nothing the width could change.
        if (pos + 4 > file.size() || readU32(file, pos) == 0) {
            return 8;
        }
        const bool ok4 = probeWidth(file, pos, 4);
        const bool ok8 = probeWidth(file, pos, 8);
        if (ok4 == ok8) {
            RAISE_RUNTIME_ERROR << "LIF: container version " << version
                << " is unknown and the block length width cannot be determined ("
                << (ok4 ? "both" : "neither") << " of 4 and 8 bytes fit)";
        }
        return ok8 ? 8 : 4;
    }

    // Parses the description; on a failure or a document without a single root,
    // retries inside a synthetic <LEICA> root. `wrapped` tells which one parsed.
    bool parseXml(tinyxml2::XMLDocument& doc, const std::string& xml, bool& wrapped)
    {
        wrapped = false;
        std::string text = xml;
        while (!text.empty() && text.back() == '\0') {
            text.pop_back();   // a terminator would be read as content
        }
        if (doc.Parse(text.c_str(), text.size()) == tinyxml2::XML_SUCCESS) {
            const tinyxml2::XMLElement* root = doc.RootElement();
            if (root && !root->NextSiblingElement()) {
                return true;
            }
        }
        const std::string wrappedText = "<LEICA>" + text + "</LEICA>";
        doc.Clear();
        if (doc.Parse(wrappedText.c_str(), wrappedText.size()) == tinyxml2::XML_SUCCESS && doc.RootElement()) {
            wrapped = true;
            return true;
        }
        return false;
    }
}

const MemoryBlock* Container::findBlock(const std::string& id) const
{
    for (const MemoryBlock& block : m_blocks) {
        if (block.id == id) {
            return &block;
        }
    }
    return nullptr;
}

Container Container::read(const FileReader& file)
{
    const uint64_t size = file.size();
    if (size < kXmlOffset || readU32(file, 0) != kHeaderMagic) {
        RAISE_RUNTIME_ERROR << "LIF: not a Leica LIF file (no 0x70 header marker)";
    }
    uint8_t marker = 0;
    file.readAt(8, &marker, 1);
    if (marker != kMarker) {
        RAISE_RUNTIME_ERROR << "LIF: not a Leica LIF file (no 0x2A marker at offset 8)";
    }
    const uint64_t xmlChars = readU32(file, 9);
    if (xmlChars == 0) {
        RAISE_RUNTIME_ERROR << "LIF: not a Leica LIF file (empty XML description)";
    }
    uint64_t blocksStart = 0;
    if (!addChecked(kXmlOffset, xmlChars * 2, blocksStart) || blocksStart > size) {
        RAISE_RUNTIME_ERROR << "LIF: not a Leica LIF file (XML description of " << xmlChars
            << " characters does not fit in the file)";
    }

    Container c;
    c.m_xml = decodeUtf16(file, kXmlOffset, xmlChars);

    tinyxml2::XMLDocument doc;
    bool wrapped = false;
    if (!parseXml(doc, c.m_xml, wrapped)) {
        RAISE_RUNTIME_ERROR << "LIF: malformed XML description";
    }
    // The element carrying the container identity: the document root, or its
    // first child when the root is the synthetic wrapper.
    const tinyxml2::XMLElement* identity = doc.RootElement();
    if (wrapped && identity) {
        identity = identity->FirstChildElement();
    }
    if (!identity) {
        RAISE_RUNTIME_ERROR << "LIF: malformed XML description";
    }
    if (std::strcmp(identity->Name(), "LMS_Object_File") == 0) {
        RAISE_RUNTIME_ERROR << "LIF: not a Leica LIF file (a LOF object file)";
    }
    if (std::strcmp(identity->Name(), "LMSDataContainerHeader") == 0) {
        c.m_version = identity->IntAttribute("Version", 0);
    }

    c.m_lengthFieldWidth = chooseWidth(file, blocksStart, c.m_version);
    const int width = c.m_lengthFieldWidth;

    std::map<std::string, size_t> firstIndex;
    uint64_t pos = blocksStart;
    while (pos < size) {
        if (pos + 4 > size) {
            SLIDEIO_LOG(WARNING) << "LIF: " << (size - pos) << " stray bytes at the end of the file";
            break;
        }
        if (readU32(file, pos) == 0) {
            // The zero trailer; whatever follows should be zero too.
            uint64_t p = pos + 4;
            bool nonZero = false;
            std::vector<uint8_t> buf(4096);
            while (p < size && !nonZero) {
                const size_t n = static_cast<size_t>(std::min<uint64_t>(buf.size(), size - p));
                file.readAt(p, buf.data(), n);
                nonZero = std::any_of(buf.begin(), buf.begin() + n, [](uint8_t b) { return b != 0; });
                p += n;
            }
            if (nonZero) {
                SLIDEIO_LOG(WARNING) << "LIF: non-zero bytes after the end-of-blocks trailer ignored";
            }
            break;
        }
        const Record r = parseRecord(file, pos, width, std::numeric_limits<uint32_t>::max(), true);
        if (!r.valid) {
            SLIDEIO_LOG(WARNING) << "LIF: malformed memory block header at offset " << pos
                << "; remaining blocks ignored";
            break;
        }
        MemoryBlock block;
        block.id = r.id;
        block.offset = r.payloadOffset;
        block.length = r.length;
        block.truncated = r.end > size;
        const auto it = firstIndex.find(block.id);
        if (it != firstIndex.end()) {
            ++c.m_blocks[it->second].occurrences;
            SLIDEIO_LOG(INFO) << "LIF: memory block id \"" << block.id << "\" occurs more than once";
        }
        else {
            firstIndex[block.id] = c.m_blocks.size();
        }
        c.m_blocks.push_back(block);
        if (block.truncated) {
            SLIDEIO_LOG(WARNING) << "LIF: memory block \"" << block.id << "\" runs past the end of the file";
            break;
        }
        pos = r.end;
    }
    return c;
}
