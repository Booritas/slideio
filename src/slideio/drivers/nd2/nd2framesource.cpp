// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#include "slideio/drivers/nd2/nd2framesource.hpp"
#include "slideio/core/exceptions.hpp"
#include "slideio/core/tools/cvtools.hpp"
#include "slideio/core/tools/endian.hpp"
#include "slideio/core/tools/filereader.hpp"

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

    // Samples are little-endian in the file (design 7). slideio builds for no
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

    /// Ends a zlib inflate stream however the read leaves.
    struct InflateGuard
    {
        z_stream& stream;
        ~InflateGuard() { inflateEnd(&stream); }
    };

    class ChunkFrameSource : public FrameSource
    {
    public:
        ChunkFrameSource(std::shared_ptr<const FileReader> file, const ChunkFile& chunks, const Attributes& attrs)
            : m_file(std::move(file)),
              m_frames(chunks.frames()),
              m_attrs(attrs),
              m_sampleBytes(attrs.sampleBytes()),
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
            out.create(rect.height, rect.width, m_cvType);
            if (m_attrs.compression == FrameCompression::Zlib) {
                readZlib(frame, it->second, rect, out);
            }
            else {
                readRaw(frame, it->second, rect, out);
            }
            for (int y = 0; y < out.rows; ++y) {
                toNative(out.ptr(y), static_cast<size_t>(rect.width) * m_attrs.components, m_sampleBytes);
            }
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

        [[noreturn]] static void corrupt(int64_t frame)
        {
            RAISE_RUNTIME_ERROR << "ND2: frame " << frame << ": corrupt zlib stream";
        }

        // Inflates the frame row by row into a one-row buffer: rows before rect.y are
        // discarded and the stream is left after the last row needed. When that is the
        // frame's last row, the stream must end exactly there.
        void readZlib(int64_t frame, const ChunkHeader& chunk, const cv::Rect& rect, cv::Mat& out) const
        {
            if (chunk.dataLength < kTimePrefix) {
                corrupt(frame);
            }
            const uint64_t compressedSize = chunk.dataLength - kTimePrefix;
            if (compressedSize > std::numeric_limits<uInt>::max()
                || m_attrs.rowStride > static_cast<int64_t>(std::numeric_limits<uInt>::max())) {
                RAISE_RUNTIME_ERROR << "ND2: frame " << frame << " is too large for a zlib frame";
            }
            std::vector<uint8_t> compressed(static_cast<size_t>(compressedSize));
            if (!compressed.empty()) {
                m_file->readAt(chunk.dataOffset() + kTimePrefix, compressed.data(), compressed.size());
            }
            z_stream stream{};
            if (inflateInit(&stream) != Z_OK) {
                RAISE_RUNTIME_ERROR << "ND2: frame " << frame << ": cannot initialise zlib";
            }
            InflateGuard guard{stream};
            stream.next_in = compressed.data();
            stream.avail_in = static_cast<uInt>(compressed.size());

            std::vector<uint8_t> row(static_cast<size_t>(m_attrs.rowStride));
            const size_t rowBytes = static_cast<size_t>(rect.width * m_pixelBytes);
            const size_t column = static_cast<size_t>(rect.x * m_pixelBytes);
            const int lastRow = rect.y + rect.height - 1;
            bool ended = false;
            for (int y = 0; y <= lastRow; ++y) {
                if (ended) {
                    corrupt(frame);   // the stream ended before this row
                }
                stream.next_out = row.data();
                stream.avail_out = static_cast<uInt>(row.size());
                while (stream.avail_out > 0) {
                    const int ret = inflate(&stream, Z_NO_FLUSH);
                    if (ret == Z_STREAM_END) {
                        ended = true;
                        break;
                    }
                    if (ret != Z_OK) {
                        corrupt(frame);   // damaged data, or input exhausted (Z_BUF_ERROR)
                    }
                }
                if (stream.avail_out > 0) {
                    corrupt(frame);       // ended part-way through a row
                }
                if (y >= rect.y) {
                    std::memcpy(out.ptr(y - rect.y), row.data() + column, rowBytes);
                }
            }
            if (ended && lastRow < m_attrs.height - 1) {
                corrupt(frame);           // the stream holds fewer rows than the frame
            }
            if (lastRow == m_attrs.height - 1 && !ended) {
                // The whole frame has been inflated: nothing may follow it.
                uint8_t extra = 0;
                stream.next_out = &extra;
                stream.avail_out = 1;
                const int ret = inflate(&stream, Z_NO_FLUSH);
                if (ret != Z_STREAM_END || stream.avail_out != 1) {
                    corrupt(frame);
                }
            }
        }

        std::shared_ptr<const FileReader> m_file;
        std::map<int64_t, ChunkHeader> m_frames;
        Attributes m_attrs;
        int m_sampleBytes;
        int64_t m_pixelBytes;
        int m_cvType;
    };
}

std::shared_ptr<const FrameSource> slideio::nd2::makeChunkFrameSource(
    std::shared_ptr<const FileReader> file, const ChunkFile& chunks, const Attributes& attrs)
{
    return std::make_shared<const ChunkFrameSource>(std::move(file), chunks, attrs);
}
