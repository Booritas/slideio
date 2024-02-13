// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#include "slideio/drivers/vsi/pyramid.hpp"

#include "slideio/base/exceptions.hpp"
#include "slideio/drivers/vsi/dimensions.hpp"

using namespace slideio;
using namespace slideio::vsi;

const TileInfo& PyramidLevel::getTile(int tileIndex, int channelIndex, int zIndex, int tIndex) const {
    const int numTiles = static_cast<int>(m_tileIndices.size());
    if (tileIndex < 0 || tileIndex >= numTiles) {
        RAISE_RUNTIME_ERROR << "Tile index " << tileIndex << "is out of range";
    }
    const int tileStartIndex = m_tileIndices[tileIndex];
    const int tileEndIndex = (tileIndex < numTiles - 1)
                                 ? m_tileIndices[tileIndex + 1]
                                 : static_cast<int>(m_tiles.size());

    for (int index = tileStartIndex; index < tileEndIndex; ++index) {
        const auto& tile = m_tiles[index];
        if (m_channelDimIndex>0 && tile.coordinates[m_channelDimIndex] != channelIndex) {
            continue;
        }
        if (m_zDimIndex>0 && tile.coordinates[m_zDimIndex] != zIndex) {
            continue;
        }
        if (m_tDimIndex>0 && tile.coordinates[m_tDimIndex] != tIndex) {
            continue;
        }
        return tile;
    }
    RAISE_RUNTIME_ERROR << "Tile not found: index: " << tileIndex
        << " channel: " << channelIndex << " z: " << zIndex << " t: " << tIndex;
}

void Pyramid::init(std::vector<TileInfo>& tiles, const cv::Size& imageSize, const cv::Size& tileSize,
                   const IDimensionOrder* dimOrder) {
    if (tiles.empty()) {
        return;
    }

    const int numDimensions = static_cast<int>(tiles.front().coordinates.size());
    int numPyramidLevels = 0;
    m_numChannelIndices = 0;
    m_numZIndices = 0;
    m_numTIndices = 0;
    const int channelIndex = dimOrder->getDimensionOrder(Dimensions::C);
    const int zIndex = dimOrder->getDimensionOrder(Dimensions::Z);
    const int tIndex = dimOrder->getDimensionOrder(Dimensions::T);
    for (auto& tile : tiles) {
        numPyramidLevels = std::max(numPyramidLevels, tile.coordinates.back());
        if (channelIndex > 0) {
            m_numChannelIndices = std::max(m_numChannelIndices, tile.coordinates[channelIndex]);
        }
        if (zIndex > 0) {
            m_numZIndices = std::max(m_numZIndices, tile.coordinates[zIndex]);
        }
        if(tIndex > 0) {
            m_numTIndices = std::max(m_numTIndices, tile.coordinates[tIndex]);
        }
    }
    numPyramidLevels++;
    m_numChannelIndices++;
    m_numTIndices++;
    m_numZIndices++;

    m_levels.resize(numPyramidLevels);

    for (int level = 0; level < numPyramidLevels; ++level) {
        PyramidLevel& pyramidLevel = m_levels[level];
        const int width = imageSize.width >> level;
        const int height = imageSize.height >> level;
        const int numTilesX = (width - 1) / tileSize.width + 1;
        const int numTilesY = (height - 1) / tileSize.height + 1;
        const int numTiles = numTilesX * numTilesY;
        pyramidLevel.m_tiles.reserve(numTiles);
        pyramidLevel.m_scaleLevel = 1 << level;
        pyramidLevel.m_size.width = width;
        pyramidLevel.m_size.height = height;
        pyramidLevel.m_channelDimIndex = channelIndex;
        pyramidLevel.m_zDimIndex = dimOrder->getDimensionOrder(Dimensions::Z);
        pyramidLevel.m_tDimIndex = dimOrder->getDimensionOrder(Dimensions::T);
    }

    if (numPyramidLevels == 1) {
        PyramidLevel& pyramidLevel = m_levels[0];
        pyramidLevel.m_tiles.assign(tiles.begin(), tiles.end());
    }
    else {
        for (const auto& tileInfo : tiles) {
            const int level = tileInfo.coordinates.back();
            PyramidLevel& pyramidLevel = m_levels[level];
            pyramidLevel.m_tiles.push_back(tileInfo);
        }
    }
    std::vector<int> sortOrder = {1, 0};

    if (channelIndex > 0) {
        sortOrder.push_back(channelIndex);
    }
    if (zIndex > 0) {
        sortOrder.push_back(zIndex);
    }
    if (tIndex > 0) {
        sortOrder.push_back(tIndex);
    }
    std::vector<int> unknownDims;
    for (int dim = 2; dim < numDimensions; ++dim) {
        if (std::find(sortOrder.begin(), sortOrder.end(), dim) == sortOrder.end()) {
            unknownDims.push_back(dim);
        }
    }
    const int knownDims = static_cast<int>(sortOrder.size());
    sortOrder.insert(sortOrder.end(), unknownDims.begin(), unknownDims.end());

    for (auto& pyramidLevel : m_levels) {
        std::sort(pyramidLevel.m_tiles.begin(), pyramidLevel.m_tiles.end(),
                  [&sortOrder](const TileInfo& left, const TileInfo& right) {
                      for (int dim : sortOrder) {
                          const int leftVal = left.coordinates[dim];
                          const int rightVal = right.coordinates[dim];
                          if (leftVal < rightVal) {
                              return true;
                          }
                          if (rightVal < leftVal) {
                              return false;
                          }
                      }
                      return false;
                  });
    }
    const int numCoreDimensions = 2;
    std::vector<int> tileValues;
    tileValues.reserve(numCoreDimensions);
    for (auto& pyramidLevel : m_levels) {
        std::vector<int>& tileIndices = pyramidLevel.m_tileIndices;
        const auto& tiles = pyramidLevel.m_tiles;
        tileValues.clear();
        for (size_t index = 0; index < tiles.size(); ++index) {
            const auto& coordinates = tiles[index].coordinates;
            bool newTile = false;
            if (tileValues.empty()) {
                newTile = true;
                for (int dim = 0; dim < numCoreDimensions; ++dim)
                    tileValues.push_back(coordinates[dim]);
            }
            else {
                for (int dim = 0; dim < numCoreDimensions; ++dim) {
                    if (coordinates[dim] != tileValues[dim] || newTile) {
                        newTile = true;
                        tileValues[dim] = coordinates[dim];
                    }
                }
            }
            if (newTile) {
                tileIndices.push_back(static_cast<int>(index));
            }
        }
    }
}
