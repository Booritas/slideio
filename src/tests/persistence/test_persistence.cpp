#include <gtest/gtest.h>
#include "slideio/persistence/storage.hpp"
#include "slideio/persistence/hdf5storage.hpp"
#include "slideio/core/tools/tempfile.hpp"
#include "slideio/base/exceptions.hpp"

using namespace slideio;

TEST(Persistence, openNotExitingFile)
{
	const slideio::TempFile tmp("hdf5");
	const std::string outputPath = tmp.getPath().string();

   ASSERT_THROW(Storage::openStorage(outputPath), slideio::RuntimeError);
}

TEST(Persistence, createFile)
{
	const slideio::TempFile tmp("svs");
	const std::string outputPath = tmp.getPath().string();

   auto storage = Storage::createStorage(outputPath, cv::Size(10000, 10000));
   ASSERT_TRUE(storage!=nullptr);
}

TEST(HDF5Storage, writeTile)
{
	const slideio::TempFile tmp("svs");
	const std::string outputPath = tmp.getPath().string();

   cv::Mat tile(10, 10, CV_32S);
   tile.setTo(5);
   cv::Point offset(5, 5);

   std::shared_ptr<HDF5Storage> storage = HDF5Storage::createStorage(outputPath, cv::Size(100, 100), cv::Size(50, 50));

   storage->writeTile(tile, offset);

}