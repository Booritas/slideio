// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#pragma once
#include "slideio/core/slideio_core_def.hpp"
#include <unordered_map>
#include <boost/container_hash/hash.hpp>
#include <opencv2/core.hpp>

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

            friend bool operator==(const Metadata& lhs, const Metadata& rhs)
            {
                return lhs.zoomX == rhs.zoomX
                    && lhs.zoomY == rhs.zoomY
                    && lhs.rect == rhs.rect;
            }

            friend bool operator!=(const Metadata& lhs, const Metadata& rhs)
            {
                return !(lhs == rhs);
            }
        };
        struct MetadataHash {
            std::size_t operator()(const Metadata& key) const {
                std::size_t seed = 0;
                boost::hash_combine(seed, key.zoomX);
                boost::hash_combine(seed, key.zoomY);
                boost::hash_combine(seed, key.rect.x);
                boost::hash_combine(seed, key.rect.y);
                boost::hash_combine(seed, key.rect.width);
                boost::hash_combine(seed, key.rect.height);
                return seed;
            }

        };
        CacheManager();
        virtual ~CacheManager();
        void addCache(const Metadata& metadata, const cv::Mat& raster);
        cv::Mat getCache(const Metadata& metadata);
    private:
        std::unordered_map<Metadata, cv::Mat, MetadataHash> m_cachePointers;
    };
}
