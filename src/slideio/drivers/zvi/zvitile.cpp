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
    if(m_XIndex<0 || m_YIndex<0)
    {
        m_XIndex = xIndex;
        m_YIndex = yIndex;
        m_Rect.width = item->getWidth();
        m_Rect.height = item->getHeight();
    }
    if(xIndex!=m_XIndex || yIndex!=m_YIndex)
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
