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
#include <cfloat>
#include <cmath>
#include <cstring>
#include <deque>
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
        float weight;   // OpenCV keeps its area weights in float
    };

    // The area filter's weights along one axis of a downscale (dsize <= ssize), by
    // source index: OpenCV's computeResizeAreaTab, the table cv::resize(INTER_AREA)
    // builds, with the same scale, cell bounds, 1e-3 thresholds and float weights.
    // An output cell is at least one source pixel wide, so a source index feeds at
    // most two outputs. Flat: source s's taps are taps[start[s] .. start[s + 1]).
    struct AxisTaps
    {
        std::vector<int> start;        // ssize + 1 offsets into taps
        std::vector<Tap> taps;         // ordered by source index
        std::vector<int> lastSource;   // per output: the last source index feeding it
        const Tap* begin(int s) const { return taps.data() + start[s]; }
        const Tap* end(int s) const { return taps.data() + start[s + 1]; }
    };

    AxisTaps areaTaps(int ssize, int dsize)
    {
        const double scale = 1. / (static_cast<double>(dsize) / ssize);
        AxisTaps result;
        result.start.assign(static_cast<size_t>(ssize) + 1, 0);
        result.lastSource.assign(dsize, -1);
        // OpenCV's loop emits taps with non-decreasing source indices, so they are
        // already in source order: count them per source, then take prefix sums.
        int previous = 0;
        const auto add = [&](int s, int out, double weight) {
            if (s < previous) {
                RAISE_RUNTIME_ERROR << "LIF: internal error: area taps out of source order";
            }
            previous = s;
            result.taps.push_back({out, static_cast<float>(weight)});
            ++result.start[s + 1];
            result.lastSource[out] = std::max(result.lastSource[out], s);
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
        for (int s = 0; s < ssize; ++s) {
            result.start[s + 1] += result.start[s];
        }
        return result;
    }

    // cv::resize's integer-ratio path (resizeAreaFast): every source pixel of a
    // scale x scale block counts once, and the block sum is scaled afterwards.
    AxisTaps unitTaps(int ssize, int scale)
    {
        AxisTaps result;
        const int dsize = ssize / scale;
        result.start.resize(static_cast<size_t>(ssize) + 1);
        result.lastSource.resize(dsize);
        for (int s = 0; s < ssize; ++s) {
            result.start[s] = static_cast<int>(result.taps.size());
            if (s / scale < dsize) {
                result.taps.push_back({s / scale, 1.f});
                result.lastSource[s / scale] = s;
            }
        }
        result.start[ssize] = static_cast<int>(result.taps.size());
        return result;
    }

    // Whether cv::resize(INTER_AREA) takes its integer-ratio path for this downscale,
    // decided exactly as OpenCV decides it; sets the integer scales.
    bool areaFast(const cv::Size& src, const cv::Size& dst, int& scaleX, int& scaleY)
    {
        const double sx = 1. / (static_cast<double>(dst.width) / src.width);
        const double sy = 1. / (static_cast<double>(dst.height) / src.height);
        scaleX = cv::saturate_cast<int>(sx);
        scaleY = cv::saturate_cast<int>(sy);
        return std::abs(sx - scaleX) < DBL_EPSILON && std::abs(sy - scaleY) < DBL_EPSILON;
    }

    // Sums of the output rows a source strip can still add to: rows [base, base +
    // size()). A row leaves the window, rounded into the output, once every source row
    // feeding it has been read; its buffer is reused for the next row to enter.
    template <typename W>
    class RowWindow
    {
    public:
        explicit RowWindow(size_t rowLength) : m_rowLength(rowLength) {}
        int base() const { return m_base; }
        int size() const { return static_cast<int>(m_rows.size()); }
        // The sums of output row oy (>= base), opening it and any rows before it.
        W* row(int oy)
        {
            while (m_base + size() <= oy) {
                std::vector<W> fresh;
                if (!m_spare.empty()) {
                    fresh = std::move(m_spare.back());
                    m_spare.pop_back();
                }
                fresh.assign(m_rowLength, W(0));
                m_rows.push_back(std::move(fresh));
            }
            return m_rows[static_cast<size_t>(oy - m_base)].data();
        }
        const std::vector<W>& front() const { return m_rows.front(); }
        void popFront()
        {
            m_spare.push_back(std::move(m_rows.front()));
            m_rows.pop_front();
            ++m_base;
        }
    private:
        size_t m_rowLength;
        int m_base = 0;
        std::deque<std::vector<W>> m_rows;
        std::vector<std::vector<W>> m_spare;
    };

    // Adds one source tile's contribution to the output sums: horizontal pass into
    // `row`, then vertical into the window's rows. The tile's top-left is (x0, y0) in
    // the coordinates of the resampled rectangle, so every weight is the one the
    // whole-rectangle filter would use. W is OpenCV's accumulator type for the depth
    // (float, double for CV_64F), and each output's terms arrive in OpenCV's order --
    // source x, then source y, ascending -- so a tile as wide as the rectangle sums
    // exactly as ResizeArea_Invoker does.
    template <typename T, typename W>
    void accumulateTile(const cv::Mat& tile, int x0, int y0, const AxisTaps& xTaps, const AxisTaps& yTaps,
                        std::vector<W>& row, RowWindow<W>& window)
    {
        const int n = tile.channels();
        int outLo = std::numeric_limits<int>::max(), outHi = -1;
        for (int x = 0; x < tile.cols; ++x) {
            for (const Tap* tap = xTaps.begin(x0 + x); tap != xTaps.end(x0 + x); ++tap) {
                outLo = std::min(outLo, tap->out);
                outHi = std::max(outHi, tap->out);
            }
        }
        if (outHi < 0) {
            return;
        }
        const W* from = row.data() + static_cast<size_t>(outLo) * n;
        const size_t count = static_cast<size_t>(outHi - outLo + 1) * n;
        for (int y = 0; y < tile.rows; ++y) {
            const Tap* rowBegin = yTaps.begin(y0 + y);
            const Tap* rowEnd = yTaps.end(y0 + y);
            if (rowBegin == rowEnd) {
                continue;
            }
            std::fill(row.begin() + static_cast<size_t>(outLo) * n, row.begin() + static_cast<size_t>(outHi + 1) * n, W(0));
            const T* src = tile.ptr<T>(y);
            for (int x = 0; x < tile.cols; ++x) {
                for (const Tap* tap = xTaps.begin(x0 + x); tap != xTaps.end(x0 + x); ++tap) {
                    W* dst = row.data() + static_cast<size_t>(tap->out) * n;
                    const W weight = static_cast<W>(tap->weight);
                    for (int c = 0; c < n; ++c) {
                        dst[c] += static_cast<W>(src[static_cast<size_t>(x) * n + c]) * weight;
                    }
                }
            }
            for (const Tap* tap = rowBegin; tap != rowEnd; ++tap) {
                W* dst = window.row(tap->out) + static_cast<size_t>(outLo) * n;
                const W weight = static_cast<W>(tap->weight);
                for (size_t i = 0; i < count; ++i) {
                    dst[i] += weight * from[i];
                }
            }
        }
    }

    void raiseTo(std::atomic<int64_t>& peak, int64_t value)
    {
        int64_t current = peak.load();
        while (value > current && !peak.compare_exchange_weak(current, value)) {
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
// Memory bounds, on either axis and at any ratio:
// - Source: no buffer this read assembles from the file holds more than
//   max(maxResampleSourcePixels, 1) source pixels -- a tile is as wide as the
//   rectangle when a row fits in the budget, otherwise a run of one row. (readRaw's
//   per-row byte span for gapped or interleaved pixels is part of one tile row, so
//   within that too.) ReadStats::peakSourcePixels records it.
// - Sums: beyond the output itself, only the output rows the current strip of tiles
//   can reach are accumulated: at most ceil(tileH / sy) + 2 rows of output width in
//   the accumulator type, plus one such row for the horizontal pass and the per-axis weight tables
//   (O(source width + height)). Strips arrive top to bottom, and a row is rounded
//   into the output and its buffer reused as soon as the last source row feeding it
//   (AxisTaps::lastSource) has been read. ReadStats::peakSumRows records it.
//
// The weights are those of the whole rectangle (areaTaps), indexed by each source
// pixel's position in it, so tiles need not align with output cells: a source pixel
// adds the same weighted value to the same output pixels whichever tile carries it,
// and an output cell spanning many tiles collects its sum from all of them. The
// result is therefore independent of the budget. It reproduces cv::resize's
// arithmetic for integer samples: at non-integer ratios float weights and a float
// accumulator summed in OpenCV's order (resizeArea); at integer ratios an exact
// block sum times 1.f / area (resizeAreaFast), (sum + 2) >> 2 at 2 x 2; rounding
// half to even (cvRound, through convertTo's saturate_cast). Doubles accumulate in
// double. Float samples are summed in a different order than OpenCV's integer-ratio
// path, so they agree to rounding error rather than bit for bit.
void LIFScene::readTiledDownscale(const cv::Rect& rect, const cv::Size& size, const std::vector<int>& channels,
                                  int z, int t, cv::Mat& out) const
{
    // OpenCV's accumulator: double for doubles; an exact integer sum on the
    // integer-ratio path for integer samples (int there, double here so that no
    // block size can overflow it); float otherwise.
    const int depth = cvDepth();
    const bool integral = depth != CV_16F && depth != CV_32F && depth != CV_64F;
    int scaleX = 0, scaleY = 0;
    if (depth == CV_64F || (integral && areaFast(rect.size(), size, scaleX, scaleY))) {
        readTiledDownscaleAs<double>(rect, size, channels, z, t, out);
    }
    else {
        readTiledDownscaleAs<float>(rect, size, channels, z, t, out);
    }
}

template <typename W>
void LIFScene::readTiledDownscaleAs(const cv::Rect& rect, const cv::Size& size, const std::vector<int>& channels,
                                    int z, int t, cv::Mat& out) const
{
    const int n = static_cast<int>(channels.size());
    const int wDepth = cv::DataType<W>::depth;
    int scaleX = 0, scaleY = 0;
    const bool fast = areaFast(rect.size(), size, scaleX, scaleY);
    const AxisTaps xTaps = fast ? unitTaps(rect.width, scaleX) : areaTaps(rect.width, size.width);
    const AxisTaps yTaps = fast ? unitTaps(rect.height, scaleY) : areaTaps(rect.height, size.height);
    const size_t rowLength = static_cast<size_t>(size.width) * n;
    std::vector<W> row(rowLength, W(0));
    RowWindow<W> window(rowLength);
    const int64_t budget = std::max<int64_t>(1, m_limits.maxResampleSourcePixels);
    const int tileW = static_cast<int>(std::min<int64_t>(rect.width, budget));
    const int tileH = static_cast<int>(std::max<int64_t>(1, std::min<int64_t>(rect.height, budget / tileW)));
    const int depth = cvDepth();
    const bool integral = depth != CV_16F && depth != CV_32F && depth != CV_64F;
    // The integer-ratio path: sum * (1.f / area), except 2 x 2 integer samples,
    // which OpenCV rounds as (sum + 2) >> 2.
    const float areaScale = fast ? 1.f / (scaleX * scaleY) : 1.f;
    const bool halfUp = fast && integral && scaleX == 2 && scaleY == 2;
    out.create(size, CV_MAKETYPE(depth, n));
    int peakRows = 0;

    // Rounds output row `oy` from its sums into `out`: convertTo's saturate_cast is
    // cvRound, half to even, as in cv::resize.
    const auto emit = [&](int oy, const std::vector<W>& sums) {
        cv::Mat target = out.row(oy);
        if (!fast) {
            const cv::Mat wide(1, size.width, CV_MAKETYPE(wDepth, n), const_cast<W*>(sums.data()));
            wide.convertTo(target, target.type());
            return;
        }
        if (depth == CV_64F) {
            cv::Mat scaled(1, size.width, CV_MAKETYPE(CV_64F, n));
            double* r = scaled.ptr<double>(0);
            for (size_t i = 0; i < rowLength; ++i) {
                r[i] = static_cast<double>(sums[i]) * areaScale;
            }
            scaled.convertTo(target, target.type());
            return;
        }
        cv::Mat scaled(1, size.width, CV_MAKETYPE(CV_32F, n));
        float* r = scaled.ptr<float>(0);
        for (size_t i = 0; i < rowLength; ++i) {
            r[i] = halfUp ? static_cast<float>(std::floor((static_cast<double>(sums[i]) + 2) / 4))
                          : static_cast<float>(sums[i]) * areaScale;
        }
        scaled.convertTo(target, target.type());
    };

    for (int ty = 0; ty < rect.height; ty += tileH) {
        const int th = std::min(tileH, rect.height - ty);
        for (int tx = 0; tx < rect.width; tx += tileW) {
            const int tw = std::min(tileW, rect.width - tx);
            cv::Mat tile;
            readRaw(cv::Rect(rect.x + tx, rect.y + ty, tw, th), channels, z, t, tile);
            switch (depth) {
            case CV_8U: accumulateTile<uint8_t, W>(tile, tx, ty, xTaps, yTaps, row, window); break;
            case CV_8S: accumulateTile<int8_t, W>(tile, tx, ty, xTaps, yTaps, row, window); break;
            case CV_16U: accumulateTile<uint16_t, W>(tile, tx, ty, xTaps, yTaps, row, window); break;
            case CV_16S: accumulateTile<int16_t, W>(tile, tx, ty, xTaps, yTaps, row, window); break;
            case CV_32S: accumulateTile<int32_t, W>(tile, tx, ty, xTaps, yTaps, row, window); break;
            case CV_32F: accumulateTile<float, W>(tile, tx, ty, xTaps, yTaps, row, window); break;
            case CV_64F: accumulateTile<double, W>(tile, tx, ty, xTaps, yTaps, row, window); break;
            case CV_16F: {
                cv::Mat wide;
                tile.convertTo(wide, CV_MAKETYPE(CV_32F, n));
                accumulateTile<float, W>(wide, tx, ty, xTaps, yTaps, row, window);
                break;
            }
            default:
                RAISE_RUNTIME_ERROR << "LIF: unexpected sample depth " << depth << " in scene \"" << m_name << "\"";
            }
        }
        peakRows = std::max(peakRows, window.size());
        // Every output row whose last source row is in this strip or above is final.
        const int lastRead = ty + th - 1;
        while (window.base() < size.height && yTaps.lastSource[window.base()] <= lastRead) {
            window.row(window.base());   // a row no source has reached yet is all zero
            emit(window.base(), window.front());
            window.popFront();
        }
    }
    if (window.base() != size.height) {
        RAISE_RUNTIME_ERROR << "LIF: internal error: " << (size.height - window.base())
            << " output rows not emitted in scene \"" << m_name << "\"";
    }
    if (m_limits.stats) {
        m_limits.stats->noteSumRows(peakRows);
    }
}

void ReadStats::noteSource(int64_t pixels)
{
    raiseTo(peakSourcePixels, pixels);
}

void ReadStats::noteSumRows(int64_t rows)
{
    raiseTo(peakSumRows, rows);
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
