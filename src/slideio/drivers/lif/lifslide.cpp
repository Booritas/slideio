// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#include "slideio/drivers/lif/lifslide.hpp"
#include "slideio/drivers/lif/lifcontainer.hpp"
#include "slideio/drivers/lif/liflayout.hpp"
#include "slideio/drivers/lif/lifxml.hpp"
#include "slideio/core/exceptions.hpp"
#include "slideio/core/log.hpp"
#include "slideio/core/tools/filereader.hpp"
#include <exception>
#include <sstream>

using namespace slideio;
using namespace slideio::lif;

namespace
{
    // How many skip reasons the "no supported image" error quotes.
    constexpr size_t kReasonsInError = 3;
}

std::shared_ptr<LIFSlide> LIFSlide::openFile(const std::string& path, ReadLimits limits)
{
    SLIDEIO_LOG(INFO) << "LIFSlide::openFile: " << path;
    std::shared_ptr<LIFSlide> slide(new LIFSlide);
    slide->m_filePath = path;
    slide->setDriverId("LIF");
    try {
        slide->init(limits);
    }
    catch (const std::exception& e) {
        // The user sees "LIF: <cause> (file: <path>)": the file:line prefix that
        // RAISE_RUNTIME_ERROR adds is dropped, and a cause from outside the driver
        // (FileReader, tinyxml2) gains the driver's prefix.
        const std::string what = e.what();
        const size_t start = what.find("LIF: ");
        const std::string cause = start == std::string::npos ? "LIF: " + what : what.substr(start);
        throw RuntimeError() << cause << " (file: " << path << ")";
    }
    return slide;
}

void LIFSlide::init(const ReadLimits& limits)
{
    m_file = std::make_shared<const FileReader>(m_filePath);
    const Container container = Container::read(*m_file);
    m_rawMetadata = container.xml();
    m_metadataFormat = MetadataFormat::XML;

    const std::vector<ImageDescription> images = parseImages(container.description());
    for (const ImageDescription& image : images) {
        std::string reason;
        const MemoryBlock* block = container.findBlock(image.memoryId);
        if (block && block->length == 0 && !block->truncated && block->occurrences == 1
            && image.declaredSize <= 0) {
            // A block of length 0 marks a container element, not an image (D 4.3).
            // One whose Memory/@Size promises data is an image with its data
            // missing: it falls through to the size check and is reported.
            continue;
        }
        std::shared_ptr<const Layout> layout;
        if (!block) {
            reason = "memory block \"" + image.memoryId + "\" is not in the file";
        }
        else if (block->occurrences > 1) {
            reason = "memory block \"" + image.memoryId + "\" occurs " + std::to_string(block->occurrences)
                + " times; which one holds the image is ambiguous";
        }
        else if (block->truncated) {
            reason = "memory block \"" + image.memoryId + "\" runs past the end of the file";
        }
        else if (image.declaredSize >= 0 && static_cast<uint64_t>(image.declaredSize) != block->length) {
            reason = "Memory/@Size declares " + std::to_string(image.declaredSize)
                + " bytes but memory block \"" + image.memoryId + "\" holds " + std::to_string(block->length);
        }
        else {
            try {
                layout = std::make_shared<const Layout>(Layout::build(image, block->length));
            }
            catch (const Unsupported& e) {
                reason = e.what();
            }
        }
        // CVTools::toOpencvType maps DT_UInt32 (8) and DT_UInt64 (10) onto OpenCV type
        // codes of other types, so the base class's 4D and edge paths would return
        // wrong output for them.
        if (layout && layout->dataType() == DataType::DT_UInt32) {
            layout.reset();
            reason = "32-bit unsigned samples are not supported yet";
        }
        else if (layout && layout->dataType() == DataType::DT_UInt64) {
            layout.reset();
            reason = "64-bit unsigned samples are not supported yet";
        }
        if (!layout) {
            SLIDEIO_LOG(WARNING) << "LIF: image \"" << image.path << "\" skipped: " << reason;
            m_skipped.push_back(image.path + ": " + reason);
            continue;
        }
        const int numFields = layout->numFields();
        for (int field = 0; field < numFields; ++field) {
            const std::string name = numFields > 1
                ? image.path + "/Field " + std::to_string(field + 1)
                : image.path;
            m_scenes.push_back(std::make_shared<LIFScene>(m_filePath, name, static_cast<int>(m_scenes.size()),
                m_file, layout, block->offset, field, image, limits));
        }
    }

    if (m_scenes.empty()) {
        std::ostringstream message;
        message << "LIF: the file holds no supported image";
        if (!m_skipped.empty()) {
            message << ": ";
            for (size_t i = 0; i < m_skipped.size() && i < kReasonsInError; ++i) {
                message << (i ? "; " : "") << m_skipped[i];
            }
            if (m_skipped.size() > kReasonsInError) {
                message << "; and " << (m_skipped.size() - kReasonsInError) << " more";
            }
        }
        throw RuntimeError() << message.str();
    }
    if (!m_skipped.empty()) {
        SLIDEIO_LOG(INFO) << "LIF: " << m_skipped.size() << " of " << images.size()
            << " images skipped in " << m_filePath;
    }
}

int LIFSlide::getNumScenes() const
{
    return static_cast<int>(m_scenes.size());
}

std::string LIFSlide::getFilePath() const
{
    return m_filePath;
}

std::shared_ptr<CVScene> LIFSlide::getScene(int index) const
{
    if (index < 0 || index >= getNumScenes()) {
        RAISE_RUNTIME_ERROR << "LIF driver: invalid scene index: " << index << ", the slide has "
            << getNumScenes() << " scenes";
    }
    return m_scenes[index];
}
