// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#include "slideio/drivers/vsi/pyramid.hpp";

using namespace slideio;
using namespace slideio::vsi;

void Pyramid::init(std::vector<TileInfo>& tiles, const cv::Size& imageSize, cv::Size& tileSize, Volume* volume) {
   int numPyramidLevels = 0;
   for(auto& tile: tiles) {
      numPyramidLevels = std::max(numPyramidLevels, tile.coordinates.back());
   }
   numPyramidLevels++;

   m_levels.resize(numPyramidLevels);

   for (int level = 0; level < numPyramidLevels; ++level) {
      PyramidLevel &pyramidLevel = m_levels[level];
      const int width = imageSize.width >> level;
      const int height = imageSize.height >> level;
      const int numTilesX = (width - 1) / tileSize.width + 1;
      const int numTilesY = (height - 1) / tileSize.height + 1;
      const int numTiles = numTilesX * numTilesY;
      pyramidLevel.tiles.reserve(numTiles);
      pyramidLevel.scaleLevel = 1 << level;
      pyramidLevel.width = width;
      pyramidLevel.height = height;
   }

   if (numPyramidLevels == 1) {
      PyramidLevel &pyramidLevel = m_levels[0];
      pyramidLevel.tiles.assign(tiles.begin(), tiles.end());
   }
   else {
      for (const auto &tileInfo : tiles) {
         const int level = tileInfo.coordinates.back();
         PyramidLevel &pyramidLevel = m_levels[level];
         pyramidLevel.tiles.push_back(tileInfo);
      }
   }
   std::vector<int> sortOrder = {0, 1};

   const int channelIndex = volume->getDimensionOrder(Dimensions::C);
   if(channelIndex>0)
    sortOrder.push_back(channelIndex);
    const int zIndex = volume->getDimensionOrder(Dimensions::Z);
    if(zIndex>0)
        sortOrder.push_back(zIndex);
    const int tIndex = volume->getDimensionOrder(Dimensions::T);
    if(tIndex>0)
        sortOrder.push_back(tIndex);
    std::vector<int> unknownDims;
    for(int dim=2; dim<m_numDimensions; ++dim) {
        if(std::find(sortOrder.begin(), sortOrder.end(), dim) == sortOrder.end()) {
            unknownDims.push_back(dim);
        }
    }
    const int knownDims = static_cast<int>(sortOrder.size());
    sortOrder.insert(sortOrder.end(), unknownDims.begin(), unknownDims.end());

    for(auto& pyramidLevel: m_levels) {
       std::sort(pyramidLevel.tiles.begin(), pyramidLevel.tiles.end(),
                 [&sortOrder](const TileInfo& left, const TileInfo& right) {
           for(int dim: sortOrder) {
               const int leftVal = left.coordinates[dim];
               const int rightVal = right.coordinates[dim];
               if(leftVal < rightVal) {
                   return true;
               } else if (rightVal < leftVal) {
                   return false;
               }
           }
           return false;
       });
   }
    const int numCoreDimensions = (channelIndex > 0) ? 3 : 2;
    std::vector<int> tileValues.reserve(numCoreDimensions);
    for(auto& pyramidLevel: m_levels) {
        std::vector<int>& tileIndices = pyramidLevel.tileIndices;
        const auto& tiles = pyramidLevel.tiles;
        tileValues.clear();
        for(size_t index=0; index < tiles.size(); ++index) {
            const auto& coordinates = tiles[index].coordinates;
            bool newTile = false;
            if(tileValues.empty()) {
                newTile = true;
                for(int dim=0; dim < numCoreDimensions; ++dim)
                    tileValues.push_back(coordinates[dim]);
            } else {
                for(int dim=0; dim < numCoreDimensions; ++dim) {
                    if(coordinates[dim] != tileValues[dim] || newTile) {
                        newTile = true;
                        tileValues[dim] = coordinates[dim];
                    }
                }
            }
            if(newTile) {
                tileIndices.push_back(index);
            }
        }
    }
}