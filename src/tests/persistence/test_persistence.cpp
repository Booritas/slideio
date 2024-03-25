#include <gtest/gtest.h>
#include "slideio/persistence/storage.hpp"
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

