// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#pragma once
#include "slideio/core/slideio_core_def.hpp"
#include <unordered_map>
#include <fstream>
#include <opencv2/core.hpp>
#include "slideio/core/tools/tilevisitor.hpp"

#if defined(_MSC_VER)
#pragma warning( push )
#pragma warning(disable: 4251)
#endif

namespace slideio {
    class TempFile;
    class SLIDEIO_CORE_EXPORTS CacheManager : public ITileVisitor {
    public:
        CacheManager(const cv::Size& tileSize, const cv::Size& tileCounts);
        virtual ~CacheManager();
        void saveTile(int x, int y, const cv::Mat& tile);
        cv::Mat getTile(int x, int y);
        void visit(int x, int y, const cv::Mat& tile) override;
    private:
        cv::Size m_tileSize;
        cv::Size m_tileCounts;
        std::unordered_map<int64_t, std::pair<int64_t,int>> m_cachePointers;
        std::fstream m_cacheFile;
        int64_t m_lastPointer;
        int m_type;
        std::unique_ptr<TempFile> m_tempFile;
    };
}
