// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#include "slideio/drivers/nd2/nd2chunkfile.hpp"
#include "slideio/core/exceptions.hpp"
#include "slideio/core/log.hpp"
#include "slideio/core/tools/filereader.hpp"

#include <algorithm>
#include <cstring>
#include <limits>
#include <optional>

using namespace slideio;
using namespace slideio::nd2;

namespace
{
    constexpr uint32_t kMagic = 0x0ABECEDA;
    constexpr uint64_t kHeaderSize = 16;
    constexpr uint32_t kMaxNameLength = 8192;       // exclusive
    constexpr size_t kScanBlock = 4u << 20;
    constexpr size_t kScanOverlap = 16;
    const char kSignatureName[] = "ND2 FILE SIGNATURE CHUNK NAME01!";
    const char kMapName[] = "ND2 FILEMAP SIGNATURE NAME 0001!";
    const char kTrailerName[] = "ND2 CHUNK MAP SIGNATURE 0000001!";
    constexpr size_t kNameFieldSize = 32;           // length of the three names above
    constexpr uint64_t kTrailerSize = 40;
    const char kFramePrefix[] = "ImageDataSeq|";

    uint32_t le32(const uint8_t* p)
    {
        return uint32_t(p[0]) | uint32_t(p[1]) << 8 | uint32_t(p[2]) << 16 | uint32_t(p[3]) << 24;
    }

    uint64_t le64(const uint8_t* p)
    {
        return uint64_t(le32(p)) | uint64_t(le32(p + 4)) << 32;
    }

    /// a + b, or nullopt on overflow.
    std::optional<uint64_t> addChecked(uint64_t a, uint64_t b)
    {
        if (a > std::numeric_limits<uint64_t>::max() - b) {
            return std::nullopt;
        }
        return a + b;
    }

    /// End of a chunk's data, nullopt if the arithmetic overflows.
    std::optional<uint64_t> dataEnd(const ChunkHeader& h)
    {
        auto start = addChecked(h.offset, kHeaderSize + h.nameLength);
        if (!start) {
            return std::nullopt;
        }
        return addChecked(*start, h.dataLength);
    }

    bool dataInFile(const ChunkHeader& h, uint64_t fileSize)
    {
        auto end = dataEnd(h);
        return end && *end <= fileSize;
    }

    /// A header read from `offset` after the magic and name length checks, with the
    /// first `nameBytes` bytes of its name field (fewer if the field is shorter).
    /// nullopt: no plausible chunk here. The data is not checked against EOF;
    /// callers decide what that means.
    std::optional<ChunkHeader> readHeader(const FileReader& file, uint64_t offset,
                                          std::vector<uint8_t>* nameField, size_t nameBytes)
    {
        const uint64_t size = file.size();
        auto afterHeader = addChecked(offset, kHeaderSize);
        if (!afterHeader || *afterHeader > size) {
            return std::nullopt;
        }
        uint8_t raw[kHeaderSize];
        file.readAt(offset, raw, sizeof(raw));
        if (le32(raw) != kMagic) {
            return std::nullopt;
        }
        ChunkHeader h;
        h.offset = offset;
        h.nameLength = le32(raw + 4);
        h.dataLength = le64(raw + 8);
        if (h.nameLength == 0 || h.nameLength >= kMaxNameLength) {
            return std::nullopt;
        }
        if (*afterHeader + h.nameLength > size) {
            return std::nullopt;
        }
        if (nameField) {
            const size_t n = std::min<size_t>(h.nameLength, nameBytes);
            nameField->assign(n, 0);
            if (n > 0) {
                file.readAt(*afterHeader, nameField->data(), n);
            }
        }
        return h;
    }

    /// The `!`-terminated name at the start of a name field, including the `!`.
    /// Empty if there is no `!` or a byte before it is not printable ASCII.
    std::string nameOf(const std::vector<uint8_t>& field)
    {
        for (size_t i = 0; i < field.size(); ++i) {
            const uint8_t c = field[i];
            if (c == '!') {
                return std::string(field.begin(), field.begin() + i + 1);
            }
            if (c < 0x20 || c > 0x7E) {
                return std::string();
            }
        }
        return std::string();
    }

    /// Looks for a chunk header at `offset` the way the scan does: magic, plausible
    /// name length, a `!`-terminated name inside the name field, data inside the file.
    bool scanHeaderAt(const FileReader& file, uint64_t offset, ChunkHeader& header, std::string& name)
    {
        std::vector<uint8_t> field;
        auto h = readHeader(file, offset, &field, kMaxNameLength);
        if (!h) {
            return false;
        }
        name = nameOf(field);
        if (name.empty() || !dataInFile(*h, file.size())) {
            return false;
        }
        header = *h;
        return true;
    }

    struct Version
    {
        int major = 0;
        int minor = 0;
    };

    /// Checks the signature chunk at offset 0 and returns the version in it.
    Version readSignature(const FileReader& file)
    {
        std::vector<uint8_t> field;
        auto h = readHeader(file, 0, &field, kNameFieldSize);
        if (!h) {
            RAISE_RUNTIME_ERROR << "ND2: not a Nikon ND2 file (no chunk header at offset 0)";
        }
        if (h->nameLength != kNameFieldSize || h->dataLength != 64
            || field.size() != kNameFieldSize
            || std::memcmp(field.data(), kSignatureName, kNameFieldSize) != 0) {
            RAISE_RUNTIME_ERROR << "ND2: not a Nikon ND2 file (no signature chunk at offset 0)";
        }
        if (!dataInFile(*h, file.size())) {
            RAISE_RUNTIME_ERROR << "ND2: not a Nikon ND2 file (signature chunk runs past the end of the file)";
        }
        uint8_t data[64];
        file.readAt(h->dataOffset(), data, sizeof(data));
        const bool ok = std::memcmp(data, "Ver", 3) == 0
            && data[3] >= '0' && data[3] <= '9' && data[4] == '.'
            && data[5] >= '0' && data[5] <= '9';
        if (!ok) {
            RAISE_RUNTIME_ERROR << "ND2: not a Nikon ND2 file (no version string in the signature chunk)";
        }
        Version v;
        v.major = data[3] - '0';
        v.minor = data[5] - '0';
        return v;
    }

    struct MapEntry
    {
        std::string name;
        uint64_t offset = 0;
    };

    /// The entries of the chunk map, without the terminating one. nullopt if the
    /// map chunk cannot be located or parsed.
    std::optional<std::vector<MapEntry>> readMap(const FileReader& file)
    {
        const uint64_t size = file.size();
        if (size < kTrailerSize + kHeaderSize) {
            return std::nullopt;
        }
        uint8_t trailer[kTrailerSize];
        file.readAt(size - kTrailerSize, trailer, sizeof(trailer));
        if (std::memcmp(trailer, kTrailerName, kNameFieldSize) != 0) {
            return std::nullopt;
        }
        const uint64_t mapOffset = le64(trailer + kNameFieldSize);
        std::vector<uint8_t> field;
        auto h = readHeader(file, mapOffset, &field, kNameFieldSize);
        if (!h || field.size() != kNameFieldSize
            || std::memcmp(field.data(), kMapName, kNameFieldSize) != 0
            || !dataInFile(*h, size)) {
            return std::nullopt;
        }
        std::vector<uint8_t> data(static_cast<size_t>(h->dataLength));
        if (!data.empty()) {
            file.readAt(h->dataOffset(), data.data(), data.size());
        }
        std::vector<MapEntry> entries;
        size_t pos = 0;
        while (true) {
            // name up to and including '!'
            size_t end = pos;
            while (end < data.size() && data[end] != '!' && end - pos < 256) {
                ++end;
            }
            if (end >= data.size() || data[end] != '!') {
                return std::nullopt;
            }
            std::string name(data.begin() + pos, data.begin() + end + 1);
            pos = end + 1;
            if (name == kTrailerName) {
                return entries;   // padding after the terminating entry is not parsed
            }
            if (data.size() - pos < 16) {
                return std::nullopt;
            }
            MapEntry e;
            e.name = std::move(name);
            e.offset = le64(data.data() + pos);
            pos += 16;   // offset, then the size field, which is ignored
            entries.push_back(std::move(e));
        }
    }

    enum class Verdict { Good, Absent, Bad };

    /// Verifies one map entry: the magic and a name field that starts with the
    /// expected name. A chunk whose data runs past EOF is absent, not bad.
    Verdict verifyEntry(const FileReader& file, const MapEntry& entry, ChunkHeader& header)
    {
        std::vector<uint8_t> field;
        auto h = readHeader(file, entry.offset, &field, entry.name.size());
        if (!h || h->nameLength < entry.name.size()
            || field.size() != entry.name.size()
            || std::memcmp(field.data(), entry.name.data(), entry.name.size()) != 0) {
            return Verdict::Bad;
        }
        if (!dataInFile(*h, file.size())) {
            return Verdict::Absent;
        }
        header = *h;
        return Verdict::Good;
    }

    bool parseFrameName(const std::string& name, int64_t& index)
    {
        const size_t prefix = sizeof(kFramePrefix) - 1;
        if (name.size() < prefix + 2 || name.compare(0, prefix, kFramePrefix) != 0
            || name.back() != '!') {
            return false;
        }
        int64_t v = 0;
        for (size_t i = prefix; i + 1 < name.size(); ++i) {
            const char c = name[i];
            if (c < '0' || c > '9' || v > (std::numeric_limits<int64_t>::max() - 9) / 10) {
                return false;
            }
            v = v * 10 + (c - '0');
        }
        index = v;
        return true;
    }
}

uint64_t ChunkHeader::dataOffset() const
{
    auto v = addChecked(offset, kHeaderSize + nameLength);
    if (!v) {
        RAISE_RUNTIME_ERROR << "ND2: chunk offset " << offset << " overflows";
    }
    return *v;
}

ChunkFile ChunkFile::read(const FileReader& file)
{
    ChunkFile result;
    const Version version = readSignature(file);
    result.m_versionMajor = version.major;
    result.m_versionMinor = version.minor;
    if (version.major != 2 && version.major != 3) {
        RAISE_RUNTIME_ERROR << "ND2: unsupported ND2 version " << version.major << "." << version.minor;
    }

    bool needScan = false;
    auto entries = readMap(file);
    if (!entries) {
        SLIDEIO_LOG(WARNING) << "ND2: " << file.path() << ": chunk map is missing or unreadable; scanning the file";
        needScan = true;
    }
    else {
        for (const MapEntry& entry : *entries) {
            ChunkHeader header;
            switch (verifyEntry(file, entry, header)) {
            case Verdict::Good:
                result.m_chunks[entry.name] = header;
                break;
            case Verdict::Absent:
                SLIDEIO_LOG(WARNING) << "ND2: " << file.path() << ": chunk " << entry.name
                                     << " runs past the end of the file and is ignored";
                break;
            case Verdict::Bad:
                ++result.m_badMapEntries;
                break;
            }
        }
        if (result.m_badMapEntries > 0) {
            SLIDEIO_LOG(WARNING) << "ND2: " << file.path() << ": " << result.m_badMapEntries
                                 << " chunk map entries are stale; scanning the file";
            needScan = true;
        }
    }

    if (needScan) {
        result.m_recoveredByScan = true;
        result.m_chunks.clear();
        const uint64_t size = file.size();
        uint64_t scanFrom = 0;      // first offset a header may start at
        std::vector<uint8_t> block;
        uint64_t blockStart = 0;
        while (blockStart < size) {
            const size_t n = static_cast<size_t>(std::min<uint64_t>(kScanBlock, size - blockStart));
            block.resize(n);
            file.readAt(blockStart, block.data(), n);
            bool jumped = false;
            for (size_t i = 0; i + 4 <= n; ++i) {
                const uint64_t pos = blockStart + i;
                if (pos < scanFrom || le32(block.data() + i) != kMagic) {
                    continue;
                }
                ChunkHeader header;
                std::string name;
                if (!scanHeaderAt(file, pos, header, name)) {
                    continue;
                }
                if (result.m_chunks.count(name)) {
                    SLIDEIO_LOG(INFO) << "ND2: " << file.path() << ": chunk " << name
                                      << " found twice; keeping the one at " << pos;
                }
                result.m_chunks[name] = header;
                scanFrom = *dataEnd(header);
                blockStart = scanFrom;
                jumped = true;
                break;
            }
            if (jumped) {
                continue;
            }
            if (blockStart + n >= size) {
                break;
            }
            blockStart += n - kScanOverlap;
        }
    }

    result.indexFrames();
    return result;
}

void ChunkFile::indexFrames()
{
    m_frames.clear();
    for (const auto& kv : m_chunks) {
        int64_t index = 0;
        if (parseFrameName(kv.first, index)) {
            m_frames[index] = kv.second;
        }
    }
}

const ChunkHeader* ChunkFile::find(const std::string& name) const
{
    auto it = m_chunks.find(name);
    return it == m_chunks.end() ? nullptr : &it->second;
}

std::vector<uint8_t> ChunkFile::readData(const FileReader& file, const ChunkHeader& chunk) const
{
    if (!dataInFile(chunk, file.size())) {
        RAISE_RUNTIME_ERROR << "ND2: chunk at " << chunk.offset << " runs past the end of the file";
    }
    if (chunk.dataLength > std::numeric_limits<size_t>::max()) {
        RAISE_RUNTIME_ERROR << "ND2: chunk at " << chunk.offset << " is too large to read";
    }
    std::vector<uint8_t> data(static_cast<size_t>(chunk.dataLength));
    if (!data.empty()) {
        file.readAt(chunk.dataOffset(), data.data(), data.size());
    }
    return data;
}
