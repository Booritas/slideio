// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#pragma once

#include "slideio/drivers/nd2/nd2_api_def.hpp"
#include "slideio/drivers/nd2/nd2variant.hpp"
#include "slideio/core/slideio_enums.hpp"
#include <cstdint>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace slideio
{
    namespace nd2
    {
        /// A file the driver does not support: the message names the offending field
        /// or lists the offending loops. Distinct from RuntimeError, which means a
        /// damaged file.
        class SLIDEIO_ND2_EXPORTS Unsupported : public std::runtime_error
        {
        public:
            using std::runtime_error::runtime_error;
        };

        enum class FrameCompression { None, Zlib };

        /// `ImageAttributes[LV]!` after the SLx wrapper is removed (FD 5).
        struct SLIDEIO_ND2_EXPORTS Attributes
        {
            int width = 0;
            int height = 0;
            /// Samples per pixel: all channels times the components of each (`uiComp`).
            int components = 0;
            int bitsInMemory = 0;
            /// `uiBpcSignificant`; equals bitsInMemory when the file does not say.
            int significantBits = 0;
            /// `uiWidthBytes`: row stride in bytes, padding included.
            int64_t rowStride = 0;
            FrameCompression compression = FrameCompression::None;
            /// `uiSequenceCount`: reported as metadata only (FD 7.4).
            int64_t sequenceCount = 0;

            /// 8 -> DT_Byte, 16 -> DT_UInt16, 32 -> DT_Float32.
            DataType dataType() const;
            int sampleBytes() const { return bitsInMemory / 8; }
            /// Validates as in design 4.4. Throws Unsupported naming the field.
            static Attributes parse(const Variant& attrs);
        };

        /// One slideio channel, which is one component of a frame (design 6.2).
        struct SLIDEIO_ND2_EXPORTS ChannelInfo
        {
            std::string name;
            /// Packed 0x00BBGGRR, as stored in `uiColor`.
            uint32_t color = 0xFFFFFF;
            /// Index of the ND2 channel (optical configuration) this component is in.
            int nd2Channel = 0;
            /// Index of the component within that channel.
            int component = 0;
            /// Probe and optics fields of the plane record, verbatim as text.
            std::vector<std::pair<std::string, std::string>> attributes;
        };

        /// One entry per component of a frame. `pictureMetadata` is the unwrapped
        /// `ImageMetadataSeq[LV]|0!` tree, or nullptr. Without usable plane records
        /// every component is a single-component channel with no name. Throws
        /// Unsupported when `components` is not a multiple of `sPicturePlanes.uiCount`.
        SLIDEIO_ND2_EXPORTS std::vector<ChannelInfo> parseChannels(const Variant* pictureMetadata, int components);

        /// 0x00BBGGRR -> "#RRGGBB".
        SLIDEIO_ND2_EXPORTS std::string colorToHex(uint32_t packedBGR);
    }
}
