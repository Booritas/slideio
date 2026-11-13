// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#pragma once

#include "slideio/drivers/nd2/nd2_api_def.hpp"
#include "slideio/drivers/nd2/nd2attributes.hpp"
#include "slideio/drivers/nd2/nd2chunkfile.hpp"
#include <opencv2/core.hpp>
#include <cstdint>
#include <memory>

namespace slideio
{
    class FileReader;

    namespace nd2
    {
        class LegacyFile;

        /// One frame during one block read. The tiled downscale asks for its tiles top to
        /// bottom and left to right; a cursor keeps the decode state between them, so a
        /// compressed frame is decoded once per read rather than once per tile. Used by
        /// one thread at a time.
        class SLIDEIO_ND2_EXPORTS FrameCursor
        {
        public:
            virtual ~FrameCursor() = default;
            /// As FrameSource::readRows, for the cursor's frame.
            virtual void readRows(const cv::Rect& rect, cv::Mat& out) = 0;
        };

        /// The pixels of the frames of one file (design 7). Immutable after construction
        /// and safe to call from several threads at once: every decode buffer is local
        /// to a call and the only file state is a cursor-free FileReader.
        class SLIDEIO_ND2_EXPORTS FrameSource
        {
        public:
            virtual ~FrameSource() = default;
            /// True when the file holds frame `frame`.
            virtual bool hasFrame(int64_t frame) const = 0;
            /// Rows [rect.y, rect.y+rect.height) and columns [rect.x, rect.x+rect.width) of
            /// `frame`, every component interleaved, into `out` (CV type of the sample
            /// depth with `components` channels). `rect` must lie inside the frame.
            virtual void readRows(int64_t frame, const cv::Rect& rect, cv::Mat& out) const = 0;
            /// A cursor over `frame`, which must be in the file. The default forwards every
            /// call to readRows.
            virtual std::unique_ptr<FrameCursor> openCursor(int64_t frame) const;
        };

        /// The frames of a chunked file: `ImageDataSeq|N!` chunks, uncompressed or zlib.
        /// Keeps `file` alive and copies what it needs from `chunks` and `attrs`.
        SLIDEIO_ND2_EXPORTS std::shared_ptr<const FrameSource> makeChunkFrameSource(
            std::shared_ptr<const FileReader> file, const ChunkFile& chunks, const Attributes& attrs);

        /// The frames of a legacy JPEG 2000 file (design 4.7): frame i, ND2 channel c is the
        /// (i*channelCount + c)-th codestream, and an RGB file (`ihdr` 3 components) has one
        /// codestream per channel. A missing codestream reads as zeros; a frame exists when at
        /// least one of its codestreams does. Keeps `file` alive.
        SLIDEIO_ND2_EXPORTS std::shared_ptr<const FrameSource> makeJp2FrameSource(
            std::shared_ptr<const FileReader> file, const LegacyFile& legacy, int channelCount);
    }
}
