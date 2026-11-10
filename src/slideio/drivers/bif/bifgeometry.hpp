// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#pragma once

#include "slideio/drivers/bif/bif_api_def.hpp"
#include "slideio/drivers/bif/bifxml.hpp"
#include <opencv2/core/types.hpp>
#include <optional>
#include <vector>

// Stitching geometry of one AOI, in level-0 pixels unless stated otherwise.
// See software-docs/specs/2026-11-09-bif-driver-design.md section 5.
namespace slideio::bif
{
    // Median of the usable joints' overlap along `axis` (OverlapX for Horizontal,
    // OverlapY for Vertical) across all scanned AOIs; nullopt if there are none.
    SLIDEIO_BIF_EXPORTS std::optional<double> slideMedianOverlap(const std::vector<Aoi>& aois,
                                                                 JointAxis axis);

    // Overlap at each boundary: columns[b] between columns b and b+1 (size cols-1),
    // rows[b] between rows b and b+1 (size rows-1).
    struct SLIDEIO_BIF_EXPORTS BoundaryOverlaps
    {
        std::vector<int> columns;
        std::vector<int> rows;
    };

    // Per-boundary median of the AOI's joints, falling back to the AOI median, then the
    // slide median, then 0; clamped to [0, tile size - 1].
    SLIDEIO_BIF_EXPORTS BoundaryOverlaps estimateOverlaps(const Aoi& aoi,
                                                          std::optional<double> slideMedianX,
                                                          std::optional<double> slideMedianY);

    class SLIDEIO_BIF_EXPORTS AoiGeometry
    {
    public:
        AoiGeometry(const Aoi& aoi, const BoundaryOverlaps& overlaps);
        cv::Size size() const { return m_size; }
        const std::vector<int>& columnPositions() const { return m_x; }
        const std::vector<int>& rowPositions() const { return m_y; }
        const std::vector<int>& columnCuts() const { return m_cx; }
        const std::vector<int>& rowCuts() const { return m_cy; }
        // Storage pixel, at the level downsampled by `downsample`, of each output pixel in
        // [start, start+count) at that level; clamped to [0, storageExtent-1].
        std::vector<int> mapColumns(int start, int count, int downsample, int storageWidth) const;
        std::vector<int> mapRows(int start, int count, int downsample, int storageHeight) const;
    private:
        int m_originX;
        int m_originY;
        int m_tileWidth;
        int m_tileHeight;
        cv::Size m_size;
        std::vector<int> m_x;
        std::vector<int> m_y;
        std::vector<int> m_cx;
        std::vector<int> m_cy;
    };
}
