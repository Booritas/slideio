#include <gtest/gtest.h>
#include "slideio/persistence/storage.hpp"
#include "slideio/persistence/hdf5storage.hpp"
#include "slideio/core/tools/tempfile.hpp"
#include "slideio/base/exceptions.hpp"
#include "tests/testlib/testtools.hpp"

using namespace slideio;

TEST(Persistence, openNotExitingFile) {
    const slideio::TempFile tmp("h5");
    const std::string outputPath = tmp.getPath().string();

    ASSERT_THROW(Storage::openStorage(outputPath), slideio::RuntimeError);
}

TEST(Persistence, createFile) {
    const slideio::TempFile tmp("h5");
    const std::string outputPath = tmp.getPath().string();

    auto storage = Storage::createStorage(outputPath, cv::Size(10000, 10000));
    ASSERT_TRUE(storage!=nullptr);
}

TEST(HDF5Storage, writeBlock) {
    const slideio::TempFile tmp("h5");
    const std::string outputPath = tmp.getPath().string();
    const cv::Size imageSize(100, 100);
    const cv::Point offset(5, 5);
    const cv::Size tileSize = {10, 10};

    cv::Mat tile(tileSize, CV_32S);
    tile.setTo(5);

    std::shared_ptr<HDF5Storage> storage = HDF5Storage::createStorage(outputPath, imageSize, cv::Size(50, 50));

    storage->writeBlock(tile, offset);
    cv::Mat readTile;
    storage->readBlock(offset, tileSize, readTile);

    TestTools::compareRasters(tile, readTile);

    cv::Mat image(imageSize, CV_32S);
    storage->readBlock(cv::Point(0, 0), imageSize, image);
    cv::Mat imageROI = image(cv::Rect(offset, tileSize));
    TestTools::compareRasters(tile, imageROI);
}

TEST(HDF5Storage, readWriteBlock) {
    const slideio::TempFile tmp("h5");
    const std::string outputPath = tmp.getPath().string();
    const cv::Size imageSize(400, 500);
    const cv::Size tileSize = {200, 250};

    std::shared_ptr<HDF5Storage> storage = HDF5Storage::createStorage(outputPath, imageSize, tileSize);

    const cv::Size blockSize = { 200, 250 };

    for(int y = 0; y < imageSize.height; y += blockSize.height) {
        for (int x = 0; x < imageSize.width; x += blockSize.width) {
            const int value = y*10 + x;
            cv::Mat tile(tileSize, CV_32S);
            tile.setTo(value);
            cv::Point offset(x, y);
            storage->writeBlock(tile, offset);
        }
    }

    for (int y = 0; y < imageSize.height; y += blockSize.height) {
        for (int x = 0; x < imageSize.width; x += blockSize.width) {
            const int value = y * 10 + x;
            cv::Mat readBlock(tileSize, CV_32S);
            cv::Mat testBlock(tileSize, CV_32S);
            testBlock.setTo(value);
            cv::Point offset(x, y);
            storage->readBlock(offset, blockSize, readBlock);
            TestTools::compareRasters(readBlock, testBlock);
        }
    }
}
