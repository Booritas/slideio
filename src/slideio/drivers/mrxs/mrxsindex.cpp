// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#include "slideio/drivers/mrxs/mrxsindex.hpp"
#include "slideio/drivers/mrxs/mrxserrors.hpp"
#include "slideio/core/tools/filereader.hpp"

#include <cstring>
#include <memory>
#include <optional>
#include <set>
#include <system_error>
#include <unordered_set>

using namespace slideio::mrxs;
namespace fs = std::filesystem;

namespace
{
    int32_t readLE32(const uint8_t* p)
    {
        const uint32_t v = uint32_t(p[0]) | (uint32_t(p[1]) << 8) | (uint32_t(p[2]) << 16) | (uint32_t(p[3]) << 24);
        int32_t result;
        std::memcpy(&result, &v, sizeof(result));
        return result;
    }

    // Checked, little-endian access to the index file. Every read is bounds-checked
    // against the file size before it is issued (design section 4).
    class IndexFile
    {
    public:
        explicit IndexFile(const fs::path& path) : m_name(path.u8string())
        {
            std::error_code ec;
            if (!fs::is_regular_file(path, ec)) {
                MRXS_CORRUPT << "index file " << m_name << " does not exist";
            }
            m_reader = std::make_unique<slideio::FileReader>(m_name);
        }
        int64_t size() const { return static_cast<int64_t>(m_reader->size()); }
        const std::string& name() const { return m_name; }

        void require(int64_t offset, int64_t length, const char* what) const
        {
            if (offset < 0 || length < 0 || offset > size() || length > size() - offset) {
                MRXS_CORRUPT << "index " << m_name << ": reading " << what << " (" << length
                    << " bytes at offset " << offset << ") would pass the end of the " << size() << "-byte file";
            }
        }
        std::vector<uint8_t> bytes(int64_t offset, int64_t length, const char* what) const
        {
            require(offset, length, what);
            std::vector<uint8_t> data(static_cast<size_t>(length));
            if (length > 0) {
                m_reader->readAt(static_cast<uint64_t>(offset), data.data(), data.size());
            }
            return data;
        }
        int32_t i32(int64_t offset, const char* what) const
        {
            const std::vector<uint8_t> data = bytes(offset, 4, what);
            return readLE32(data.data());
        }
        // A pointer field: zero means absent, a negative value is corrupt.
        int64_t pointer(int64_t offset, const char* what) const
        {
            const int32_t value = i32(offset, what);
            if (value < 0) {
                MRXS_CORRUPT << "index " << m_name << ": " << what << " at offset " << offset
                    << " is negative (" << value << ")";
            }
            return value;
        }
    private:
        std::string m_name;
        std::unique_ptr<slideio::FileReader> m_reader;
    };

    class StructureReader
    {
    public:
        StructureReader(SlideStructure& slide, const IndexBudget& budget)
            : m_slide(slide), m_meta(slide.metadata), m_budget(budget), m_index(slide.metadata.indexFile)
        {
            for (const fs::path& file : m_meta.dataFiles) {
                std::error_code ec;
                const auto size = fs::file_size(file, ec);
                if (ec || !fs::is_regular_file(file, ec)) {
                    MRXS_CORRUPT << "data file " << file.u8string() << " does not exist";
                }
                m_dataSizes.push_back(static_cast<int64_t>(size));
            }
        }

        void read()
        {
            readHeader();
            for (size_t l = 0; l < m_meta.levels.size(); ++l) {
                m_slide.levels.push_back(readLevel(static_cast<int>(l)));
            }
            readNonHierarchical();
        }

    private:
        void readHeader()
        {
            const std::string& id = m_meta.slideId;
            const int64_t headerSize = 5 + static_cast<int64_t>(id.size()) + 8;
            const std::vector<uint8_t> header = m_index.bytes(0, headerSize, "the index header");
            if (std::memcmp(header.data(), "01.02", 5) != 0) {
                MRXS_UNSUPPORTED << "index " << m_index.name() << " has version '"
                    << std::string(header.begin(), header.begin() + 5) << "' (supported: 01.02)";
            }
            if (std::memcmp(header.data() + 5, id.data(), id.size()) != 0) {
                MRXS_CORRUPT << "index " << m_index.name() << " does not carry SLIDE_ID " << id;
            }
            // The two table pointers follow the identifier, whatever its length.
            m_hierTable = m_index.pointer(5 + static_cast<int64_t>(id.size()), "the hierarchical table pointer");
            m_nonHierTable = m_index.pointer(9 + static_cast<int64_t>(id.size()), "the non-hierarchical table pointer");
            if (m_hierTable == 0) {
                MRXS_CORRUPT << "index " << m_index.name() << " has no hierarchical table";
            }
        }

        PayloadRef payload(int32_t fileNumber, int64_t offset, int64_t length, const std::string& what) const
        {
            if (fileNumber < 0 || fileNumber >= static_cast<int32_t>(m_dataSizes.size())) {
                MRXS_CORRUPT << what << " names data file " << fileNumber << " of " << m_dataSizes.size();
            }
            const int64_t size = m_dataSizes[fileNumber];
            if (offset < 0 || length <= 0 || offset > size || length > size - offset) {
                MRXS_CORRUPT << what << ": bytes [" << offset << ", " << offset + length << ") lie outside data file "
                    << m_meta.dataFiles[fileNumber].u8string() << " of " << size << " bytes";
            }
            return PayloadRef{fileNumber, offset, length};
        }

        std::vector<ImageRecord> readLevel(int level)
        {
            const int64_t concat = m_meta.levels[level].concat;
            const int64_t head = m_index.pointer(m_hierTable + 4 * int64_t(level), "a zoom level pointer");
            if (head == 0) {
                MRXS_UNSUPPORTED << "zoom level " << level << " is absent from the index";
            }
            if (m_index.i32(head, "a zoom level head") != 0) {
                MRXS_CORRUPT << "index " << m_index.name() << ": the head of zoom level " << level
                    << " at offset " << head << " is not an empty page";
            }
            int64_t page = m_index.pointer(head + 4, "a zoom level's first page pointer");
            std::vector<ImageRecord> records;
            std::unordered_set<int64_t> seenPages, seenImages;
            const int64_t gridSize = int64_t(m_meta.nx) * m_meta.ny;
            while (page != 0) {
                if (!seenPages.insert(page).second) {
                    MRXS_CORRUPT << "index " << m_index.name() << ": the page chain of zoom level " << level
                        << " returns to the page at offset " << page;
                }
                if (++m_pages > m_budget.maxPages) {
                    MRXS_LIMIT << "index " << m_index.name() << " has more than " << m_budget.maxPages << " pages";
                }
                const int32_t count = m_index.i32(page, "a page entry count");
                const int64_t next = m_index.pointer(page + 4, "a next-page pointer");
                if (count < 0) {
                    MRXS_CORRUPT << "index " << m_index.name() << ": the page at offset " << page
                        << " has a negative entry count";
                }
                m_records += count;
                if (m_records > m_budget.maxRecords) {
                    MRXS_LIMIT << "index " << m_index.name() << " has more than " << m_budget.maxRecords << " records";
                }
                const std::vector<uint8_t> entries = m_index.bytes(page + 8, 16 * int64_t(count), "a page's entries");
                for (int32_t k = 0; k < count; ++k) {
                    const uint8_t* e = entries.data() + 16 * k;
                    const int32_t imageIndex = readLE32(e);
                    const std::string what = "zoom level " + std::to_string(level) + " record " + std::to_string(k)
                        + " of the page at index offset " + std::to_string(page);
                    if (imageIndex < 0 || imageIndex >= gridSize) {
                        MRXS_CORRUPT << what << " has image index " << imageIndex << " outside the "
                            << m_meta.nx << " x " << m_meta.ny << " grid";
                    }
                    ImageRecord record;
                    record.imageIndex = imageIndex;
                    record.x = static_cast<int>(imageIndex % m_meta.nx);
                    record.y = static_cast<int>(imageIndex / m_meta.nx);
                    if (record.x % concat != 0 || record.y % concat != 0) {
                        MRXS_CORRUPT << what << " at (" << record.x << ", " << record.y
                            << ") is not aligned to the level's concatenation factor " << concat;
                    }
                    if (!seenImages.insert(imageIndex).second) {
                        MRXS_CORRUPT << what << " repeats image index " << imageIndex;
                    }
                    record.payload = payload(readLE32(e + 12), readLE32(e + 4), readLE32(e + 8), what);
                    records.push_back(record);
                }
                page = next;
            }
            if (records.empty()) {
                MRXS_UNSUPPORTED << "zoom level " << level << " has no images";
            }
            return records;
        }

        // The first entry of the first data page of a non-hierarchical value, or nothing
        // when the value is absent. Selected records must have zero tags (design section 4).
        std::optional<PayloadRef> firstEntry(int ordinal, const std::string& what) const
        {
            if (m_nonHierTable == 0) {
                return std::nullopt;
            }
            const int64_t head = m_index.pointer(m_nonHierTable + 4 * int64_t(ordinal), "a non-hierarchical pointer");
            if (head == 0) {
                return std::nullopt;
            }
            const int64_t page = m_index.pointer(head + 4, "a non-hierarchical first page pointer");
            if (page == 0) {
                return std::nullopt;
            }
            const int32_t count = m_index.i32(page, "a non-hierarchical entry count");
            if (count < 1) {
                return std::nullopt;
            }
            const std::vector<uint8_t> e = m_index.bytes(page + 8, 20, "a non-hierarchical entry");
            if (readLE32(e.data()) != 0 || readLE32(e.data() + 4) != 0) {
                MRXS_CORRUPT << what << " has nonzero tags in its first entry";
            }
            return payload(readLE32(e.data() + 16), readLE32(e.data() + 8), readLE32(e.data() + 12), what);
        }

        void readNonHierarchical()
        {
            int ordinal = 0;
            struct Known { const char* value; const char* typeKey; const char* name; };
            static const Known associated[] = {
                {"ScanDataLayer_SlideThumbnail", "THUMBNAIL_IMAGE_TYPE", "Macro"},
                {"ScanDataLayer_SlideBarcode", "BARCODE_IMAGE_TYPE", "Label"},
                {"ScanDataLayer_SlidePreview", "PREVIEW_IMAGE_TYPE", "Thumbnail"},
            };
            std::optional<PayloadRef> raw, zlib;
            for (const Family& family : m_meta.nonHierarchical) {
                for (size_t j = 0; j < family.values.size(); ++j, ++ordinal) {
                    const FamilyValue& value = family.values[j];
                    if (j == 0 && family.name == "VIMSLIDE_POSITION_BUFFER") {
                        raw = firstEntry(ordinal, "the position map");
                        if (!raw) MRXS_CORRUPT << "VIMSLIDE_POSITION_BUFFER has no record";
                    }
                    else if (j == 0 && family.name == "StitchingIntensityLayer") {
                        zlib = firstEntry(ordinal, "the compressed position map");
                        if (!zlib) MRXS_CORRUPT << "StitchingIntensityLayer has no record";
                    }
                    else if (family.name == "Scan data layer") {
                        for (const Known& known : associated) {
                            if (value.name != known.value) continue;
                            const std::optional<PayloadRef> ref = firstEntry(ordinal, value.name);
                            if (!ref) break;
                            const std::string* type = m_slide.ini.find(value.section, known.typeKey);
                            if (!type) {
                                MRXS_CORRUPT << value.name << " has no " << known.typeKey;
                            }
                            if (*type != "JPEG") {
                                MRXS_UNSUPPORTED << value.name << " is " << *type << " (supported: JPEG)";
                            }
                            m_slide.associated[known.name] = *ref;
                        }
                    }
                }
            }
            // The raw map wins when both exist (format description 7.3).
            if (raw) {
                m_slide.positionSource = PositionSource::Raw;
                m_slide.positions = *raw;
            }
            else if (zlib) {
                m_slide.positionSource = PositionSource::Zlib;
                m_slide.positions = *zlib;
            }
        }

        SlideStructure& m_slide;
        const SlideMetadata& m_meta;
        IndexBudget m_budget;
        IndexFile m_index;
        std::vector<int64_t> m_dataSizes;
        int64_t m_hierTable = 0;
        int64_t m_nonHierTable = 0;
        int64_t m_pages = 0;
        int64_t m_records = 0;
    };
}

SlideStructure slideio::mrxs::readSlideStructure(const std::string& entryPath, const IndexBudget& budget)
{
    SlideStructure slide;
    slide.paths = resolveBundle(entryPath);
    slide.ini = IniFile::read(slide.paths.ini);
    slide.metadata = readMetadata(slide.ini, slide.paths.dataDir);
    StructureReader(slide, budget).read();
    return slide;
}
