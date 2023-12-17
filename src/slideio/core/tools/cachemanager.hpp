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
        class Metadata {
        public:
            Metadata() : zoomX(0.0), zoomY(0.0) {}
            Metadata(double zX, double zY) :
                zoomX(zX), zoomY(zY)
            {
            }
            friend bool operator==(const Metadata& lhs, const Metadata& rhs)
            {
                return std::lround(lhs.zoomX*zoomRound) == std::lround(rhs.zoomX*zoomRound)
                    && std::lround(lhs.zoomY*zoomRound) == std::lround(rhs.zoomY*zoomRound)
                    && lhs.rect == rhs.rect;
            }
            friend bool operator!=(const Metadata& lhs, const Metadata& rhs)
            {
                return !(lhs == rhs);
            }
            std::size_t hash() const
            {
                std::size_t seed = 0;
                const long zoomX = std::lround(this->zoomX * zoomRound);
                const long zoomY = std::lround(this->zoomY * zoomRound);
                boost::hash_combine(seed, zoomX);
                boost::hash_combine(seed, zoomY);
                boost::hash_combine(seed, rect.x);
                boost::hash_combine(seed, rect.y);
                boost::hash_combine(seed, rect.width);
                boost::hash_combine(seed, rect.height);
                return seed;
            }
        public:
            double zoomX;
            double zoomY;
            cv::Rect rect;
        private:
            static const double zoomRound;
        };
        struct MetadataHash {
            std::size_t operator()(const Metadata& key) const {
                return key.hash();
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
