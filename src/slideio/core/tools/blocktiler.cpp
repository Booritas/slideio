// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.

#include "slideio/core/tools/blocktiler.hpp"
#include "slideio/core/tools/cachemanager.hpp"

using namespace slideio;

BlockTiler::BlockTiler(const cv::Mat& block, const cv::Size& tileSize) :
    m_tileSize(tileSize),
    m_block(block)
{
}


void BlockTiler::apply(ITileVisitor* visitor) const
{
    const int tileCountX = m_block.cols / m_tileSize.width;
    const int tileCountY = m_block.rows / m_tileSize.height;
    for (int tileY = 0; tileY < tileCountY; ++tileY) {
        const int y = tileY * m_tileSize.height;
        int height = m_tileSize.width;
        if ((y + m_tileSize.height) > m_block.rows) {
            height = m_block.rows - y;
        }
        for (int tileX = 0; tileX < tileCountX; ++tileX) {
            int width = m_tileSize.width;
            const int x = tileX * m_tileSize.width;
            if((x + m_tileSize.width) > m_block.cols) {
                width = m_block.cols - x;
            }
            const cv::Mat tile = m_block(cv::Rect(x, y, width, height));
            visitor->visit(tileX, tileY, tile);
        }
    }
}
