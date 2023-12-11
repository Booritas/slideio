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
    class SLIDEIO_CORE_EXPORTS CacheManager {
    public:
        struct Metadata {
            double zoomX;
            double zoomY;
            cv::Rect rect;
        }
        CacheManager();
        virtual ~CacheManager();
        void addCache(const Metadata& metadata, const cv::Map& raster);
        cv::Mat getCache(const Metadata& metadata);
    private:
        std::unordered_map<Metadata, std::pair<int64_t,int>> m_cachePointers;
    };
}
