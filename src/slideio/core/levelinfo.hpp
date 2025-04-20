// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#pragma once
#include "slideio/core/slideio_core_def.hpp"
#include "slideio/base/slideio_structs.hpp"
#include <cmath>

#if defined(_MSC_VER)
#pragma warning( push )
#pragma warning(disable: 4251)
#endif

namespace slideio
{
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

        int getLevel() const { return m_level; }
        void setLevel(int level) { m_level = level; }

        Size getSize() const { return m_size; }
        void setSize(const Size& size) { m_size = size; }

        double getScale() const { return m_scale; }
        void setScale(double scale) { m_scale = scale; }

        double getMagnification() const { return m_magnification; }
        void setMagnification(double magnification) { m_magnification = magnification; }

        Size getTileSize() const { return m_tileSize; }
        void setTileSize(const Size& tileSize) { m_tileSize = tileSize; }

		int getTileCount() const {
            if (m_tileCount < 1)
                updateTileCount();
            return m_tileCount;
        }

        std::string toString() const;

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
