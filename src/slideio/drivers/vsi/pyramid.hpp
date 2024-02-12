// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#pragma once
#include "slideio/drivers/vsi/vsi_api_def.hpp"
#include <vector>

namespace cv
{
   class Size;
};

namespace slideio
{
   namespace vsi 
   {
         class Volume;

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
            int getScaleLevel() const { return scaleLevel; }
            int getWidth() const { return width; }
            int getHeight() const { return height; }
            const std::vector<TileInfo>& getTiles() const { return tiles; }
            const std::vector<int>& getTileIndices() const { return tileIndices; }
         private:
            int scaleLevel = 1;
            int width = 0;
            int height = 0;
            std::vector<TileInfo> tiles;
            std::vector<int> tileIndices;
         };

         class SLIDEIO_VSI_EXPORTS Pyramid
         {
         public:
            int getNumLevels() const { return static_cast<int>(m_levels.size()); }
            const PyramidLevel& getLevel(int index) const { return m_levels[index]; }
            void init(std::vector<TileInfo>& tiles, const cv::Size& imageSize, cv::Size& tileSize, Volume* volume);
         private:
            std::vector<PyramidLevel> m_levels;
         }
   }
}