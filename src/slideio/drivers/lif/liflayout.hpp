// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#pragma once

#include "slideio/drivers/lif/lif_api_def.hpp"
#include "slideio/drivers/lif/lifxml.hpp"
#include "slideio/core/slideio_enums.hpp"
#include <array>
#include <cstdint>
#include <stdexcept>
#include <vector>

#if defined(_MSC_VER)
#pragma warning(push)
#pragma warning(disable: 4251 4275)
#endif

namespace slideio
{
    namespace lif
    {
        /// An image the driver does not read. what() is the reason, phrased for a
        /// user and without a "LIF:" prefix: the caller names the image.
        class SLIDEIO_LIF_EXPORTS Unsupported : public std::runtime_error
        {
        public:
            using std::runtime_error::runtime_error;
        };

        /// Pixel spacing in metres (x, y, z) and frame interval in seconds (t);
        /// 0 means unknown.
        struct Calibration
        {
            double x = 0, y = 0, z = 0, t = 0;
        };

        /// Where every pixel of one image lives in its memory block (spec D 5),
        /// and its calibration (D 6.2). Built and validated once; every offset it
        /// returns afterwards is inside the payload.
        class SLIDEIO_LIF_EXPORTS Layout
        {
        public:
            /// Throws Unsupported when the image cannot be read correctly: an
            /// unknown sample type, an extra axis with more than one element,
            /// overlapping planes, an offset outside the payload or past 2^64.
            static Layout build(const ImageDescription& image, uint64_t payloadLength);

            int width() const { return m_width; }
            int height() const { return m_height; }
            /// Scene channels: 3 for interleaved RGB.
            int numChannels() const { return m_numChannels; }
            /// 1 when the axis is absent or is the display Y of an XZ/XT scan.
            int numZ() const { return m_numZ; }
            int numT() const { return m_numT; }
            int numFields() const { return m_numFields; }
            DataType dataType() const { return m_dataType; }
            int sampleBytes() const { return m_sampleBytes; }
            /// ChannelDescription/@Resolution of the scene channel; the storage
            /// width in bits when the attribute is absent.
            int significantBits(int channel) const;
            bool interleaved() const { return m_interleaved; }
            /// Scene channel -> index of its ChannelDescription (document order).
            /// Identity for scalar images; R, G, B for interleaved ones.
            int storedSample(int channel) const;
            /// X BytesInc.
            int64_t pixelStride() const { return static_cast<int64_t>(m_xStride); }
            /// Y BytesInc (of the display Y axis).
            int64_t rowStride() const { return static_cast<int64_t>(m_yStride); }
            /// Byte offset of pixel (0, 0) of a plane, relative to the payload
            /// start. channel is a scene channel; for interleaved RGB the result
            /// addresses that colour's sample within the first pixel.
            uint64_t planeOffset(int channel, int z, int t, int field) const;
            /// Rank of the plane in storage order: the stacked axes with more
            /// than one element (scalar C, Z, T, M), sorted by stride, fastest
            /// first (D 6.3). The three samples of an interleaved pixel share a
            /// plane.
            int64_t storageIndex(int channel, int z, int t, int field) const;
            Calibration calibration() const { return m_calibration; }

        private:
            enum class Axis { C, Z, T, M };
            struct StackAxis
            {
                Axis axis;
                uint64_t count;
                uint64_t stride;
            };
            void checkIndices(int channel, int z, int t, int field) const;

            int m_width = 0;
            int m_height = 0;
            int m_numChannels = 0;
            int m_numZ = 1;
            int m_numT = 1;
            int m_numFields = 1;
            DataType m_dataType = DataType::DT_Unknown;
            int m_sampleBytes = 0;
            bool m_interleaved = false;
            std::vector<int> m_bits;              // per ChannelDescription
            std::vector<uint64_t> m_channelInc;   // per ChannelDescription
            std::array<int, 3> m_rgb{{0, 1, 2}};  // interleaved: scene -> description
            uint64_t m_channelBase = 0;           // scalar: smallest channel increment
            uint64_t m_channelSpacing = 0;        // scalar: channel axis stride
            uint64_t m_xStride = 0;
            uint64_t m_yStride = 0;
            uint64_t m_zStride = 0;
            uint64_t m_tStride = 0;
            uint64_t m_mStride = 0;
            std::vector<StackAxis> m_storageOrder;   // fastest first
            Calibration m_calibration;
        };
    }
}

#if defined(_MSC_VER)
#pragma warning(pop)
#endif
