// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#include "zvitile.hpp"
#include <boost/format.hpp>
#include "zviimageitem.hpp"

using namespace slideio;

void ZVITile::addItem(const slideio::ZVIImageItem* item)
{
    const int xIndex = item->getTileIndexX();
    const int yIndex = item->getTileIndexY();
    if (m_XIndex < 0 || m_YIndex < 0)
    {
        m_XIndex = xIndex;
        m_YIndex = yIndex;
        m_Rect.width = item->getWidth();
        m_Rect.height = item->getHeight();
    }
    if (xIndex != m_XIndex || yIndex != m_YIndex)
    {
        throw std::runtime_error(
            (boost::format("ZVIImageDriver: unexpected image item (%1%,%2%). Expected: (%3%, %4%).")
                % xIndex % yIndex % m_XIndex % m_YIndex).str()
        );
    }
    m_ImageItems.push_back(item);
}

void ZVITile::finalize()
{
    std::sort(m_ImageItems.begin(), m_ImageItems.end(),
              [](const ZVIImageItem* left, const ZVIImageItem* right)
              {
                  bool less = left->getZIndex() < right->getZIndex();
                  if (!less && left->getZIndex() == right->getZIndex())
                  {
                      less = left->getCIndex() < right->getCIndex();
                  }
                  return less;
              });
}

void ZVITile::setTilePosition(int x, int y)
{
    m_Rect.x = x;
    m_Rect.y = y;
}

const ZVIImageItem* ZVITile::getImageItem(int slice, const int channelIndex) const
{
    const ZVIImageItem* item = nullptr;
    for (auto index = 0; item == nullptr && index < m_ImageItems.size(); ++index)
    {
        const ZVIImageItem* currItem = m_ImageItems[index];
        if (currItem->getSceneIndex() == slice)
        {
            if (currItem->getCIndex() == channelIndex)
            {
                item = currItem;
            }
        }
    }
    return item;
}

bool ZVITile::readTile(const std::vector<int>& componentIndices,
                       cv::OutputArray tileRaster, int slice, ole::compound_document& doc) const
{
    bool ok = false;

    std::vector<cv::Mat> channelRasters;

    for (auto index = 0; index < componentIndices.size(); ++index)
    {
        const int channelIndex = componentIndices[index];
        const ZVIImageItem* item = getImageItem(slice, channelIndex);
        if(!item) {
            throw std::runtime_error(
                (boost::format("ZVIImageDriver: Cannot find image item for channel %1% and slice %2%")
                    % channelIndex % slice).str());
        }
        cv::Mat itemRaster;
        item->readRaster(doc, itemRaster);
        if (itemRaster.channels() == 1)
        {
            channelRasters.push_back(itemRaster);
        }
        else
        {
            cv::Mat channelRaster;
            cv::extractChannel(itemRaster, channelRaster, channelIndex);
            channelRasters.push_back(channelRaster);
        }
    }

    ok = true;
    if (channelRasters.size()==1) {
        channelRasters[0].copyTo(tileRaster);
    }
    else {
        cv::merge(channelRasters, tileRaster);
    }
    return ok;
}
