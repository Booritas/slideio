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
#include <limits>

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

    // One source index's contribution to one output index along one axis.
    struct Tap
    {
        int out;
        double weight;
    };

    // The area filter's weights along one axis of a downscale (dsize <= ssize), by
    // source index: OpenCV's computeResizeAreaTab, the table cv::resize(INTER_AREA)
    // builds, with the same scale, cell bounds, 1e-3 thresholds and float weights.
    // An output cell is at least one source pixel wide, so a source index feeds at
    // most two outputs.
    std::vector<std::vector<Tap>> areaTaps(int ssize, int dsize)
    {
        const double scale = 1. / (static_cast<double>(dsize) / ssize);
        std::vector<std::vector<Tap>> bySource(ssize);
        const auto add = [&](int s, int out, double weight) {
            bySource[s].push_back({out, static_cast<double>(static_cast<float>(weight))});
        };
        for (int dx = 0; dx < dsize; ++dx) {
            const double fsx1 = dx * scale;
            const double fsx2 = fsx1 + scale;
            const double cellWidth = std::min(scale, ssize - fsx1);
            int sx1 = static_cast<int>(std::ceil(fsx1));
            int sx2 = static_cast<int>(std::floor(fsx2));
            sx2 = std::min(sx2, ssize - 1);
            sx1 = std::min(sx1, sx2);
            if (sx1 - fsx1 > 1e-3) {
                add(sx1 - 1, dx, (sx1 - fsx1) / cellWidth);
            }
            for (int sx = sx1; sx < sx2; ++sx) {
                add(sx, dx, 1. / cellWidth);
            }
            if (fsx2 - sx2 > 1e-3) {
                add(sx2, dx, std::min(std::min(fsx2 - sx2, 1.), cellWidth) / cellWidth);
            }
        }
        return bySource;
    }

    // Adds one source tile's contribution to the output sums: horizontal pass into
    // `row`, then vertical into `sums` (outW x outH x n, interleaved). The tile's
    // top-left is (x0, y0) in the coordinates of the resampled rectangle, so every
    // weight is the one the whole-rectangle filter would use.
    template <typename T>
    void accumulateTile(const cv::Mat& tile, int x0, int y0, const std::vector<std::vector<Tap>>& xTaps,
                        const std::vector<std::vector<Tap>>& yTaps, int outW, std::vector<double>& row,
                        std::vector<double>& sums)
    {
        const int n = tile.channels();
        int outLo = std::numeric_limits<int>::max(), outHi = -1;
        for (int x = 0; x < tile.cols; ++x) {
            for (const Tap& tap : xTaps[x0 + x]) {
                outLo = std::min(outLo, tap.out);
                outHi = std::max(outHi, tap.out);
            }
        }
        if (outHi < 0) {
            return;
        }
        for (int y = 0; y < tile.rows; ++y) {
            const std::vector<Tap>& rowTaps = yTaps[y0 + y];
            if (rowTaps.empty()) {
                continue;
            }
            std::fill(row.begin() + static_cast<size_t>(outLo) * n, row.begin() + static_cast<size_t>(outHi + 1) * n, 0.);
            const T* src = tile.ptr<T>(y);
            for (int x = 0; x < tile.cols; ++x) {
                for (const Tap& tap : xTaps[x0 + x]) {
                    double* dst = row.data() + static_cast<size_t>(tap.out) * n;
                    for (int c = 0; c < n; ++c) {
                        dst[c] += tap.weight * static_cast<double>(src[static_cast<size_t>(x) * n + c]);
                    }
                }
            }
            for (const Tap& tap : rowTaps) {
                double* dst = sums.data() + (static_cast<size_t>(tap.out) * outW + outLo) * n;
                const double* from = row.data() + static_cast<size_t>(outLo) * n;
                const size_t count = static_cast<size_t>(outHi - outLo + 1) * n;
                for (size_t i = 0; i < count; ++i) {
                    dst[i] += tap.weight * from[i];
                }
            }
        }
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
            // Within the budget, or enlarging along either axis (cv::resize then
            // interpolates rather than averaging, and the read needs no more memory than
            // its output on the enlarged axis): one read and cv::resize.
            const bool downscale = size.width <= rect.width && size.height <= rect.height;
            if (int64_t(rect.width) * rect.height <= budget || !downscale) {
                cv::Mat raw, resized;
                readRaw(rect, channels, zSliceIndex, tFrameIndex, raw);
                resizeSamples(raw, resized, size);
                resized.copyTo(part);
                return;
            }
            cv::Mat result;
            readTiledDownscale(rect, size, channels, zSliceIndex, tFrameIndex, result);
            result.copyTo(part);
        },
        output);
}

// The area filter of cv::resize(INTER_AREA), computed tile by tile (spec D 7).
//
// Invariant: no buffer this read assembles from the file holds more than
// max(maxResampleSourcePixels, 1) source pixels, on either axis and at any ratio --
// a tile is as wide as the rectangle when a row fits in the budget, otherwise a run
// of one row. (readRaw's per-row byte span for gapped or interleaved pixels is a
// part of one tile row, so within that too.) ReadStats::peakSourcePixels records it.
//
// The weights are those of the whole rectangle (areaTaps), indexed by each source
// pixel's position in it, so tiles need not align with output cells: a source pixel
// adds the same weighted value to the same output pixels whichever tile carries it,
// and an output cell spanning many tiles collects its sum from all of them. The
// result is therefore independent of the budget, and equals cv::resize up to
// rounding: the sums are kept in double and rounded half up for integer depths,
// where OpenCV accumulates in float (or fixed point at integer ratios).
void LIFScene::readTiledDownscale(const cv::Rect& rect, const cv::Size& size, const std::vector<int>& channels,
                                  int z, int t, cv::Mat& out) const
{
    const int n = static_cast<int>(channels.size());
    const auto xTaps = areaTaps(rect.width, size.width);
    const auto yTaps = areaTaps(rect.height, size.height);
    std::vector<double> sums(static_cast<size_t>(size.width) * size.height * n, 0.);
    std::vector<double> row(static_cast<size_t>(size.width) * n, 0.);
    const int64_t budget = std::max<int64_t>(1, m_limits.maxResampleSourcePixels);
    const int tileW = static_cast<int>(std::min<int64_t>(rect.width, budget));
    const int tileH = static_cast<int>(std::max<int64_t>(1, std::min<int64_t>(rect.height, budget / tileW)));
    const int depth = cvDepth();
    for (int ty = 0; ty < rect.height; ty += tileH) {
        const int th = std::min(tileH, rect.height - ty);
        for (int tx = 0; tx < rect.width; tx += tileW) {
            const int tw = std::min(tileW, rect.width - tx);
            cv::Mat tile;
            readRaw(cv::Rect(rect.x + tx, rect.y + ty, tw, th), channels, z, t, tile);
            switch (depth) {
            case CV_8U: accumulateTile<uint8_t>(tile, tx, ty, xTaps, yTaps, size.width, row, sums); break;
            case CV_8S: accumulateTile<int8_t>(tile, tx, ty, xTaps, yTaps, size.width, row, sums); break;
            case CV_16U: accumulateTile<uint16_t>(tile, tx, ty, xTaps, yTaps, size.width, row, sums); break;
            case CV_16S: accumulateTile<int16_t>(tile, tx, ty, xTaps, yTaps, size.width, row, sums); break;
            case CV_32S: accumulateTile<int32_t>(tile, tx, ty, xTaps, yTaps, size.width, row, sums); break;
            case CV_32F: accumulateTile<float>(tile, tx, ty, xTaps, yTaps, size.width, row, sums); break;
            case CV_64F: accumulateTile<double>(tile, tx, ty, xTaps, yTaps, size.width, row, sums); break;
            case CV_16F: {
                cv::Mat wide;
                tile.convertTo(wide, CV_MAKETYPE(CV_32F, n));
                accumulateTile<float>(wide, tx, ty, xTaps, yTaps, size.width, row, sums);
                break;
            }
            default:
                RAISE_RUNTIME_ERROR << "LIF: unexpected sample depth " << depth << " in scene \"" << m_name << "\"";
            }
        }
    }
    const bool integral = depth != CV_16F && depth != CV_32F && depth != CV_64F;
    if (integral) {
        for (double& v : sums) {
            v = std::floor(v + 0.5);
        }
    }
    const cv::Mat wide(size, CV_MAKETYPE(CV_64F, n), sums.data());
    if (depth == CV_16F) {
        cv::Mat single;
        wide.convertTo(single, CV_MAKETYPE(CV_32F, n));
        single.convertTo(out, CV_MAKETYPE(CV_16F, n));
    }
    else {
        wide.convertTo(out, CV_MAKETYPE(depth, n));
    }
}

void ReadStats::noteSource(int64_t pixels)
{
    int64_t peak = peakSourcePixels.load();
    while (pixels > peak && !peakSourcePixels.compare_exchange_weak(peak, pixels)) {
    }
}

void LIFScene::readRaw(const cv::Rect& rect, const std::vector<int>& channels, int z, int t, cv::Mat& out) const
{
    if (m_limits.stats) {
        m_limits.stats->noteSource(static_cast<int64_t>(rect.width) * rect.height);
    }
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
