// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#pragma once
#include "slideio/core/slideio_core_def.hpp"
#include <opencv2/core.hpp>
#include <atomic>
#include <cstdint>
#include <functional>
#include <memory>

#if defined(_MSC_VER)
#pragma warning( push )
#pragma warning(disable: 4251)
#endif

namespace slideio
{
    /// What reads have done, for tests: shared by every scene opened with the
    /// ResampleLimits that carry it.
    struct SLIDEIO_CORE_EXPORTS ResampleStats
    {
        /// The most source pixels one read assembled into one buffer.
        std::atomic<int64_t> peakSourcePixels{0};
        /// The most output rows one tiled downscale held sums for at once.
        std::atomic<int64_t> peakSumRows{0};
        void noteSource(int64_t pixels);
        void noteSumRows(int64_t rows);
    };

    /// Bounds of a read. A downscaling read whose source rectangle holds more pixels
    /// than maxResampleSourcePixels is read in tiles of at most that many pixels and
    /// area-filtered tile by tile on the global sampling grid, so memory stays
    /// bounded on large mosaics and the result does not depend on the budget.
    struct ResampleLimits
    {
        int64_t maxResampleSourcePixels = int64_t(16) << 20;
        /// Optional: when set, reads record into it.
        std::shared_ptr<ResampleStats> stats;
    };

    /// Reads `rect` (source coordinates) at native size into `tile`, `channels`
    /// interleaved planes.
    using TileReader = std::function<void(const cv::Rect& rect, cv::Mat& tile)>;

    /// True when a read of a `rect`-sized region to `size` must go through
    /// tiledAreaDownscale: a downscale (no larger on either axis, and not the
    /// identity) of a source holding more pixels than the budget.
    SLIDEIO_CORE_EXPORTS bool needsTiledDownscale(const cv::Size& rect, const cv::Size& size,
                                                  const ResampleLimits& limits);

    /// Downscales `rect` to `size`, no larger on either axis, with the area filter of
    /// cv::resize(INTER_AREA), reading the source in tiles through `read` within
    /// `limits`. `cvDepth` is the sample depth (CV_8U ...) and `channels` the number
    /// of interleaved channels `read` returns.
    SLIDEIO_CORE_EXPORTS void tiledAreaDownscale(const cv::Rect& rect, const cv::Size& size, int cvDepth,
                                                 int channels, const ResampleLimits& limits,
                                                 const TileReader& read, cv::Mat& out);
}

#if defined(_MSC_VER)
#pragma warning( pop )
#endif
