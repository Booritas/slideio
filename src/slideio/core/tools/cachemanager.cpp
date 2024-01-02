// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#include "slideio/core/tools/cachemanager.hpp"

#include <opencv2/imgproc.hpp>

#include "tools.hpp"
#include "slideio/base/exceptions.hpp"
#include "slideio/core/tools/tempfile.hpp"

using namespace slideio;

CacheManager::CacheManager() = default;

CacheManager::~CacheManager() = default;

void CacheManager::addTile(int level, cv::Point2i pos, const cv::Mat& tile)
{
	cv::Mat tileCopy;
	tile.copyTo(tileCopy);
    m_cache.push_back(tileCopy);
    int tileIndex = static_cast<int>(m_cache.size() - 1);
    if(m_levels.find(level) == m_levels.end()) {
        m_levels[level] = std::make_shared<Level>(Level(level));
    }
    cv::Rect rect(pos, tile.size());
    m_levels[level]->addTile(rect, tileIndex);

}

int CacheManager::getTileCount(int level) const
{
	auto it = m_levels.find(level);
	if(it == m_levels.end()) {
	    return 0;
	}
	return it->second->getTileCount();
}

cv::Mat CacheManager::getTile(int levelId, int index) const
{
	auto it = m_levels.find(levelId);
	if(it == m_levels.end()) {
		RAISE_RUNTIME_ERROR << "CacheManager: Level " << levelId << " is not found for getTile method.";
	}
	const Level& level = *it->second;
	if(level.getTileCount() <= index) {
	    RAISE_RUNTIME_ERROR << "CacheManager: Invalid tile index " << index << " for level " << levelId;
    }
	const int cacheIndex = level.getTileCacheIndex(index);
	if(cacheIndex < 0 || cacheIndex >= static_cast<int>(m_cache.size())) {
	    RAISE_RUNTIME_ERROR << "CacheManager: Invalid cache index " << cacheIndex << " for level " << levelId;
    }
	return m_cache[cacheIndex];
}

const cv::Rect CacheManager::getTileRect(int levelId, int tileIndex) const
{
	auto it = m_levels.find(levelId);
	if(it == m_levels.end()) {
	    RAISE_RUNTIME_ERROR << "CacheManager: Level " << levelId << " is not found for getTileRect method.";
	}
	const Level& level = *it->second;
	if(level.getTileCount() <= tileIndex) {
	    RAISE_RUNTIME_ERROR << "CacheManager: Invalid tile index " << tileIndex << " for level " << levelId;
    }
	return level.getTileRect(tileIndex);
}

