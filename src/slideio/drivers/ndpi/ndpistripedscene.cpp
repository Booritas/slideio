// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#include "slideio/drivers/ndpi//ndpistripedscene.hpp"

#include "ndpifile.hpp"

using namespace slideio;


int NDPIStripedScene::getTileCount(void* userData)
{
    // const TiffDirectory* dir = (const TiffDirectory*)userData;
    // int tilesX = (dir->width-1)/dir->tileWidth + 1;
    // int tilesY = (dir->height-1)/dir->tileHeight + 1;
    // return tilesX * tilesY;
    return 0;
}

bool NDPIStripedScene::getTileRect(int tileIndex, cv::Rect& tileRect, void* userData)
{
    // const TiffDirectory* dir = (const TiffDirectory*)userData;
    // const int tilesX = (dir->width - 1) / dir->tileWidth + 1;
    // const int tilesY = (dir->height - 1) / dir->tileHeight + 1;
    // const int tileY = tileIndex / tilesX;
    // const int tileX = tileIndex % tilesX;
    // tileRect.x = tileX * dir->tileWidth;
    // tileRect.y = tileY * dir->tileHeight;
    // tileRect.width = dir->tileWidth;
    // tileRect.height = dir->tileHeight;
    // return true;
    return false;
}

bool NDPIStripedScene::readTile(int tileIndex, const std::vector<int>& channelIndices, cv::OutputArray tileRaster,
    void* userData)
{
    //const TiffDirectory* dir = (const TiffDirectory*)userData;
    bool ret = false;
    // try
    // {
    //     TiffTools::readTile(getFileHandle(), *dir, tileIndex, channelIndices, tileRaster);
    //     ret = true;
    // }
    // catch(std::runtime_error&){
    // }

    return ret;
}
