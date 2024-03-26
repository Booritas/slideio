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

TEST(HDF5Storage, writeTile) {
    const slideio::TempFile tmp("h5");
    const std::string outputPath = tmp.getPath().string();
    const cv::Size imageSize(100, 100);
    const cv::Point offset(5, 5);
    const cv::Size tileSize = {10, 10};

    cv::Mat tile(tileSize, CV_32S);
    tile.setTo(5);

    std::shared_ptr<HDF5Storage> storage = HDF5Storage::createStorage(outputPath, imageSize, cv::Size(50, 50));

    storage->writeTile(tile, offset);
    cv::Mat readTile;
    storage->readTile(offset, tileSize, readTile);

    TestTools::compareRasters(tile, readTile);

    cv::Mat image(imageSize, CV_32S);
    storage->readTile(cv::Point(0, 0), imageSize, image);
    cv::Mat imageROI = image(cv::Rect(offset, tileSize));
    TestTools::compareRasters(tile, imageROI);
}

//TEST(HDF5Storage, writeTile2) {
//    const slideio::TempFile tmp("h5", false);
//    const std::string outputPath = tmp.getPath().string();
//    const cv::Point offset(5, 5);
//    const cv::Size tileSize = {1000, 1000};
//    const int tilesX = 10;
//    const int tilesY = 10;
//    const cv::Size imageSize(tileSize.width*tilesX, tileSize.height*tilesY);
//
//    cv::Mat tile(tileSize, CV_32S);
//
//    std::shared_ptr<HDF5Storage> storage = HDF5Storage::createStorage(outputPath, imageSize, cv::Size(50, 50));
//    int32_t low = 0;
//    int32_t high = 1000000;
//    int tiles = tilesX * tilesY;
//    int tileCount = 0;
//
//
//
//    for (int y = 0; y < tilesY; ++y) {
//        for (int x = 0; x < tilesX; ++x) {
//            cv::Point offset(x * tileSize.width, y * tileSize.height);
//            //cv::randu(tile, low, high);
//            int randomNumber = std::rand() % 100001;
//            tile.setTo(randomNumber);
//            storage->writeTile(tile, offset);
//            tileCount++;
//            std::cout << "Tile " << tileCount << " of " << tiles << "(" << randomNumber << ")" << std::endl;
//        }
//    }
//    //storage->writeTile(tile, offset);
//    //cv::Mat readTile;
//    //storage->readTile(offset, tileSize, readTile);
//
//    //TestTools::compareRasters(tile, readTile);
//
//    storage->closeStorage();
//
//    //cv::Mat image(imageSize, CV_32S);
//    //storage->readTile(cv::Point(0, 0), imageSize, image);
//    //cv::Mat imageROI = image(cv::Rect(offset, tileSize));
//    //TestTools::compareRasters(tile, imageROI);
//}
