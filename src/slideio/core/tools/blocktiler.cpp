// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.

#include "slideio/core/tools/blocktiler.hpp"
#include "slideio/base/exceptions.hpp"
#include "slideio/core/tools/tilevisitor.hpp"

using namespace slideio;

BlockTiler::BlockTiler(const cv::Mat& block, const cv::Size& tileSize) :
    m_tileSize(tileSize),
    m_block(block)
{
    if (tileSize.width <= 0 || tileSize.height <= 0) {
        RAISE_RUNTIME_ERROR << "BlockTiler: Invalid tile size: (" << tileSize.width << ", " << tileSize.height << ")";
    }

    if(block.cols <= 0 || block.rows <= 0) {
        RAISE_RUNTIME_ERROR << "BlockTiler: Invalid block size: (" << block.cols << ", " << block.rows << ")";
    }
}


void BlockTiler::apply(ITileVisitor* visitor) const
{
    apply([&](int tileX, int tileY, const cv::Mat& tile){
        visitor->visit(tileX, tileY, tile);
    });
}

void slideio::BlockTiler::apply(std::function<void(int, int, const cv::Mat&)> visitor) const
{
    if(!visitor) {
        RAISE_RUNTIME_ERROR << "BlockTiler: visitor lambda is null";
    }
    const int tileCountX = (m_block.cols - 1) / m_tileSize.width + 1;
    const int tileCountY = (m_block.rows - 1) / m_tileSize.height + 1;
    for (int tileY = 0; tileY < tileCountY; ++tileY) {
               const int y = tileY * m_tileSize.height;
        int height = m_tileSize.height;
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
            visitor(tileX, tileY, tile);
        }
    }
}