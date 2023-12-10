// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#include "slideio/core/tools/cachemanager.hpp"
#include "slideio/core/tools/tempfile.hpp"

using namespace slideio;

CacheManager::CacheManager(const cv::Size& tileSize, const cv::Size& tileCounts) : m_tileSize(tileSize), m_tileCounts(tileCounts), m_lastPointer(0), m_type(-1) {
	m_tempFile = std::make_unique<TempFile>("bin");
    m_cacheFile.open(m_tempFile->getPath().c_str(), std::ios::binary | std::ios::in | std::ios::out | std::ios::trunc);
    m_cachePointers.reserve(m_tileCounts.area());
}

CacheManager::~CacheManager()
{
	m_cacheFile.close();
}

void CacheManager::saveTile(int x, int y, const cv::Mat& tile)
{
    if(m_type < 0)
	    m_type = tile.type();
    const int64_t tileIndex = y * m_tileCounts.width + x;
	const int tileSize = static_cast<int>(tile.total() * tile.elemSize());
    m_cacheFile.write(reinterpret_cast<const char*>(tile.data), tileSize);
    m_cachePointers[tileIndex] = std::make_pair(m_cacheFile.tellp(), tileSize);
}

cv::Mat CacheManager::getTile(int x, int y)
{
	const int64_t tileIndex = y * m_tileCounts.width + x;
	const int64_t tileOffset = m_cachePointers[tileIndex].first;
	const int tileSize = m_cachePointers[tileIndex].second;
	m_cacheFile.seekp(tileOffset);
	cv::Mat tile(m_tileSize, m_type);
	m_cacheFile.read((char*)tile.data, m_tileSize.area());
	return tile;
}

void CacheManager::visit(int x, int y, const cv::Mat& tile)
{
    saveTile(x, y, tile);
}
