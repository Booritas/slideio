// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#pragma once
#include <map>

#include "slideio/core/slideio_core_def.hpp"
#include <unordered_map>
#include <boost/container_hash/hash.hpp>
#include "slideio-opencv/core.hpp"

#include "tilecomposer.hpp"
#include "tools.hpp"

#if defined(_MSC_VER)
#pragma warning( push )
#pragma warning(disable: 4251)
#endif

namespace slideio
{
    class SLIDEIO_CORE_EXPORTS CacheManager
    {
    public:
        class Level
        {
        public:
            Level(int id) : m_id(id)
            {
            }

            int getId() const { return m_id; }

            void addTile(const cv::Rect& rect, int cacheIndex)
            {
                tiles.emplace_back(rect, cacheIndex);
            }

            int getTileCount() const
            {
                return static_cast<int>(tiles.size());
            }

            const cv::Rect& getTileRect(int index) const
            {
                return tiles[index].first;
            }

            int getTileCacheIndex(int index) const
            {
                return tiles[index].second;
            }

        private:
            int m_id;
            std::vector<std::pair<cv::Rect, int>> tiles;
        };

        CacheManager();
        virtual ~CacheManager();
        void addTile(int level, cv::Point2i pos, const cv::Mat& tile);
        int getTileCount(int level) const;
        cv::Mat getTile(int levelId, int index) const;
        const cv::Rect getTileRect(int levelId, int tileIndex) const;

    private:
        std::vector<cv::Mat> m_cache;
        std::map<int, std::shared_ptr<Level>> m_levels;
    };

    class SLIDEIO_CORE_EXPORTS CacheManagerTiler : public Tiler
    {
    public:
        CacheManagerTiler(std::shared_ptr<CacheManager>& cacheManager, const cv::Size& tileSize, int levelId) :
            m_cacheManager(cacheManager), m_tileSize(tileSize), m_levelId(levelId) {
        }
        int getTileCount(void* userData) override {
            return m_cacheManager->getTileCount(m_levelId);
        }
        bool getTileRect(int tileIndex, cv::Rect& tileRect, void* userData) override {
            tileRect = m_cacheManager->getTileRect(m_levelId, tileIndex);
            return true;
        }
        bool readTile(int tileIndex, const std::vector<int>& channelIndices, cv::OutputArray tileRaster,
                      void* userData) override {
            cv::Mat tile = m_cacheManager->getTile(m_levelId, tileIndex);
            tileRaster.create(tile.size(), tile.type());
            Tools::extractChannels(tile, channelIndices, tileRaster);
            return true;
        }
        void initializeBlock(const cv::Size& blockSize, const std::vector<int>& channelIndices,
                             cv::OutputArray output) override {
            cv::Mat tile = m_cacheManager->getTile(m_levelId, 0);
            int channelCount = static_cast<int>(channelIndices.size());
            if (channelCount == 0) {
                channelCount = tile.channels();
            }
            output.create(blockSize, CV_MAKETYPE(tile.depth(), channelCount));
            output.setTo(0);
        }

    private:
        std::shared_ptr<CacheManager> m_cacheManager;
        cv::Size m_tileSize;
        int m_levelId;
    };
}
