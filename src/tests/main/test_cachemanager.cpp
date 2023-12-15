
#include <gtest/gtest.h>
#include "slideio/core/tools/cachemanager.hpp"

using namespace slideio;

TEST(CacheManagerTest, AddAndGetCache) {
    // Create a CacheManager object
    CacheManager cacheManager;

    // Create some test metadata and raster
    CacheManager::Metadata metadata1;
    metadata1.zoomX = 1.0;
    metadata1.zoomY = 1.0;
    metadata1.rect = cv::Rect(0, 0, 100, 100);
    cv::Mat raster1(100, 100, CV_8UC3, cv::Scalar(255, 0, 0));
    // Add the cache to the CacheManager
    cacheManager.addCache(metadata1, raster1);

    // Retrieve the cache using the metadata
    cv::Mat retrievedRaster = cacheManager.getCache(metadata1);

    // Check if the retrieved cache matches the original raster
    ASSERT_EQ(retrievedRaster.rows, raster1.rows);
    ASSERT_EQ(retrievedRaster.cols, raster1.cols);
    ASSERT_EQ(retrievedRaster.type(), raster1.type());
    ASSERT_TRUE(cv::countNonZero(retrievedRaster != raster1) == 0);
}

TEST(CacheManagerTest, EmptyCache) {
    // Create a CacheManager object
    CacheManager cacheManager;

    // Create some test metadata
    CacheManager::Metadata metadata1;

    // Retrieve a non-existing cache using the metadata
    cv::Mat retrievedRaster = cacheManager.getCache(metadata1);

    // Check if the retrieved cache is empty
    ASSERT_TRUE(retrievedRaster.empty());
}

TEST(CacheManagerTest, NotExistingCache) {
    // Create a CacheManager object
    CacheManager cacheManager;

    // Create some test metadata and raster
    CacheManager::Metadata metadata1;
    metadata1.zoomX = 1.0;
    metadata1.zoomY = 1.0;
    metadata1.rect = cv::Rect(0, 0, 100, 100);
    cv::Mat raster1(100, 100, CV_8UC3, cv::Scalar(255, 0, 0));
    // Add the cache to the CacheManager
    cacheManager.addCache(metadata1, raster1);

    CacheManager::Metadata metadata2;
    metadata2.zoomX = 1.0001;
    metadata2.zoomY = 1.0;
    // Retrieve the cache using the metadata
    cv::Mat retrievedRaster = cacheManager.getCache(metadata2);

    // Check if the retrieved cache is empty
    ASSERT_TRUE(retrievedRaster.empty());
}

TEST(CacheManagerTest, MultipleValues) {
    // Create a CacheManager object
    CacheManager cacheManager;

    // Create some test metadata and raster
    CacheManager::Metadata metadata2;
    metadata2.zoomX = 1.000004;
    metadata2.zoomY = 1.0;
    metadata2.rect = cv::Rect(0, 0, 100, 100);
    cv::Mat raster1(100, 100, CV_8UC1, cv::Scalar(0));
    // Add the cache to the CacheManager
    cacheManager.addCache(metadata2, raster1);


    // Retrieve the cache using the metadata
    cv::Mat retrievedRaster = cacheManager.getCache(metadata2);


    for(int i = 0; i < 10; ++i) {
               // Create some test metadata and raster
        CacheManager::Metadata metadata1;
        metadata1.zoomX = 1.0 + double(i) * 0.01;
        metadata1.zoomY = 1.0;
        metadata1.rect = cv::Rect(0, 0, 100, 100);
        cv::Mat raster1(10+i+1, 10, CV_8UC1, cv::Scalar(0));
        // Add the cache to the CacheManager
        cacheManager.addCache(metadata1, raster1);
    }

    // Check if the retrieved cache matches the original raster
    ASSERT_EQ(retrievedRaster.rows, raster1.rows);
    ASSERT_EQ(retrievedRaster.cols, raster1.cols);
    ASSERT_EQ(retrievedRaster.type(), raster1.type());
    ASSERT_TRUE(cv::countNonZero(retrievedRaster != raster1) == 0);
}

TEST(CacheManagerTest, SimilarZooms) {
    // Create a CacheManager object
    CacheManager cacheManager;

    // Create some test metadata and raster
    CacheManager::Metadata metadata1;
    metadata1.zoomX = 1.0;
    metadata1.zoomY = 1.0;
    metadata1.rect = cv::Rect(0, 0, 100, 100);
    cv::Mat raster1(100, 100, CV_8UC3, cv::Scalar(255, 0, 0));
    // Add the cache to the CacheManager
    cacheManager.addCache(metadata1, raster1);

    CacheManager::Metadata metadata2;
    metadata2.zoomX = 1.0001;
    metadata2.zoomY = 1.0;
    metadata2.rect = cv::Rect(0, 0, 100, 100);
    // Retrieve the cache using the metadata
    cv::Mat retrievedRaster = cacheManager.getCache(metadata2);

    // Check if the retrieved cache is empty
    ASSERT_TRUE(retrievedRaster.empty());

    metadata2.zoomX = 1.00001;
    retrievedRaster = cacheManager.getCache(metadata2);
    ASSERT_FALSE(retrievedRaster.empty());
}
