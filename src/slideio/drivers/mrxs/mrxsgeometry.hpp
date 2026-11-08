// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#pragma once

#include "slideio/drivers/mrxs/mrxs_api_def.hpp"
#include "slideio/drivers/mrxs/mrxsindex.hpp"
#include <opencv2/core.hpp>
#include <cstdint>
#include <optional>
#include <vector>

namespace slideio::mrxs
{
    /**@brief a camera photo's origin in original-lattice pixels (format description 8.3).*/
    struct CameraPosition
    {
        int64_t x = 0;
        int64_t y = 0;
    };

    /**@brief decodes a raw or zlib-wrapped position map of px * py nine-byte records and
     * scales it by C0. The inflated size must be exactly 9 * px * py, and inflation stops
     * there rather than growing with the stream. Flags must be 0 or 1.*/
    SLIDEIO_MRXS_EXPORTS std::vector<CameraPosition> decodePositionMap(const std::vector<uint8_t>& payload,
        PositionSource source, int px, int py, int64_t c0);

    /**@brief reads a payload's bytes from its data file.*/
    SLIDEIO_MRXS_EXPORTS std::vector<uint8_t> readPayload(const SlideStructure& slide, const PayloadRef& ref);

    /**@brief one placed subtile of an encoded image (format description 10.4).
     *
     * Its source rectangle is (xi * tileWidth, yi * tileHeight, tileWidth, tileHeight)
     * in the decoded image and its destination starts at (x, y) in the level's pixels,
     * with the level's tile size. Kept to 32 bytes: the largest sample has millions.*/
    struct Drawable
    {
        double x = 0;
        double y = 0;
        uint32_t record = 0;        ///< index into SlideStructure::levels[level]
        uint16_t xi = 0;
        uint16_t yi = 0;
        int32_t gridRow = 0;
        int32_t gridColumn = 0;
    };

    /**@brief the drawables of one stored level and a spatial index over them.*/
    class SLIDEIO_MRXS_EXPORTS LevelGeometry
    {
    public:
        int64_t concat = 1;             ///< C_l
        int64_t downsample = 1;         ///< d_l = C_l / C_0
        int64_t width = 0;              ///< nominal level size
        int64_t height = 0;
        int tilesPerImage = 1;          ///< T_l, per axis
        int positionsPerTile = 1;       ///< Q_l, per axis
        int64_t gridDivisor = 1;
        double tileWidth = 0;
        double tileHeight = 0;

        const std::vector<Drawable>& drawables() const { return m_drawables; }
        /// Whether a drawable can change any pixel of [x0, x1) x [y0, y1) in level pixels:
        /// its ceil-sized intermediate at its fractional origin, plus one pixel of bilinear support.
        bool contributes(const Drawable& d, double x0, double y0, double x1, double y1) const;
        /// The drawables that contribute to the region, in painting order: descending
        /// grid row, then descending grid column (format description 11.3).
        std::vector<uint32_t> query(double x0, double y0, double x1, double y1) const;
        /// Bytes held by the drawables and the spatial index.
        size_t memoryBytes() const;

    private:
        friend class Geometry;
        void buildIndex();

        std::vector<Drawable> m_drawables;
        double m_binOriginX = 0;
        double m_binOriginY = 0;
        double m_binSize = 1;
        int m_binColumns = 0;
        int m_binRows = 0;
        std::vector<uint32_t> m_binStart;   ///< m_binColumns * m_binRows + 1 offsets into m_binItems
        std::vector<uint32_t> m_binItems;
    };

    /**@brief the slide's geometry at every stored level (design section 5).*/
    class SLIDEIO_MRXS_EXPORTS Geometry
    {
    public:
        /// Reads the position map (if any) and builds every level.
        static Geometry build(const SlideStructure& slide);
        /// Builds from given positions; positionMap says whether they came from a map.
        static Geometry build(const SlideStructure& slide, const std::vector<CameraPosition>& positions,
                              bool positionMap);

        bool positionAware = false;
        int64_t latticeWidth = 0;       ///< nominal original-lattice size (format description 9.2)
        int64_t latticeHeight = 0;
        int cameraColumns = 0;          ///< Px
        int cameraRows = 0;             ///< Py
        /// Union of the level-0 drawables in level-0 pixels, not clipped to the canvas.
        std::optional<cv::Rect2d> bounds0;
        std::vector<LevelGeometry> levels;
    };
}
