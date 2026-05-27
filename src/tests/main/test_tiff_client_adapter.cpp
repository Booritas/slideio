// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#include <gtest/gtest.h>

#include "slideio/imagetools/tifftools.hpp"
#include "tests/testlib/testtools.hpp"
#include "memorystream.hpp"

#include <fstream>
#include <vector>

namespace {

std::vector<uint8_t> readFileBytes(const std::string& path)
{
    std::ifstream in(path, std::ios::binary | std::ios::ate);
    EXPECT_TRUE(in.is_open()) << "Cannot open test file: " << path;
    const std::streamsize sz = in.tellg();
    in.seekg(0);
    std::vector<uint8_t> bytes(static_cast<size_t>(sz));
    in.read(reinterpret_cast<char*>(bytes.data()), sz);
    return bytes;
}

} // namespace

TEST(TiffClientAdapterTest, ReadsSameDirectoriesAsPathOverload)
{
    const std::string path = TestTools::getTestImagePath("svs", "CMU-1-Small-Region.svs");

    std::vector<slideio::TiffDirectory> viaPath;
    slideio::TiffTools::scanFile(path, viaPath);

    auto stream = std::make_shared<slideio::tests::MemoryStream>(readFileBytes(path), path);
    libtiff::TIFF* tiff = slideio::TiffTools::openTiffFile(stream);
    ASSERT_NE(tiff, nullptr);
    std::vector<slideio::TiffDirectory> viaStream;
    slideio::TiffTools::scanFile(tiff, viaStream);
    slideio::TiffTools::closeTiffFile(tiff);

    ASSERT_EQ(viaPath.size(), viaStream.size());
    for (size_t i = 0; i < viaPath.size(); ++i) {
        EXPECT_EQ(viaPath[i].width, viaStream[i].width) << "dir " << i;
        EXPECT_EQ(viaPath[i].height, viaStream[i].height) << "dir " << i;
        EXPECT_EQ(viaPath[i].tiled, viaStream[i].tiled) << "dir " << i;
        EXPECT_EQ(viaPath[i].channels, viaStream[i].channels) << "dir " << i;
        EXPECT_EQ(viaPath[i].tileWidth, viaStream[i].tileWidth) << "dir " << i;
        EXPECT_EQ(viaPath[i].tileHeight, viaStream[i].tileHeight) << "dir " << i;
    }
}

TEST(TiffClientAdapterTest, ReadsTileDataIdentically)
{
    const std::string path = TestTools::getTestImagePath("svs", "CMU-1-Small-Region.svs");

    // Open via path.
    libtiff::TIFF* tiffPath = slideio::TiffTools::openTiffFile(path);
    ASSERT_NE(tiffPath, nullptr);
    slideio::TiffDirectory dirPath;
    slideio::TiffTools::scanTiffDir(tiffPath, 0, 0, dirPath);
    dirPath.dataType = slideio::DataType::DT_Byte;

    // Open via stream.
    auto stream = std::make_shared<slideio::tests::MemoryStream>(readFileBytes(path), path);
    libtiff::TIFF* tiffStream = slideio::TiffTools::openTiffFile(stream);
    ASSERT_NE(tiffStream, nullptr);
    slideio::TiffDirectory dirStream;
    slideio::TiffTools::scanTiffDir(tiffStream, 0, 0, dirStream);
    dirStream.dataType = slideio::DataType::DT_Byte;

    const int tileSx = (dirPath.width - 1) / dirPath.tileWidth + 1;
    const int tile = 5 * tileSx + 5;
    const std::vector<int> channelIndices = {0};

    cv::Mat tilePath, tileStream;
    slideio::TiffTools::readTile(tiffPath, dirPath, tile, channelIndices, tilePath);
    slideio::TiffTools::readTile(tiffStream, dirStream, tile, channelIndices, tileStream);

    slideio::TiffTools::closeTiffFile(tiffPath);
    slideio::TiffTools::closeTiffFile(tiffStream);

    ASSERT_EQ(tilePath.rows, tileStream.rows);
    ASSERT_EQ(tilePath.cols, tileStream.cols);
    ASSERT_EQ(tilePath.type(), tileStream.type());
    EXPECT_EQ(0.0, cv::norm(tilePath, tileStream, cv::NORM_INF));
}
