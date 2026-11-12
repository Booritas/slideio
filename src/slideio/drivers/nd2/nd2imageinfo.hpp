// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#pragma once

#include "slideio/drivers/nd2/nd2_api_def.hpp"
#include "slideio/drivers/nd2/nd2attributes.hpp"
#include "slideio/drivers/nd2/nd2chunkfile.hpp"
#include "slideio/drivers/nd2/nd2experiment.hpp"
#include "slideio/drivers/nd2/nd2variant.hpp"
#include <cstdint>
#include <functional>
#include <string>
#include <vector>

namespace slideio
{
    class FileReader;

    namespace nd2
    {
        /// One acquired XY position, which becomes one scene (design 4.6).
        struct SLIDEIO_ND2_EXPORTS Position
        {
            /// Scene name: `dPosName`, else `Position <k+1>`, made unique with ` (<k+1>)`;
            /// the file stem when the file has no XY loop.
            std::string name;
            /// The point record (`Points.i<item>`) as JSON; empty without an XY loop.
            std::string rawJson;
        };

        /// Everything a chunked ND2 file says about its image, decoded once (design 6).
        struct SLIDEIO_ND2_EXPORTS ImageInfo
        {
            Attributes attributes;
            std::vector<ChannelInfo> channels;
            Experiment experiment;
            /// One per valid XY point, in loop order; size == experiment.count(Axis::P).
            std::vector<Position> positions;
            /// Metres; 0 when the file is not calibrated.
            double pixelSizeX = 0;
            double pixelSizeY = 0;
            /// Metres and seconds; 0 when unknown.
            double zStep = 0;
            double tStep = 0;
            double magnification = 0;
            /// Julian day of the acquisition start; 0 = unknown.
            double absoluteJulianDay = 0;
            /// Milliseconds by frame index (length = the loop product). Each entry is the
            /// `CustomData|AcqTimesCache!` value when the cache covers that index; entries past
            /// the end of a short (or absent) cache come from the frame's own 8-byte prefix
            /// when the frame exists. NaN where neither exists.
            std::vector<double> frameTimesMs;
            /// Slide raw metadata (design 6.5).
            std::string rawJson;
        };

        /// Throws Unsupported for an unsupported file and RuntimeError for a damaged one.
        SLIDEIO_ND2_EXPORTS ImageInfo buildImageInfo(const ChunkFile& chunks, const FileReader& file,
            const std::string& fileStem);

        /// Z step in metres from the parameters of a Z loop: |dZStep|, or when that is 0
        /// and `uiCount` > 1, |dZHigh - dZLow| / (uiCount - 1). The fields are micrometres. Each
        /// field may also be spelled without its type prefix (`ZStep`, `Count`...), as in the
        /// legacy fixtures.
        SLIDEIO_ND2_EXPORTS double zStepFrom(const Variant& zLoopPars);

        /// Frame times (ms) for `count` frames: the first cache.size() entries come from `cache`
        /// (clipped to count); the rest from `prefixTime(frame, out)` where it returns true, else NaN.
        SLIDEIO_ND2_EXPORTS std::vector<double> mergeFrameTimes(const std::vector<double>& cache, int64_t count,
            const std::function<bool(int64_t, double&)>& prefixTime);

        /// Mean difference in seconds of consecutive per-T times (ms) at p=0, z=0; 0 when
        /// T < 2 or any of those times is NaN. `times` is indexed by frame.
        SLIDEIO_ND2_EXPORTS double meanStepSeconds(const Experiment& experiment, const std::vector<double>& times);
    }
}
