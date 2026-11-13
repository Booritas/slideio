// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#pragma once

#include "slideio/drivers/nd2/nd2_api_def.hpp"
#include "slideio/drivers/nd2/nd2attributes.hpp"
#include "slideio/drivers/nd2/nd2chunkfile.hpp"
#include "slideio/drivers/nd2/nd2experiment.hpp"
#include "slideio/drivers/nd2/nd2frametimes.hpp"
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
            /// Metres; 0 when unknown.
            double zStep = 0;
            /// Seconds: the time loop's nominal period, 0 when unknown. A scene reports
            /// its own measured step first (timeStepSeconds, design 6.3).
            double tStep = 0;
            double magnification = 0;
            /// Julian day of the acquisition start; 0 = unknown.
            double absoluteJulianDay = 0;
            /// Milliseconds by frame index (size = the loop product). Each time is the
            /// `CustomData|AcqTimesCache!` value when the cache covers that index; past the
            /// end of a short (or absent) cache it is the frame's own 8-byte prefix when the
            /// frame exists, read when first asked for. NaN where neither exists.
            FrameTimes frameTimesMs;
            /// Slide raw metadata (design 6.5).
            std::string rawJson;
        };

        /// Throws Unsupported for an unsupported file and RuntimeError for a damaged one.
        /// Keeps `file` for the frame times it reads later.
        SLIDEIO_ND2_EXPORTS ImageInfo buildImageInfo(const ChunkFile& chunks, std::shared_ptr<const FileReader> file,
            const std::string& fileStem);

        /// Z step in metres from the parameters of a Z loop: |dZStep|, or when that is 0
        /// and `uiCount` > 1, |dZHigh - dZLow| / (uiCount - 1). The fields are micrometres. Each
        /// field may also be spelled without its type prefix (`ZStep`, `Count`...), as in the
        /// legacy fixtures.
        SLIDEIO_ND2_EXPORTS double zStepFrom(const Variant& zLoopPars);

        /// The magnification an objective name states: the number immediately before the
        /// first 'x' or 'X' that ends a number beginning a word ("Plan Apo 60x Oil" -> 60,
        /// "20X" -> 20, "1.4x" -> 1.4, "SR HP Apo TIRF 100xAC" -> 100); 0 when there is
        /// none. Used when dObjectiveMag is not > 0 (design 6.1, R27).
        SLIDEIO_ND2_EXPORTS double magnificationFromObjectiveName(const std::string& name);

        /// The T increment in seconds of the scene at XY ordinal `p` (design 6.3, R28, R30):
        /// meanStepSeconds at that p when it is > 0, else the time loop's nominal period
        /// (`dPeriod`, or the first valid NE period); 0 without a time loop. With empty
        /// `times` it is the nominal period alone.
        SLIDEIO_ND2_EXPORTS double timeStepSeconds(const Experiment& experiment, const FrameTimes& times,
            int64_t p = 0);

        /// Mean difference in seconds of consecutive per-T times (ms) at XY ordinal `p`,
        /// z = 0; 0 when T < 2, p is out of range or any of those times is NaN. `times` is
        /// indexed by frame.
        SLIDEIO_ND2_EXPORTS double meanStepSeconds(const Experiment& experiment, const FrameTimes& times,
            int64_t p = 0);
    }
}
