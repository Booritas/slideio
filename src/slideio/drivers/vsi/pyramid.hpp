// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#pragma once
#include "slideio/drivers/vsi/vsi_api_def.hpp"
#include "slideio-opencv/core.hpp"
#include <vector>

#if defined(_MSC_VER)
#pragma warning( push )
#pragma warning(disable: 4251)
#endif

namespace slideio
{
    namespace vsi
    {
        class IDimensionOrder;

        struct TileInfo
        {
            std::vector<int> coordinates;
            int64_t offset = 0;
            uint32_t size = 0;
        };

        class SLIDEIO_VSI_EXPORTS PyramidLevel
        {
            friend class Pyramid;

        public:
            int getScaleLevel() const { return m_scaleLevel; }
            cv::Size getSize() const { return m_size; }
            int getNumTiles() const { return static_cast<int>(m_tileIndices.size()); }
            const TileInfo& getTile(int tileIndex, int channelIndex, int zIndex, int tIndex) const;
        private:
            int m_scaleLevel = 1;
            cv::Size m_size;
            std::vector<TileInfo> m_tiles;
            std::vector<int> m_tileIndices;
            int m_channelDimIndex = -1;
            int m_zDimIndex = -1;
            int m_tDimIndex = -1;
        };

        class SLIDEIO_VSI_EXPORTS Pyramid
        {
        public:
            int getNumLevels() const { return static_cast<int>(m_levels.size()); }
            const PyramidLevel& getLevel(int index) const { return m_levels[index]; }
            void init(std::vector<TileInfo>& tiles, const cv::Size& imageSize, const cv::Size& tileSize,
                      const IDimensionOrder* dimOrder);
            int getNumChannelIndices() const { return m_numChannelIndices; }
            int getNumZIndices() const { return m_numZIndices; }
            int getNumTIndices() const { return m_numTIndices; }
        private:
            std::vector<PyramidLevel> m_levels;
            int m_numChannelIndices = 1;
            int m_numZIndices = 1;
            int m_numTIndices = 1;
        };
    }
}

#if defined(_MSC_VER)
#pragma warning( pop )
#endif
