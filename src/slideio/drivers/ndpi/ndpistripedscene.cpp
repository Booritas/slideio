// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#include "slideio/drivers/ndpi//ndpistripedscene.hpp"

#include "ndpifile.hpp"

using namespace slideio;


int NDPIStripedScene::getTileCount(void* userData)
{
    const NDPITiffDirectory* dir = (const NDPITiffDirectory*)userData;
    const int stripes = (dir->height-1)/dir->rowsPerStrip + 1;
    return stripes;
}

bool NDPIStripedScene::getTileRect(int tileIndex, cv::Rect& tileRect, void* userData)
{
    const NDPITiffDirectory* dir = (const NDPITiffDirectory*)userData;
    
    const int y = tileIndex * dir->rowsPerStrip;
    tileRect.x = 0;
    tileRect.y = y;
    tileRect.width = dir->width;
    tileRect.height = NDPITiffTools::computeStripHeight(*dir, tileIndex);
    return true;
}

bool NDPIStripedScene::readTile(int tileIndex, const std::vector<int>& channelIndices, cv::OutputArray tileRaster,
    void* userData)
{
    const NDPITiffDirectory* dir = (const NDPITiffDirectory*)userData;
    bool ret = false;
    try
    {
        NDPITiffTools::readStrip(m_pfile->getTiffHandle(), *dir, tileIndex, channelIndices, tileRaster);
        ret = true;
    }
    catch(std::runtime_error&){
    }

    return ret;
}
