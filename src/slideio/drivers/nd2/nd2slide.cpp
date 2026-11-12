// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#include "slideio/drivers/nd2/nd2slide.hpp"
#include "slideio/drivers/nd2/nd2chunkfile.hpp"
#include "slideio/drivers/nd2/nd2framesource.hpp"
#include "slideio/drivers/nd2/nd2imageinfo.hpp"
#include "slideio/drivers/nd2/nd2scene.hpp"
#include "slideio/core/exceptions.hpp"
#include "slideio/core/log.hpp"
#include "slideio/core/tools/filereader.hpp"
#include <opencv2/core.hpp>
#include <algorithm>
#include <cstring>
#include <exception>
#include <filesystem>
#include <limits>

using namespace slideio;
using namespace slideio::nd2;

namespace
{
    const uint8_t kChunkMagic[4] = { 0xDA, 0xCE, 0xBE, 0x0A };
    const uint8_t kJp2Signature[12] = { 0x00, 0x00, 0x00, 0x0C, 0x6A, 0x50, 0x20, 0x20, 0x0D, 0x0A, 0x87, 0x0A };
    const char kSignatureName[] = "ND2 FILE SIGNATURE CHUNK NAME01!";
    const char kTrailerName[] = "ND2 CHUNK MAP SIGNATURE 0000001!";
    constexpr uint64_t kNameSize = 32;
    constexpr uint64_t kTrailerSize = 40;

    bool bytesAt(const FileReader& file, uint64_t offset, const void* expected, size_t size)
    {
        if (offset > file.size() || file.size() - offset < size) {
            return false;
        }
        std::vector<uint8_t> buffer(size);
        file.readAt(offset, buffer.data(), size);
        return std::memcmp(buffer.data(), expected, size) == 0;
    }

    /// Design 4.1: which variant the file is, from its first bytes; throws for
    /// anything that is not a chunked ND2.
    void detect(const FileReader& file)
    {
        if (bytesAt(file, 0, kChunkMagic, sizeof(kChunkMagic))) {
            if (!bytesAt(file, 16, kSignatureName, kNameSize)) {
                RAISE_RUNTIME_ERROR << "ND2: not a Nikon ND2 file (no file signature at offset 16)";
            }
            if (file.size() < kTrailerSize || !bytesAt(file, file.size() - kTrailerSize, kTrailerName, kNameSize)) {
                RAISE_RUNTIME_ERROR << "ND2: not a Nikon ND2 file (no chunk map signature "
                                       "40 bytes before the end of the file)";
            }
            return;
        }
        if (bytesAt(file, 0, kJp2Signature, sizeof(kJp2Signature))) {
            // Task 8 replaces this with the legacy variant's reader.
            RAISE_RUNTIME_ERROR << "ND2: legacy JPEG 2000 ND2 is not supported yet";
        }
        if (file.size() < sizeof(kChunkMagic)) {
            RAISE_RUNTIME_ERROR << "ND2: not a Nikon ND2 file (the file is " << file.size() << " bytes long)";
        }
        RAISE_RUNTIME_ERROR << "ND2: not a Nikon ND2 file (unrecognised signature at offset 0)";
    }

    int countToInt(int64_t count, const char* axis)
    {
        if (count < 1 || count > std::numeric_limits<int>::max()) {
            throw Unsupported(std::string("ND2: unsupported experiment: ") + axis + " count " + std::to_string(count));
        }
        return static_cast<int>(count);
    }
}

std::shared_ptr<ND2Slide> ND2Slide::openFile(const std::string& path, ResampleLimits limits)
{
    SLIDEIO_LOG(INFO) << "ND2Slide::openFile: " << path;
    std::shared_ptr<ND2Slide> slide(new ND2Slide);
    slide->m_filePath = path;
    slide->setDriverId("ND2");
    try {
        slide->init(limits);
    }
    catch (const std::exception& e) {
        // The user sees "ND2: <cause> (file: <path>)": the file:line prefix that
        // RAISE_RUNTIME_ERROR adds is dropped, and a cause from outside the driver
        // (FileReader, OpenCV) gains the driver's prefix.
        const std::string what = e.what();
        const size_t start = what.find("ND2: ");
        const std::string cause = start == std::string::npos ? "ND2: " + what : what.substr(start);
        throw RuntimeError() << cause << " (file: " << path << ")";
    }
    return slide;
}

void ND2Slide::init(const ResampleLimits& limits)
{
    m_file = std::make_shared<const FileReader>(m_filePath);
    detect(*m_file);
    const ChunkFile chunks = ChunkFile::read(*m_file);
    const std::string stem = std::filesystem::u8path(m_filePath).stem().u8string();
    auto info = std::make_shared<const ImageInfo>(buildImageInfo(chunks, *m_file, stem));

    const Attributes& attrs = info->attributes;
    if (attrs.components > CV_CN_MAX) {
        throw Unsupported("ND2: unsupported image attributes: uiComp " + std::to_string(attrs.components)
            + " exceeds " + std::to_string(CV_CN_MAX) + " components");
    }
    countToInt(info->experiment.count(Axis::Z), "Z");
    countToInt(info->experiment.count(Axis::T), "T");
    if (info->positions.empty()) {
        RAISE_RUNTIME_ERROR << "ND2: the file has no acquired XY position";
    }
    // Design 4.5: frames beyond the loops are ignored.
    const int64_t loopProduct = info->experiment.loopProduct();
    const auto& frames = chunks.frames();
    const int64_t beyond = std::count_if(frames.begin(), frames.end(),
        [loopProduct](const std::pair<const int64_t, ChunkHeader>& f) { return f.first >= loopProduct; });
    if (beyond > 0) {
        SLIDEIO_LOG(INFO) << "ND2: " << beyond << " frames beyond the " << loopProduct
            << " the experiment describes are ignored in " << m_filePath;
    }

    m_rawMetadata = info->rawJson;
    m_metadataFormat = MetadataFormat::JSON;

    const Compression compression = attrs.compression == FrameCompression::Zlib
        ? Compression::Zlib : Compression::Uncompressed;
    std::shared_ptr<const FrameSource> source = makeChunkFrameSource(m_file, chunks, attrs);
    for (size_t p = 0; p < info->positions.size(); ++p) {
        m_scenes.push_back(std::make_shared<ND2Scene>(m_filePath, static_cast<int>(p), info, source,
            compression, limits));
    }
}

int ND2Slide::getNumScenes() const
{
    return static_cast<int>(m_scenes.size());
}

std::string ND2Slide::getFilePath() const
{
    return m_filePath;
}

std::shared_ptr<CVScene> ND2Slide::getScene(int index) const
{
    if (index < 0 || index >= getNumScenes()) {
        RAISE_RUNTIME_ERROR << "ND2: invalid scene index " << index << ", the slide has "
            << getNumScenes() << " scenes";
    }
    return m_scenes[index];
}
