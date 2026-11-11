// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#include "slideio/drivers/lif/lifscene.hpp"
#include "slideio/core/exceptions.hpp"
#include "slideio/core/levelinfo.hpp"
#include "slideio/core/log.hpp"
#include "slideio/core/tools/cvtools.hpp"
#include "slideio/core/tools/endian.hpp"
#include "slideio/core/tools/filereader.hpp"
#include "slideio/core/tools/tools.hpp"
#include <opencv2/imgproc.hpp>
#include <algorithm>
#include <cmath>
#include <cstring>

using namespace slideio;
using namespace slideio::lif;

namespace
{
    // Samples are little-endian in the file (spec D 7). slideio builds for no
    // big-endian target today, so this is a guard rather than a tested path.
    void toNative(uint8_t* data, size_t count, int sampleBytes)
    {
        if (sampleBytes <= 1 || Endian::isLittleEndian()) {
            return;
        }
        for (size_t i = 0; i < count; ++i) {
            std::reverse(data + i * sampleBytes, data + (i + 1) * sampleBytes);
        }
    }

    // INTER_AREA resampling for every depth a LIF scene returns. cv::resize has no
    // CV_16F path.
    void resizeSamples(const cv::Mat& src, cv::Mat& dst, const cv::Size& size)
    {
        if (src.depth() == CV_16F) {
            cv::Mat wide, resized;
            src.convertTo(wide, CV_MAKETYPE(CV_32F, src.channels()));
            cv::resize(wide, resized, size, 0, 0, cv::INTER_AREA);
            resized.convertTo(dst, src.type());
            return;
        }
        Tools::resize(src, dst, size, cv::INTER_AREA);
    }
}

LIFScene::LIFScene(const std::string& filePath, const std::string& name, int sceneIndex,
                   std::shared_ptr<const FileReader> file, std::shared_ptr<const Layout> layout,
                   uint64_t payloadOffset, int field, const ImageDescription& image,
                   ReadLimits limits)
    : m_filePath(filePath),
      m_driverId("LIF"),
      m_name(name),
      m_sceneIndex(sceneIndex),
      m_file(std::move(file)),
      m_layout(std::move(layout)),
      m_payloadOffset(payloadOffset),
      m_field(field),
      m_calibration(m_layout->calibration()),
      m_magnification(image.magnification),
      m_limits(limits)
{
    const int numChannels = m_layout->numChannels();
    m_channelNames.resize(numChannels);
    for (int channel = 0; channel < numChannels; ++channel) {
        const int stored = m_layout->storedSample(channel);
        if (stored >= 0 && stored < static_cast<int>(image.channels.size())) {
            const Channel& description = image.channels[stored];
            m_channelNames[channel] = description.name;
            for (const auto& attribute : description.attributes) {
                setChannelAttribute(channel, attribute.first, attribute.second);
            }
        }
    }

    // Timestamps (D 6.3). A zero tick, or a list placing the acquisition before the
    // Unix epoch, is not a time anyone recorded: treat the list as absent.
    const std::vector<uint64_t>& ticks = image.timestampTicks;
    const bool zeroTick = std::find(ticks.begin(), ticks.end(), uint64_t(0)) != ticks.end();
    if (!ticks.empty() && !zeroTick) {
        m_times = PlaneTimes::build(ticks, *m_layout, m_field);
        m_hasTimestamps = m_times.complete() && m_times.acquisitionTime() >= 0;
        if (!m_times.complete()) {
            SLIDEIO_LOG(INFO) << "LIF: image \"" << image.path << "\" has " << ticks.size()
                << " timestamps, not one per stored plane; reporting none";
        }
    }
    else if (zeroTick) {
        SLIDEIO_LOG(INFO) << "LIF: image \"" << image.path << "\" has a zero timestamp; reporting none";
    }

    m_rawMetadata = image.rawXml;
    m_metadataFormat = MetadataFormat::XML;

    const Size size(m_layout->width(), m_layout->height());
    m_levels.emplace_back(0, size, 1.0, m_magnification, size);
}

cv::Rect LIFScene::getRect() const
{
    return cv::Rect(0, 0, m_layout->width(), m_layout->height());
}

int LIFScene::getNumChannels() const
{
    return m_layout->numChannels();
}

int LIFScene::getNumZSlices() const
{
    return m_layout->numZ();
}

int LIFScene::getNumTFrames() const
{
    return m_layout->numT();
}

DataType LIFScene::getChannelDataType(int channel) const
{
    checkChannel(channel);
    return m_layout->dataType();
}

std::string LIFScene::getChannelName(int channel) const
{
    checkChannel(channel);
    return m_channelNames[channel];
}

Resolution LIFScene::getResolution() const
{
    return Resolution(m_calibration.x, m_calibration.y);
}

double LIFScene::getZSliceResolution() const
{
    // One Z slice: either there is no Z axis, or Z is the display Y of an XZ scan,
    // whose spacing is already the Y resolution.
    return m_layout->numZ() > 1 ? m_calibration.z : 0.;
}

double LIFScene::getTFrameResolution() const
{
    return m_calibration.t;
}

int LIFScene::getChannelSignificantBits(int channel) const
{
    checkChannel(channel);
    return m_layout->significantBits(channel);
}

bool LIFScene::hasPlaneTimestamps() const
{
    return m_hasTimestamps;
}

double LIFScene::getPlaneTimestamp(int tFrame, int channel, int zSlice) const
{
    return m_hasTimestamps ? m_times.offset(tFrame, channel, zSlice) : 0.;
}

int64_t LIFScene::getAcquisitionTime() const
{
    return m_hasTimestamps ? m_times.acquisitionTime() : 0;
}

double LIFScene::getChannelBackground(int) const
{
    return 0.;
}

void LIFScene::checkChannel(int channel) const
{
    if (channel < 0 || channel >= m_layout->numChannels()) {
        RAISE_RUNTIME_ERROR << "LIF: channel index " << channel << " is out of range [0, "
            << m_layout->numChannels() << ") in scene \"" << m_name << "\"";
    }
}

int LIFScene::cvDepth() const
{
    return CVTools::cvTypeFromDataType(m_layout->dataType());
}

void LIFScene::readResampledBlockChannelsEx(const cv::Rect& blockRect, const cv::Size& blockSize,
    const std::vector<int>& channelIndices, int zSliceIndex, int tFrameIndex, cv::OutputArray output)
{
    if (zSliceIndex < 0 || zSliceIndex >= m_layout->numZ()) {
        RAISE_RUNTIME_ERROR << "LIF: z slice " << zSliceIndex << " is out of range [0, "
            << m_layout->numZ() << ") in scene \"" << m_name << "\"";
    }
    if (tFrameIndex < 0 || tFrameIndex >= m_layout->numT()) {
        RAISE_RUNTIME_ERROR << "LIF: time frame " << tFrameIndex << " is out of range [0, "
            << m_layout->numT() << ") in scene \"" << m_name << "\"";
    }
    std::vector<int> channels = channelIndices;
    if (channels.empty()) {
        for (int channel = 0; channel < m_layout->numChannels(); ++channel) {
            channels.push_back(channel);
        }
    }
    for (int channel : channels) {
        checkChannel(channel);
    }
    readClampedBlock(getRect(), blockRect, blockSize, channelIndices,
        [&](const cv::Rect& rect, const cv::Size& size, cv::OutputArray part) {
            const int64_t budget = m_limits.maxResampleSourcePixels;
            if (size == rect.size()) {
                cv::Mat raw;
                readRaw(rect, channels, zSliceIndex, tFrameIndex, raw);
                raw.copyTo(part);
                return;
            }
            // Only a downscale is banded: an upscaled read needs no more memory than its
            // output, and bands would leave seams in an interpolating upscale.
            if (int64_t(rect.width) * rect.height <= budget || size.height >= rect.height) {
                cv::Mat raw, resized;
                readRaw(rect, channels, zSliceIndex, tFrameIndex, raw);
                resizeSamples(raw, resized, size);
                resized.copyTo(part);
                return;
            }
            // Too large to read whole before resampling: read and resample horizontal
            // bands of the source, each into its share of the output rows, so that the
            // band heights sum exactly to the target height. A banded read equals an
            // unbanded one exactly only at integer ratios, where every band boundary
            // falls on a whole box of the area filter (as in MRXS); elsewhere it
            // differs by rounding along the seams.
            cv::Mat result(size, CV_MAKETYPE(cvDepth(), static_cast<int>(channels.size())));
            const double sy = static_cast<double>(rect.height) / size.height;
            const int64_t budgetRows = std::max<int64_t>(1, budget / rect.width);
            // A band's source spans bandRows * sy rows plus at most one for rounding.
            const int bandRows = std::max(1, static_cast<int>(std::floor((budgetRows - 1) / sy)));
            for (int oy = 0; oy < size.height; oy += bandRows) {
                const int oh = std::min(bandRows, size.height - oy);
                const int y0 = static_cast<int>(std::floor(oy * sy));
                const int y1 = std::min(rect.height, static_cast<int>(std::ceil((oy + oh) * sy)));
                cv::Mat raw, resized;
                readRaw(cv::Rect(rect.x, rect.y + y0, rect.width, y1 - y0), channels,
                        zSliceIndex, tFrameIndex, raw);
                resizeSamples(raw, resized, cv::Size(size.width, oh));
                resized.copyTo(result(cv::Rect(0, oy, size.width, oh)));
            }
            result.copyTo(part);
        },
        output);
}

void LIFScene::readRaw(const cv::Rect& rect, const std::vector<int>& channels, int z, int t, cv::Mat& out) const
{
    if (m_layout->interleaved()) {
        readInterleaved(rect, channels, z, t, out);
        return;
    }
    if (channels.size() == 1) {
        readScalarPlane(rect, channels.front(), z, t, out);
        return;
    }
    std::vector<cv::Mat> planes(channels.size());
    for (size_t k = 0; k < channels.size(); ++k) {
        readScalarPlane(rect, channels[k], z, t, planes[k]);
    }
    cv::merge(planes, out);
}

void LIFScene::readScalarPlane(const cv::Rect& rect, int channel, int z, int t, cv::Mat& plane) const
{
    const int sampleBytes = m_layout->sampleBytes();
    const uint64_t pixelStride = static_cast<uint64_t>(m_layout->pixelStride());
    const uint64_t rowStride = static_cast<uint64_t>(m_layout->rowStride());
    const uint64_t base = m_payloadOffset + m_layout->planeOffset(channel, z, t, m_field);
    plane.create(rect.height, rect.width, cvDepth());
    const size_t rowBytes = static_cast<size_t>(rect.width) * sampleBytes;
    if (pixelStride == static_cast<uint64_t>(sampleBytes)) {
        const bool wholeRows = rect.x == 0 && rect.width == m_layout->width();
        if (wholeRows && rowStride == rowBytes && plane.isContinuous()) {
            // Rows follow one another without padding: one read for the band.
            m_file->readAt(base + rect.y * rowStride, plane.data, rowBytes * rect.height);
        }
        else {
            for (int y = 0; y < rect.height; ++y) {
                m_file->readAt(base + (rect.y + y) * rowStride + rect.x * pixelStride, plane.ptr(y), rowBytes);
            }
        }
    }
    else {
        // Gapped pixels: read the row's span and gather the samples.
        const size_t span = static_cast<size_t>(rect.width - 1) * pixelStride + sampleBytes;
        std::vector<uint8_t> buffer(span);
        for (int y = 0; y < rect.height; ++y) {
            m_file->readAt(base + (rect.y + y) * rowStride + rect.x * pixelStride, buffer.data(), span);
            uint8_t* dst = plane.ptr(y);
            for (int x = 0; x < rect.width; ++x) {
                std::memcpy(dst + static_cast<size_t>(x) * sampleBytes,
                            buffer.data() + static_cast<size_t>(x) * pixelStride, sampleBytes);
            }
        }
    }
    for (int y = 0; y < rect.height; ++y) {
        toNative(plane.ptr(y), static_cast<size_t>(rect.width), sampleBytes);
    }
}

void LIFScene::readInterleaved(const cv::Rect& rect, const std::vector<int>& channels, int z, int t, cv::Mat& out) const
{
    const int sampleBytes = m_layout->sampleBytes();
    const uint64_t pixelStride = static_cast<uint64_t>(m_layout->pixelStride());
    const uint64_t rowStride = static_cast<uint64_t>(m_layout->rowStride());
    const int numChannels = m_layout->numChannels();
    // planeOffset addresses one colour's sample inside pixel (0, 0); the pixel itself
    // starts at the smallest of them.
    std::vector<uint64_t> sampleOffsets(numChannels);
    for (int channel = 0; channel < numChannels; ++channel) {
        sampleOffsets[channel] = m_layout->planeOffset(channel, z, t, m_field);
    }
    const uint64_t pixelBase = *std::min_element(sampleOffsets.begin(), sampleOffsets.end());
    const uint64_t lastSample = *std::max_element(sampleOffsets.begin(), sampleOffsets.end()) - pixelBase;
    const int n = static_cast<int>(channels.size());
    std::vector<size_t> within(n);
    for (int k = 0; k < n; ++k) {
        within[k] = static_cast<size_t>(sampleOffsets[channels[k]] - pixelBase);
    }
    out.create(rect.height, rect.width, CV_MAKETYPE(cvDepth(), n));
    const size_t span = static_cast<size_t>(rect.width - 1) * pixelStride + lastSample + sampleBytes;
    std::vector<uint8_t> buffer(span);
    const uint64_t base = m_payloadOffset + pixelBase;
    for (int y = 0; y < rect.height; ++y) {
        m_file->readAt(base + (rect.y + y) * rowStride + rect.x * pixelStride, buffer.data(), span);
        uint8_t* dst = out.ptr(y);
        for (int x = 0; x < rect.width; ++x) {
            const uint8_t* pixel = buffer.data() + static_cast<size_t>(x) * pixelStride;
            for (int k = 0; k < n; ++k) {
                std::memcpy(dst + (static_cast<size_t>(x) * n + k) * sampleBytes, pixel + within[k], sampleBytes);
            }
        }
        toNative(dst, static_cast<size_t>(rect.width) * n, sampleBytes);
    }
}
