// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#include "slideio/core/tools/cachemanager.hpp"
#include "slideio/core/tools/tempfile.hpp"

using namespace slideio;

CacheManager::CacheManager() {
}

CacheManager::~CacheManager()
{
}

void CacheManager::addCache(const Metadata& metadata, const cv::Mat& raster)
{
    std::string filePath = TempFile::createTempFile();
    cv::imwrite(filePath, raster);
    m_cachePointers[metadata] = std::make_pair(0, filePath);
}

cv::Mat CacheManager::getCache(const Metadata& metadata)
{
    auto it = m_cachePointers.find(metadata);
    if (it == m_cachePointers.end()) {
        return cv::Mat();
    }
    auto& cache = it->second;
    return it->second;
}