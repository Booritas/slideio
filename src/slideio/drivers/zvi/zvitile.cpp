// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#include "slideio/base/exceptions.hpp"
#include "slideio/base/log.hpp"
#include "slideio/core/tools/cvtools.hpp"
#include "zvitile.hpp"
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
        RAISE_RUNTIME_ERROR << "ZVIImageDriver: unexpected image item (" 
            << xIndex << "," << yIndex << "). Expected: (" << m_XIndex << "," << m_YIndex << ")";
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
        if (currItem->getZIndex() == slice)
        {
            if (currItem->getCIndex() == channelIndex)
            {
                item = currItem;
            }
        }
    }
    return item;
}

// A packed pixel format (BGR, BGRA, BGR16, BGR32) stores every component of a
// pixel in a single image item, and ZVIScene::alignChannelInfoToPixelFormat()
// expands that one item into 3 or 4 logical channels. Those channels have no
// item carrying their own channel index -- they are extracted from the packed
// raster of the one item that does -- so getImageItem() finds nothing for
// channels 1..n-1 and the components come back blank.
//
// An item of a non-packed scene reports a single channel, so this never
// matches there: a channel that genuinely has no item stays unresolved.
const ZVIImageItem* ZVITile::getPackedImageItem(int slice, const int channelIndex) const
{
    for (const ZVIImageItem* item : m_ImageItems)
    {
        if (item->getZIndex() == slice && channelIndex < item->getChannelCount())
        {
            return item;
        }
    }
    return nullptr;
}

bool ZVITile::readTile(const std::vector<int>& componentIndices,
                       cv::OutputArray tileRaster, int slice, ole::compound_document& doc) const
{
    // Nothing at all was stored for this tile. The composer paints the
    // background over the tile rectangle when a tile reports itself
    // unavailable, which is the only thing left to do here.
    if (m_ImageItems.empty())
    {
        SLIDEIO_LOG(WARNING) << "ZVIImageDriver: tile (" << m_XIndex << "," << m_YIndex
            << ") holds no image item and is not readable.";
        return false;
    }

    bool ok = false;

    std::vector<cv::Mat> channelRasters;

    for (auto index = 0; index < componentIndices.size(); ++index)
    {
        const int channelIndex = componentIndices[index];
        const ZVIImageItem* item = getImageItem(slice, channelIndex);
        if(!item) {
            // No item owns this channel index. In a packed format that is
            // normal: the component lives inside the raster of the item that
            // holds them all, and is extracted from it below.
            item = getPackedImageItem(slice, channelIndex);
        }
        if(!item) {
            // The document carries this channel somewhere -- the scene would
            // not advertise it otherwise -- but not for this (tile, slice).
            // The channels that are present are still readable, so the absent
            // one is filled rather than costing the caller the whole tile.
            SLIDEIO_LOG(WARNING) << "ZVIImageDriver: tile (" << m_XIndex << "," << m_YIndex
                << ") has no image item for channel " << channelIndex << " and slice "
                << slice << ". The channel is filled with zeros.";
            const ZVIImageItem* reference = m_ImageItems.front();
            channelRasters.push_back(cv::Mat::zeros(m_Rect.height, m_Rect.width,
                CV_MAKETYPE(CVTools::toOpencvType(reference->getDataType()), 1)));
            continue;
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
