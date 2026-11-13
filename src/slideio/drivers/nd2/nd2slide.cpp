// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#include "slideio/drivers/nd2/nd2slide.hpp"
#include "slideio/drivers/nd2/nd2chunkfile.hpp"
#include "slideio/drivers/nd2/nd2framesource.hpp"
#include "slideio/drivers/nd2/nd2imageinfo.hpp"
#include "slideio/drivers/nd2/nd2legacy.hpp"
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
#include <memory>

using namespace slideio;
using namespace slideio::nd2;

namespace
{
    const uint8_t kChunkMagic[4] = { 0xDA, 0xCE, 0xBE, 0x0A };
    const uint8_t kJp2Signature[12] = { 0x00, 0x00, 0x00, 0x0C, 0x6A, 0x50, 0x20, 0x20, 0x0D, 0x0A, 0x87, 0x0A };
    const char kSignatureName[] = "ND2 FILE SIGNATURE CHUNK NAME01!";
    constexpr uint64_t kNameSize = 32;

    bool bytesAt(const FileReader& file, uint64_t offset, const void* expected, size_t size)
    {
        if (offset > file.size() || file.size() - offset < size) {
            return false;
        }
        std::vector<uint8_t> buffer(size);
        file.readAt(offset, buffer.data(), size);
        return std::memcmp(buffer.data(), expected, size) == 0;
    }

    /// Design 4.1: which variant the file is, from its first bytes: true for the legacy
    /// JPEG 2000 variant, false for a chunked file; throws for anything that is not an ND2.
    bool isLegacy(const FileReader& file)
    {
        if (bytesAt(file, 0, kChunkMagic, sizeof(kChunkMagic))) {
            if (!bytesAt(file, 16, kSignatureName, kNameSize)) {
                RAISE_RUNTIME_ERROR << "ND2: not a Nikon ND2 file (no file signature at offset 16)";
            }
            // A missing chunk map signature is not checked here: ChunkFile recovers
            // the chunks by scanning, with a WARNING (R18).
            return false;
        }
        if (bytesAt(file, 0, kJp2Signature, sizeof(kJp2Signature))) {
            return true;
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
    const bool legacy = isLegacy(*m_file);
    const std::string stem = std::filesystem::u8path(m_filePath).stem().u8string();
    std::unique_ptr<ChunkFile> chunks;
    std::unique_ptr<LegacyFile> boxes;
    std::shared_ptr<const ImageInfo> info;
    if (legacy) {
        boxes = std::make_unique<LegacyFile>(LegacyFile::read(*m_file));
        info = std::make_shared<const ImageInfo>(buildImageInfo(*boxes, m_file, stem));
    }
    else {
        chunks = std::make_unique<ChunkFile>(ChunkFile::read(*m_file));
        info = std::make_shared<const ImageInfo>(buildImageInfo(*chunks, m_file, stem));
    }

    // uiComp > CV_CN_MAX is refused by Attributes::parse and parseChannels (chunked and
    // legacy), before the component count sizes anything.
    const Attributes& attrs = info->attributes;
    countToInt(info->experiment.count(Axis::Z), "Z");
    countToInt(info->experiment.count(Axis::T), "T");
    if (info->positions.empty()) {
        RAISE_RUNTIME_ERROR << "ND2: the file has no acquired XY position";
    }
    // Design 4.5: frames beyond the loops are ignored.
    const int64_t loopProduct = info->experiment.loopProduct();
    int64_t beyond = 0;
    // Legacy: frame i, channel c is codestream i*C + c; RGB has one codestream per channel.
    const int legacyChannels = legacy ? attrs.components / boxes->components() : 0;
    if (legacy) {
        const int64_t streams = static_cast<int64_t>(boxes->codestreams().size());
        beyond = std::max<int64_t>(0, (streams + legacyChannels - 1) / legacyChannels - loopProduct);
    }
    else {
        const auto& frames = chunks->frames();
        beyond = std::count_if(frames.begin(), frames.end(),
            [loopProduct](const std::pair<const int64_t, ChunkHeader>& f) { return f.first >= loopProduct; });
    }
    if (beyond > 0) {
        SLIDEIO_LOG(INFO) << "ND2: " << beyond << " frames beyond the " << loopProduct
            << " the experiment describes are ignored in " << m_filePath;
    }

    m_rawMetadata = info->rawJson;
    m_metadataFormat = MetadataFormat::JSON;

    Compression compression = Compression::Uncompressed;
    std::shared_ptr<const FrameSource> source;
    if (legacy) {
        compression = Compression::Jpeg2000;
        source = makeJp2FrameSource(m_file, *boxes, legacyChannels);
    }
    else {
        if (attrs.compression == FrameCompression::Zlib) {
            compression = Compression::Zlib;
        }
        source = makeChunkFrameSource(m_file, *chunks, attrs);
    }
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
        RAISE_RUNTIME_ERROR << "ND2: invalid scene index: " << index << ", the slide has "
            << getNumScenes() << " scenes";
    }
    return m_scenes[index];
}
