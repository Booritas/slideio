// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#include "slideio/drivers/mrxs/mrxsauxscene.hpp"
#include "slideio/drivers/mrxs/mrxsdecoder.hpp"
#include "slideio/drivers/mrxs/mrxserrors.hpp"
#include "slideio/core/tools/tools.hpp"
#include "slideio/imagetools/imagetools.hpp"

using namespace slideio;
using namespace slideio::mrxs;

MRXSAuxScene::MRXSAuxScene(std::shared_ptr<const MRXSDataset> dataset, std::string name, PayloadRef payload)
    : m_dataset(std::move(dataset)), m_name(std::move(name)), m_payload(payload)
{
    // An associated image is not reachable through a file path of its own: converter and
    // transformer must not try to reopen it by path (design section 6).
    markAsAuxImage();
    if (m_payload.length > kMaxDecodedBytes) {
        MRXS_LIMIT << "associated image " << m_name << " is " << m_payload.length << " bytes";
    }
    const std::vector<uint8_t> bytes = readPayload(m_dataset->structure, m_payload);
    const ImageTools::EncodedImageInfo info = ImageTools::probeFreeImageStream(bytes.data(), bytes.size());
    if (info.codec != Compression::Jpeg || info.size.width <= 0 || info.size.height <= 0) {
        MRXS_CORRUPT << "associated image " << m_name << " is not a readable JPEG";
    }
    m_size = info.size;
    m_levels.emplace_back(0, Size(m_size.width, m_size.height), 1.0, 0.0, Size(m_size.width, m_size.height));
}

std::string MRXSAuxScene::getFilePath() const
{
    return m_dataset->structure.paths.entry.u8string();
}

const std::string& MRXSAuxScene::getDriverId() const
{
    static const std::string id = "MRXS";
    return id;
}

DataType MRXSAuxScene::getChannelDataType(int channel) const
{
    if (channel < 0 || channel >= getNumChannels()) {
        RAISE_RUNTIME_ERROR << "MRXS: invalid channel index " << channel;
    }
    return DataType::DT_Byte;
}

std::string MRXSAuxScene::getChannelName(int channel) const
{
    static const char* names[] = {"Red", "Green", "Blue"};
    if (channel < 0 || channel >= getNumChannels()) {
        RAISE_RUNTIME_ERROR << "MRXS: invalid channel index " << channel;
    }
    return names[channel];
}

void MRXSAuxScene::readResampledBlockChannelsEx(const cv::Rect& blockRect, const cv::Size& blockSize,
                                                const std::vector<int>& channelIndices, int zSliceIndex,
                                                int tFrameIndex, cv::OutputArray output)
{
    if (zSliceIndex != 0 || tFrameIndex != 0) {
        RAISE_RUNTIME_ERROR << "MRXS: the scene has one Z slice and one time frame";
    }
    for (int channel : channelIndices) {
        if (channel < 0 || channel >= getNumChannels()) {
            RAISE_RUNTIME_ERROR << "MRXS: invalid channel index " << channel;
        }
    }
    readClampedBlock(getRect(), blockRect, blockSize, channelIndices,
        [&](const cv::Rect& validRect, const cv::Size& partSize, cv::OutputArray part) {
            const cv::Mat image = decodeImage(readPayload(m_dataset->structure, m_payload), ImageFormat::Jpeg,
                                              m_size.width, m_size.height, "associated image " + m_name);
            cv::Mat block = image(validRect);
            if (block.size() != partSize) {
                cv::Mat resized;
                Tools::resize(block, resized, partSize);
                block = resized;
            }
            Tools::extractChannels(block, channelIndices, part);
        },
        output);
}
