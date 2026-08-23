// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#pragma once
#include "slideio/core/slideio_core_def.hpp"
#include "slideio/core/slideio_structs.hpp"
#include <cmath>

#if defined(_MSC_VER)
#pragma warning( push )
#pragma warning(disable: 4251)
#endif

namespace slideio
{
    /**@brief Description of a single level of the internal image pyramid of a scene.
     *
     * A scene keeps one object per zoom level and returns it from Scene::getLevelInfo.
     * The object describes the geometry of the level: its size in the pixels of the level,
     * the scale relative to level 0, the objective magnification and the size of the tiles
     * the level is stored in. It also exposes the tile grid of the level through
     * #getTileCount and #getTileRect, which return rectangles in the coordinate system of
     * the level and can be passed straight to Scene::readResampledLevelBlockChannels.
     */
    class SLIDEIO_CORE_EXPORTS LevelInfo
    {
    public:
        friend std::ostream& operator<<(std::ostream& os, const slideio::LevelInfo& levelInfo)
        {
            os << "Level: " << levelInfo.getLevel() << std::endl;
            os << "Size: " << levelInfo.getSize().width << "x" << levelInfo.getSize().height << std::endl;
            os << "Scale: " << levelInfo.getScale() << std::endl;
            os << "Magnification: " << levelInfo.getMagnification() << std::endl;
            os << "Tile Size: " << levelInfo.getTileSize().width << "x" << levelInfo.getTileSize().height << std::endl;
            return os;
        }
    public:
        LevelInfo() = default;

        LevelInfo(int level, const Size& size, double scale, double magnification, const Size& tileSize)
            : m_level(level), m_size(size), m_scale(scale), m_magnification(magnification), m_tileSize(tileSize) {}

        LevelInfo(const LevelInfo& other) {
            m_level = other.m_level;
            m_size = other.m_size;
            m_scale = other.m_scale;
            m_magnification = other.m_magnification;
            m_tileSize = other.m_tileSize;
        }

        LevelInfo& operator=(const LevelInfo& other) {
            if (this != &other) {
                m_level = other.m_level;
                m_size = other.m_size;
                m_scale = other.m_scale;
                m_magnification = other.m_magnification;
                m_tileSize = other.m_tileSize;
            }
            return *this;
        }

        bool operator==(const LevelInfo& other) const {
            return m_level == other.m_level &&
                m_size.width == other.m_size.width &&
                m_size.height == other.m_size.height &&
                std::fabs(m_scale - other.m_scale) < 1.e-2 &&
                std::fabs(m_magnification - other.m_magnification) < 1.e-2 &&
                m_tileSize.width == other.m_tileSize.width &&
                m_tileSize.height == other.m_tileSize.height;
        }

        /**@brief recomputes the cached tile count from the current level and tile size.*/
        void updateTileCount() const {
            if (getTileSize().width > 0 && getTileSize().height > 0) {
                const int tilesX = (getSize().width - 1) / getTileSize().width + 1;
                const int tilesY = (getSize().height - 1) / getTileSize().height + 1;
                m_tileCount = tilesX * tilesY;
			}
            else {
                m_tileCount = 1;
            }
        }

        /**@brief returns the index of the level. Level 0 is the level of the highest resolution.*/
        int getLevel() const { return m_level; }
        void setLevel(int level) { m_level = level; }

        /**@brief returns the size of the level in the pixels of the level.*/
        Size getSize() const { return m_size; }
        void setSize(const Size& size) { m_size = size; }

        /**@brief returns the scale of the level relative to level 0.
         *
         * The scale of level 0 is 1; a level of half the size of level 0 has a scale of 0.5.
         */
        double getScale() const { return m_scale; }
        void setScale(double scale) { m_scale = scale; }

        /**@brief returns the objective magnification of the level. 0 if the format does not report it.*/
        double getMagnification() const { return m_magnification; }
        void setMagnification(double magnification) { m_magnification = magnification; }

        /**@brief returns the size of a tile of the level. (0,0) if the level is not tiled.*/
        Size getTileSize() const { return m_tileSize; }
        void setTileSize(const Size& tileSize) { m_tileSize = tileSize; }

		/**@brief returns the number of tiles of the level.
		 *
		 * A level that is not tiled consists of a single tile that covers the whole level.
		 */
		int getTileCount() const {
            if (m_tileCount < 1)
                updateTileCount();
            return m_tileCount;
        }

        /**@brief returns a human readable description of the level.*/
        std::string toString() const;

		/**@brief returns the rectangle of a tile in the coordinate system of the level.
		 *
		 * Tiles are numbered row by row, from the top left corner of the level.
		 *
		 * @param tileIndex : tile index, in the range [0, getTileCount()).
		 * @return rectangle of the tile. On a level of more than one tile every rectangle has
		 * the size returned by #getTileSize, so the tiles of the right and the bottom edge
		 * overhang the level; reading such a rectangle with
		 * Scene::readResampledLevelBlockChannels fills the part outside the level with the
		 * background value. On a level of a single tile the rectangle is the level itself.
		 */
		Rect getTileRect(int tileIndex) const {
			Rect tileRect;
			const int tileCount = getTileCount();
			if (tileCount > 1) {
				const int tilesX = (m_size.width - 1) / m_tileSize.width + 1;
				const int tilesY = (m_size.height - 1) / m_tileSize.height + 1;
				const int tileY = tileIndex / tilesX;
				const int tileX = tileIndex % tilesX;
				tileRect.x = tileX * m_tileSize.width;
				tileRect.y = tileY * m_tileSize.height;
				tileRect.width = m_tileSize.width;
				tileRect.height = m_tileSize.height;
			}
			else {
				tileRect.x = 0;
				tileRect.y = 0;
				tileRect.width = m_size.width;
				tileRect.height = m_size.height;
			}
			return tileRect;
		}

    private:
        int m_level = 0;
        Size m_size;
        double m_scale = 0.0;
        double m_magnification = 0.0;
        Size m_tileSize;
        mutable int m_tileCount = 0;
    };
}

#if defined(_MSC_VER)
#pragma warning( pop )
#endif
