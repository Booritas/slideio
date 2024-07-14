// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.

#include "slideio/core/tools/recttiler.hpp"
#include "slideio/base/exceptions.hpp"
#include "slideio/core/tools/recttilevisitor.hpp"

using namespace slideio;

RectTiler::RectTiler(const cv::Size& rect, const cv::Size& tileSize) :
    m_tileSize(tileSize),
    m_rectSize(rect)
{
    if (tileSize.width <= 0 || tileSize.height <= 0) {
        RAISE_RUNTIME_ERROR << "RectTiler: Invalid tile size: (" << tileSize.width << ", " << tileSize.height << ")";
    }

    if(rect.width <= 0 || rect.height <= 0) {
        RAISE_RUNTIME_ERROR << "RectTiler: Invalid rectangle size: (" << rect.width << ", " << rect.height << ")";
    }
}


void RectTiler::apply(IRectTileVisitor* visitor) const
{
    if(visitor ==nullptr) {
        RAISE_RUNTIME_ERROR << "BlockTiler: visitor is null";
    }
    apply([&](const cv::Rect& rect){
        visitor->visit(rect);
    });
}

void slideio::RectTiler::apply(std::function<void(const cv::Rect&)> visitor) const
{
    if(!visitor) {
        RAISE_RUNTIME_ERROR << "RectTiler: visitor lambda is null";
    }
    const int tileCountX = (m_rectSize.width - 1) / m_tileSize.width + 1;
    const int tileCountY = (m_rectSize.height - 1) / m_tileSize.height + 1;
    for (int tileY = 0; tileY < tileCountY; ++tileY) {
               const int y = tileY * m_tileSize.height;
        int height = m_tileSize.height;
        if ((y + m_tileSize.height) > m_rectSize.height) {
                       height = m_rectSize.height - y;
        }
        for (int tileX = 0; tileX < tileCountX; ++tileX) {
            int width = m_tileSize.width;
            const int x = tileX * m_tileSize.width;
            if ((x + m_tileSize.width) > m_rectSize.width) {
                width = m_rectSize.width - x;
            }
            const cv::Rect block(x, y, width, height);
            visitor(block);
        }
    }
}