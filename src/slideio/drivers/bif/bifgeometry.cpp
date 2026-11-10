// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#include "slideio/drivers/bif/bifgeometry.hpp"
#include "slideio/core/exceptions.hpp"
#include "slideio/core/log.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>

namespace slideio::bif
{
    namespace
    {
        // Mean of the two middle values when the count is even.
        std::optional<double> median(std::vector<double> values)
        {
            if (values.empty()) {
                return std::nullopt;
            }
            const size_t mid = values.size() / 2;
            std::nth_element(values.begin(), values.begin() + mid, values.end());
            const double upper = values[mid];
            if (values.size() % 2 == 1) {
                return upper;
            }
            const double lower = *std::max_element(values.begin(), values.begin() + mid);
            return (lower + upper) / 2.;
        }

        double overlapAlong(const Joint& joint, JointAxis axis)
        {
            return axis == JointAxis::Horizontal ? joint.overlapX : joint.overlapY;
        }

        const char* axisName(JointAxis axis)
        {
            return axis == JointAxis::Horizontal ? "column" : "row";
        }

        void checkGrid(const Aoi& aoi)
        {
            if (aoi.cols < 1 || aoi.rows < 1) {
                RAISE_RUNTIME_ERROR << "BIF: AOI " << aoi.index << " has an invalid grid of "
                    << aoi.cols << "x" << aoi.rows << " tiles";
            }
            if (aoi.tileWidth < 1 || aoi.tileHeight < 1) {
                RAISE_RUNTIME_ERROR << "BIF: AOI " << aoi.index << " has an invalid tile size of "
                    << aoi.tileWidth << "x" << aoi.tileHeight;
            }
        }

        // Overlaps of one axis: a horizontal joint belongs to the column of its tile 1,
        // a vertical joint to the row of its tile 2 (the upper tile).
        std::vector<int> axisOverlaps(const Aoi& aoi, JointAxis axis, std::optional<double> slideMedian)
        {
            const bool horizontal = axis == JointAxis::Horizontal;
            const int boundaries = (horizontal ? aoi.cols : aoi.rows) - 1;
            const int tileExtent = horizontal ? aoi.tileWidth : aoi.tileHeight;

            std::vector<std::vector<double>> perBoundary(boundaries);
            std::vector<double> all;
            for (const Joint& joint : aoi.joints) {
                if (joint.axis != axis || !joint.usable()) {
                    continue;
                }
                const cv::Point pos = horizontal
                    ? snakePosition(joint.tile1, aoi.cols, aoi.rows)
                    : snakePosition(joint.tile2, aoi.cols, aoi.rows);
                const int boundary = horizontal ? pos.x : pos.y;
                if (boundary < 0 || boundary >= boundaries) {
                    RAISE_RUNTIME_ERROR << "BIF: AOI " << aoi.index << " has a joint between tiles "
                        << joint.tile1 << " and " << joint.tile2 << " outside its "
                        << axisName(axis) << " boundaries";
                }
                const double value = overlapAlong(joint, axis);
                perBoundary[boundary].push_back(value);
                all.push_back(value);
            }

            const std::optional<double> aoiMedian = median(all);
            bool loggedAoiFallback = false;
            bool loggedSlideFallback = false;
            bool loggedZeroFallback = false;
            std::vector<int> overlaps(boundaries);
            for (int b = 0; b < boundaries; ++b) {
                std::optional<double> value = median(perBoundary[b]);
                if (!value && aoiMedian) {
                    if (!loggedAoiFallback) {
                        SLIDEIO_LOG(INFO) << "BIF: AOI " << aoi.index << " has " << axisName(axis)
                            << " boundaries without usable joints; using the AOI median overlap "
                            << *aoiMedian;
                        loggedAoiFallback = true;
                    }
                    value = aoiMedian;
                }
                if (!value && slideMedian) {
                    if (!loggedSlideFallback) {
                        SLIDEIO_LOG(INFO) << "BIF: AOI " << aoi.index << " has no usable "
                            << axisName(axis) << " joints; using the slide median overlap "
                            << *slideMedian;
                        loggedSlideFallback = true;
                    }
                    value = slideMedian;
                }
                if (!value) {
                    if (!loggedZeroFallback) {
                        SLIDEIO_LOG(INFO) << "BIF: AOI " << aoi.index << " has no usable "
                            << axisName(axis) << " joints and the slide has none either; "
                            "tiles abut";
                        loggedZeroFallback = true;
                    }
                    value = 0.;
                }
                const long rounded = std::lround(*value);
                const long clamped = std::clamp<long>(rounded, 0, tileExtent - 1);
                if (clamped != rounded) {
                    SLIDEIO_LOG(WARNING) << "BIF: AOI " << aoi.index << " " << axisName(axis)
                        << " boundary " << b << " overlap " << *value << " clamped to " << clamped;
                }
                overlaps[b] = static_cast<int>(clamped);
            }
            return overlaps;
        }

        void placeAxis(const std::vector<int>& overlaps, int tileExtent, const char* name,
                       std::vector<int>& positions, std::vector<int>& cuts, int& size)
        {
            const size_t tiles = overlaps.size() + 1;
            positions.assign(tiles, 0);
            cuts.assign(overlaps.size(), 0);
            for (size_t b = 0; b < overlaps.size(); ++b) {
                const int overlap = overlaps[b];
                if (overlap < 0 || overlap >= tileExtent) {
                    RAISE_RUNTIME_ERROR << "BIF: " << name << " overlap " << overlap
                        << " at boundary " << b << " is outside [0, " << tileExtent - 1 << "]";
                }
                positions[b + 1] = positions[b] + tileExtent - overlap;
                cuts[b] = positions[b + 1] + overlap / 2;
            }
            size = positions.back() + tileExtent;
        }

        std::vector<int> mapAxis(int start, int count, int downsample, int storageExtent,
                                 int origin, int tileExtent,
                                 const std::vector<int>& positions, const std::vector<int>& cuts)
        {
            if (downsample < 1) {
                RAISE_RUNTIME_ERROR << "BIF: invalid downsample " << downsample;
            }
            if (storageExtent < 1) {
                RAISE_RUNTIME_ERROR << "BIF: invalid storage extent " << storageExtent;
            }
            if (start < 0 || count < 0) {
                RAISE_RUNTIME_ERROR << "BIF: invalid output range: start " << start
                    << ", count " << count;
            }
            std::vector<int> map(count);
            for (int i = 0; i < count; ++i) {
                const int64_t level0 = (static_cast<int64_t>(start) + i) * downsample;
                const size_t tile = std::upper_bound(cuts.begin(), cuts.end(), level0) - cuts.begin();
                const int64_t storage0 = origin + static_cast<int64_t>(tile) * tileExtent
                    + (level0 - positions[tile]);
                // storage0 >= 0 whenever the origin is: a tile owns nothing left of its position.
                int64_t storage = storage0 >= 0 ? storage0 / downsample
                                                : -((-storage0 + downsample - 1) / downsample);
                storage = std::clamp<int64_t>(storage, 0, storageExtent - 1);
                map[i] = static_cast<int>(storage);
            }
            return map;
        }
    }

    std::optional<double> slideMedianOverlap(const std::vector<Aoi>& aois, JointAxis axis)
    {
        std::vector<double> values;
        for (const Aoi& aoi : aois) {
            if (!aoi.scanned) {
                continue;
            }
            for (const Joint& joint : aoi.joints) {
                if (joint.axis == axis && joint.usable()) {
                    values.push_back(overlapAlong(joint, axis));
                }
            }
        }
        return median(std::move(values));
    }

    BoundaryOverlaps estimateOverlaps(const Aoi& aoi, std::optional<double> slideMedianX,
                                      std::optional<double> slideMedianY)
    {
        checkGrid(aoi);
        BoundaryOverlaps overlaps;
        overlaps.columns = axisOverlaps(aoi, JointAxis::Horizontal, slideMedianX);
        overlaps.rows = axisOverlaps(aoi, JointAxis::Vertical, slideMedianY);
        return overlaps;
    }

    AoiGeometry::AoiGeometry(const Aoi& aoi, const BoundaryOverlaps& overlaps)
        : m_originX(aoi.originX), m_originY(aoi.originY),
          m_tileWidth(aoi.tileWidth), m_tileHeight(aoi.tileHeight)
    {
        checkGrid(aoi);
        if (overlaps.columns.size() != static_cast<size_t>(aoi.cols - 1)
            || overlaps.rows.size() != static_cast<size_t>(aoi.rows - 1)) {
            RAISE_RUNTIME_ERROR << "BIF: AOI " << aoi.index << " is a grid of " << aoi.cols << "x"
                << aoi.rows << " tiles but has " << overlaps.columns.size() << " column and "
                << overlaps.rows.size() << " row overlaps";
        }
        placeAxis(overlaps.columns, m_tileWidth, "column", m_x, m_cx, m_size.width);
        placeAxis(overlaps.rows, m_tileHeight, "row", m_y, m_cy, m_size.height);
    }

    std::vector<int> AoiGeometry::mapColumns(int start, int count, int downsample, int storageWidth) const
    {
        return mapAxis(start, count, downsample, storageWidth, m_originX, m_tileWidth, m_x, m_cx);
    }

    std::vector<int> AoiGeometry::mapRows(int start, int count, int downsample, int storageHeight) const
    {
        return mapAxis(start, count, downsample, storageHeight, m_originY, m_tileHeight, m_y, m_cy);
    }
}
