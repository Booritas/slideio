// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#ifndef OPENCV_slideio_zvitile_HPP
#define OPENCV_slideio_zvitile_HPP
#include <opencv2/core.hpp>

namespace ole {
    class compound_document;
}

namespace slideio
{
    class ZVIImageItem;
    class ZVITile
    {
    public:
        ZVITile() = default;
        ~ZVITile() = default;
        int getIndex() const { return m_Index; }
        void setIndex(int index) { m_Index = index; }
        cv::Rect getRect() const { return m_Rect; }
        void addItem(const ZVIImageItem* item);
        void finalize();
        void setTilePosition(int x, int y);
        bool readTile(const std::vector<int>& componentIndices,
            cv::OutputArray tile_raster, int slice, ole::compound_document& doc) const;
    protected:
        const ZVIImageItem* getImageItem(int slice, int channelIndex) const;
    private:
        int m_Index = 0;
        int m_XIndex = -1;
        int m_YIndex = -1;
        cv::Rect m_Rect = {0, 0, 0, 0};
        std::vector<const ZVIImageItem*> m_ImageItems;
    };
}

#endif
