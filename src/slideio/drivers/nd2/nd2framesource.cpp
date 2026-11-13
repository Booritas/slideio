// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#include "slideio/drivers/nd2/nd2framesource.hpp"
#include "slideio/drivers/nd2/nd2legacy.hpp"
#include "slideio/core/exceptions.hpp"
#include "slideio/core/tools/cvtools.hpp"
#include "slideio/core/tools/endian.hpp"
#include "slideio/core/tools/filereader.hpp"
#include "slideio/imagetools/imagetools.hpp"

#include <zlib.h>
#include <algorithm>
#include <cstring>
#include <limits>
#include <map>
#include <vector>

using namespace slideio;
using namespace slideio::nd2;

namespace
{
    /// Every frame chunk starts its data with an 8-byte float64 time (FD 6).
    constexpr uint64_t kTimePrefix = 8;

    /// Samples are little-endian in the file (design 7). slideio builds for no
    /// big-endian target today, so this is a guard rather than a tested path.
    void toNative(cv::Mat& out, DataType type)
    {
        const size_t rowBytes = static_cast<size_t>(out.cols) * out.elemSize();
        for (int y = 0; y < out.rows; ++y) {
            Endian::fromLittleEndianToNative(type, out.ptr(y), rowBytes);
        }
    }

    [[noreturn]] void corruptZlib(int64_t frame)
    {
        RAISE_RUNTIME_ERROR << "ND2: frame " << frame << ": corrupt zlib stream";
    }

    /// One zlib frame during one block read. The compressed data is streamed from the
    /// file in blocks, never held whole, and rows are inflated top to bottom. The rows
    /// of the last band asked for are kept at full width, so the tiles the tiled
    /// downscale reads left to right along one band inflate it once, and the next band
    /// down continues the stream. Only a band above the stream's position restarts it.
    class ZlibCursor : public FrameCursor
    {
    public:
        ZlibCursor(std::shared_ptr<const FileReader> file, int64_t frame, const ChunkHeader& chunk,
            const Attributes& attrs, int64_t pixelBytes, int cvType)
            : m_file(std::move(file)), m_frame(frame), m_attrs(attrs), m_pixelBytes(pixelBytes), m_cvType(cvType)
        {
            if (chunk.dataLength < kTimePrefix) {
                corruptZlib(frame);
            }
            if (m_attrs.rowStride > static_cast<int64_t>(std::numeric_limits<uInt>::max())) {
                RAISE_RUNTIME_ERROR << "ND2: frame " << frame << " is too large for a zlib frame";
            }
            m_inputStart = chunk.dataOffset() + kTimePrefix;
            m_inputEnd = chunk.dataOffset() + chunk.dataLength;
        }

        ~ZlibCursor() override
        {
            if (m_open) {
                inflateEnd(&m_stream);
            }
        }

        void readRows(const cv::Rect& rect, cv::Mat& out) override
        {
            const size_t stride = static_cast<size_t>(m_attrs.rowStride);
            if (m_bandRows == 0 || rect.y < m_bandY || rect.y + rect.height > m_bandY + m_bandRows) {
                if (!m_open || rect.y < m_nextRow) {
                    restart();
                }
                m_scratch.resize(stride);
                while (m_nextRow < rect.y) {
                    inflateRow(m_scratch.data());   // rows above the band are discarded
                }
                m_band.resize(stride * static_cast<size_t>(rect.height));
                for (int y = 0; y < rect.height; ++y) {
                    inflateRow(m_band.data() + stride * static_cast<size_t>(y));
                }
                m_bandY = rect.y;
                m_bandRows = rect.height;
            }
            out.create(rect.height, rect.width, m_cvType);
            const size_t rowBytes = static_cast<size_t>(rect.width * m_pixelBytes);
            const size_t column = static_cast<size_t>(rect.x * m_pixelBytes);
            for (int y = 0; y < rect.height; ++y) {
                std::memcpy(out.ptr(y),
                    m_band.data() + stride * static_cast<size_t>(rect.y - m_bandY + y) + column, rowBytes);
            }
            toNative(out, m_attrs.dataType());
        }

    private:
        static constexpr size_t kInputBlock = 1u << 20;

        void restart()
        {
            if (m_open) {
                inflateEnd(&m_stream);
                m_open = false;
            }
            m_stream = z_stream{};
            if (inflateInit(&m_stream) != Z_OK) {
                RAISE_RUNTIME_ERROR << "ND2: frame " << m_frame << ": cannot initialise zlib";
            }
            m_open = true;
            m_inputPos = m_inputStart;
            m_nextRow = 0;
            m_ended = false;
            m_bandRows = 0;
        }

        /// Hands zlib the next block of compressed data once it has used the last one.
        void refill()
        {
            if (m_stream.avail_in > 0 || m_inputPos >= m_inputEnd) {
                return;
            }
            const size_t n = static_cast<size_t>(std::min<uint64_t>(kInputBlock, m_inputEnd - m_inputPos));
            m_input.resize(n);
            m_file->readAt(m_inputPos, m_input.data(), n);
            m_inputPos += n;
            m_stream.next_in = m_input.data();
            m_stream.avail_in = static_cast<uInt>(n);
        }

        bool inputLeft() const
        {
            return m_stream.avail_in > 0 || m_inputPos < m_inputEnd;
        }

        // The next row of the frame into `dst`. After the frame's last row the stream
        // must end exactly there; a stream that ends earlier is corrupt.
        void inflateRow(uint8_t* dst)
        {
            if (m_ended || m_nextRow >= m_attrs.height) {
                corruptZlib(m_frame);   // the stream ended before this row
            }
            m_stream.next_out = dst;
            m_stream.avail_out = static_cast<uInt>(m_attrs.rowStride);
            while (m_stream.avail_out > 0) {
                refill();
                const int ret = inflate(&m_stream, Z_NO_FLUSH);
                if (ret == Z_STREAM_END) {
                    m_ended = true;
                    break;
                }
                if (ret != Z_OK) {
                    corruptZlib(m_frame);   // damaged data, or input exhausted (Z_BUF_ERROR)
                }
            }
            if (m_stream.avail_out > 0) {
                corruptZlib(m_frame);       // ended part-way through a row
            }
            ++m_nextRow;
            if (m_ended && m_nextRow < m_attrs.height) {
                corruptZlib(m_frame);       // the stream holds fewer rows than the frame
            }
            if (m_nextRow == m_attrs.height && !m_ended) {
                // The whole frame has been inflated: nothing may follow it.
                uint8_t extra = 0;
                while (true) {
                    refill();
                    m_stream.next_out = &extra;
                    m_stream.avail_out = 1;
                    const int ret = inflate(&m_stream, Z_NO_FLUSH);
                    if (ret == Z_STREAM_END && m_stream.avail_out == 1) {
                        m_ended = true;
                        break;
                    }
                    if (ret != Z_OK || m_stream.avail_out != 1 || !inputLeft()) {
                        corruptZlib(m_frame);
                    }
                }
            }
        }

        std::shared_ptr<const FileReader> m_file;
        int64_t m_frame;
        const Attributes& m_attrs;
        int64_t m_pixelBytes;
        int m_cvType;
        uint64_t m_inputStart = 0;
        uint64_t m_inputEnd = 0;
        uint64_t m_inputPos = 0;
        z_stream m_stream{};
        bool m_open = false;
        bool m_ended = false;
        int m_nextRow = 0;
        std::vector<uint8_t> m_input;
        std::vector<uint8_t> m_scratch;
        std::vector<uint8_t> m_band;
        int m_bandY = 0;
        int m_bandRows = 0;
    };

    class ChunkFrameSource : public FrameSource
    {
    public:
        ChunkFrameSource(std::shared_ptr<const FileReader> file, const ChunkFile& chunks, const Attributes& attrs)
            : m_file(std::move(file)),
              m_frames(chunks.frames()),
              m_attrs(attrs),
              m_pixelBytes(static_cast<int64_t>(attrs.components) * attrs.sampleBytes()),
              m_cvType(CV_MAKETYPE(CVTools::cvTypeFromDataType(attrs.dataType()), attrs.components))
        {
        }

        bool hasFrame(int64_t frame) const override
        {
            return m_frames.count(frame) != 0;
        }

        void readRows(int64_t frame, const cv::Rect& rect, cv::Mat& out) const override
        {
            const auto it = m_frames.find(frame);
            if (it == m_frames.end()) {
                RAISE_RUNTIME_ERROR << "ND2: frame " << frame << " is not in the file";
            }
            if (rect.x < 0 || rect.y < 0 || rect.width <= 0 || rect.height <= 0
                || rect.x + rect.width > m_attrs.width || rect.y + rect.height > m_attrs.height) {
                RAISE_RUNTIME_ERROR << "ND2: frame " << frame << ": rectangle (" << rect.x << ", " << rect.y
                    << ", " << rect.width << ", " << rect.height << ") is outside the "
                    << m_attrs.width << "x" << m_attrs.height << " frame";
            }
            if (m_attrs.compression == FrameCompression::Zlib) {
                ZlibCursor(m_file, frame, it->second, m_attrs, m_pixelBytes, m_cvType).readRows(rect, out);
                return;
            }
            out.create(rect.height, rect.width, m_cvType);
            readRaw(frame, it->second, rect, out);
            toNative(out, m_attrs.dataType());
        }

        std::unique_ptr<FrameCursor> openCursor(int64_t frame) const override
        {
            const auto it = m_frames.find(frame);
            if (it == m_frames.end() || m_attrs.compression != FrameCompression::Zlib) {
                return FrameSource::openCursor(frame);
            }
            return std::make_unique<ZlibCursor>(m_file, frame, it->second, m_attrs, m_pixelBytes, m_cvType);
        }

    private:
        void readRaw(int64_t frame, const ChunkHeader& chunk, const cv::Rect& rect, cv::Mat& out) const
        {
            const uint64_t rowStride = static_cast<uint64_t>(m_attrs.rowStride);
            const uint64_t needed = kTimePrefix + rowStride * static_cast<uint64_t>(m_attrs.height);
            if (chunk.dataLength < needed) {
                RAISE_RUNTIME_ERROR << "ND2: frame " << frame << ": the chunk holds " << chunk.dataLength
                    << " bytes but the image needs " << needed;
            }
            const uint64_t pixelStart = chunk.dataOffset() + kTimePrefix;
            const size_t rowBytes = static_cast<size_t>(rect.width * m_pixelBytes);
            const uint64_t column = static_cast<uint64_t>(rect.x) * static_cast<uint64_t>(m_pixelBytes);
            const bool band = rect.x == 0 && rect.width == m_attrs.width
                && rowStride == rowBytes && out.isContinuous();
            if (band) {
                // Rows follow one another without padding: one read for the band.
                m_file->readAt(pixelStart + static_cast<uint64_t>(rect.y) * rowStride, out.data,
                    rowBytes * static_cast<size_t>(rect.height));
                return;
            }
            for (int y = 0; y < rect.height; ++y) {
                m_file->readAt(pixelStart + static_cast<uint64_t>(rect.y + y) * rowStride + column,
                    out.ptr(y), rowBytes);
            }
        }

        std::shared_ptr<const FileReader> m_file;
        std::map<int64_t, ChunkHeader> m_frames;
        Attributes m_attrs;
        int64_t m_pixelBytes;
        int m_cvType;
    };

    /// Legacy JPEG 2000 frames (design 4.7). A read decodes the whole codestream of every
    /// channel of the frame, then copies the rectangle out of it; a cursor decodes them
    /// once and copies every tile of its read out of the same decoded frame.
    class Jp2FrameSource : public FrameSource
    {
    public:
        Jp2FrameSource(std::shared_ptr<const FileReader> file, const LegacyFile& legacy, int channelCount)
            : m_file(std::move(file)),
              m_legacy(legacy),
              m_channels(channelCount),
              m_components(legacy.components()),
              m_depth(legacy.bits() == 8 ? CV_8U : CV_16U)
        {
            if (m_channels < 1 || m_components < 1 || m_channels > CV_CN_MAX / m_components) {
                RAISE_RUNTIME_ERROR << "ND2: " << m_channels << " channels of " << m_components
                    << " components each cannot be read";
            }
        }

        bool hasFrame(int64_t frame) const override
        {
            if (frame < 0 || frame > std::numeric_limits<int64_t>::max() / m_channels) {
                return false;
            }
            return frame * m_channels < static_cast<int64_t>(m_legacy.codestreams().size());
        }

        void readRows(int64_t frame, const cv::Rect& rect, cv::Mat& out) const override
        {
            checkRect(frame, rect);
            copyRect(decodeFrame(frame), rect, out);
        }

        std::unique_ptr<FrameCursor> openCursor(int64_t frame) const override;

        void checkRect(int64_t frame, const cv::Rect& rect) const
        {
            if (!hasFrame(frame)) {
                RAISE_RUNTIME_ERROR << "ND2: frame " << frame << " is not in the file";
            }
            const int width = m_legacy.width();
            const int height = m_legacy.height();
            if (rect.x < 0 || rect.y < 0 || rect.width <= 0 || rect.height <= 0
                || rect.x + rect.width > width || rect.y + rect.height > height) {
                RAISE_RUNTIME_ERROR << "ND2: frame " << frame << ": rectangle (" << rect.x << ", " << rect.y
                    << ", " << rect.width << ", " << rect.height << ") is outside the "
                    << width << "x" << height << " frame";
            }
        }

        /// Every channel's codestream of `frame`, decoded; an empty Mat for a missing one.
        std::vector<cv::Mat> decodeFrame(int64_t frame) const
        {
            const int width = m_legacy.width();
            const int height = m_legacy.height();
            const std::vector<Box>& streams = m_legacy.codestreams();
            std::vector<cv::Mat> channels(static_cast<size_t>(m_channels));
            for (int c = 0; c < m_channels; ++c) {
                const int64_t index = frame * m_channels + c;
                if (index >= static_cast<int64_t>(streams.size())) {
                    continue;   // a missing codestream reads as background
                }
                cv::Mat& decoded = channels[static_cast<size_t>(c)];
                try {
                    const std::vector<uint8_t> data = m_legacy.readBox(*m_file, streams[static_cast<size_t>(index)]);
                    ImageTools::decodeJp2KStream(data.data(), data.size(), decoded);
                }
                catch (const std::exception& e) {
                    RAISE_RUNTIME_ERROR << "ND2: frame " << frame << " channel " << c
                        << ": cannot decode the JPEG 2000 codestream: " << e.what();
                }
                if (decoded.cols != width || decoded.rows != height || decoded.channels() != m_components
                    || decoded.depth() != m_depth) {
                    RAISE_RUNTIME_ERROR << "ND2: frame " << frame << " channel " << c << ": the codestream decodes to "
                        << decoded.cols << "x" << decoded.rows << " with " << decoded.channels()
                        << " components of CV depth " << decoded.depth() << ", but ihdr says " << width << "x"
                        << height << " with " << m_components << " components of " << m_legacy.bits() << " bits";
                }
            }
            return channels;
        }

        void copyRect(const std::vector<cv::Mat>& channels, const cv::Rect& rect, cv::Mat& out) const
        {
            out.create(rect.height, rect.width, CV_MAKETYPE(m_depth, m_channels * m_components));
            out.setTo(cv::Scalar::all(0));
            for (int c = 0; c < m_channels; ++c) {
                const cv::Mat& decoded = channels[static_cast<size_t>(c)];
                if (decoded.empty()) {
                    continue;
                }
                const cv::Mat roi = decoded(rect);
                std::vector<int> pairs;
                for (int j = 0; j < m_components; ++j) {
                    pairs.push_back(j);
                    pairs.push_back(c * m_components + j);
                }
                cv::mixChannels(&roi, 1, &out, 1, pairs.data(), static_cast<size_t>(m_components));
            }
        }

    private:
        std::shared_ptr<const FileReader> m_file;
        LegacyFile m_legacy;
        int m_channels;
        int m_components;
        int m_depth;
    };

    class Jp2Cursor : public FrameCursor
    {
    public:
        Jp2Cursor(const Jp2FrameSource& source, int64_t frame) : m_source(source), m_frame(frame) {}

        void readRows(const cv::Rect& rect, cv::Mat& out) override
        {
            m_source.checkRect(m_frame, rect);
            if (!m_decoded) {
                m_channels = m_source.decodeFrame(m_frame);
                m_decoded = true;
            }
            m_source.copyRect(m_channels, rect, out);
        }

    private:
        const Jp2FrameSource& m_source;
        int64_t m_frame;
        bool m_decoded = false;
        std::vector<cv::Mat> m_channels;
    };

    std::unique_ptr<FrameCursor> Jp2FrameSource::openCursor(int64_t frame) const
    {
        return std::make_unique<Jp2Cursor>(*this, frame);
    }

    /// FrameSource's default cursor: no state, every call is a readRows.
    class ForwardingCursor : public FrameCursor
    {
    public:
        ForwardingCursor(const FrameSource& source, int64_t frame) : m_source(source), m_frame(frame) {}

        void readRows(const cv::Rect& rect, cv::Mat& out) override
        {
            m_source.readRows(m_frame, rect, out);
        }

    private:
        const FrameSource& m_source;
        int64_t m_frame;
    };
}

std::unique_ptr<FrameCursor> FrameSource::openCursor(int64_t frame) const
{
    return std::make_unique<ForwardingCursor>(*this, frame);
}

std::shared_ptr<const FrameSource> slideio::nd2::makeChunkFrameSource(
    std::shared_ptr<const FileReader> file, const ChunkFile& chunks, const Attributes& attrs)
{
    return std::make_shared<const ChunkFrameSource>(std::move(file), chunks, attrs);
}

std::shared_ptr<const FrameSource> slideio::nd2::makeJp2FrameSource(
    std::shared_ptr<const FileReader> file, const LegacyFile& legacy, int channelCount)
{
    return std::make_shared<const Jp2FrameSource>(std::move(file), legacy, channelCount);
}
