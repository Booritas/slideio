// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#include "tests/main/gdaltiffwriter.hpp"
#include "slideio/imagetools/libtiff.hpp"

#include <gtest/gtest.h>
#include <algorithm>
#include <vector>

namespace
{
    void setCommonFields(libtiff::TIFF* tiff, const cv::Mat& image, bool planar, uint32_t subFileType,
                         int numSubIfds)
    {
        ASSERT_TRUE(image.channels() == 1 || image.channels() == 3) << "channels: " << image.channels();
        ASSERT_TRUE(image.depth() == CV_8U || image.depth() == CV_16U) << "depth: " << image.depth();
        libtiff::TIFFSetField(tiff, TIFFTAG_SUBFILETYPE, subFileType);
        libtiff::TIFFSetField(tiff, TIFFTAG_IMAGEWIDTH, image.cols);
        libtiff::TIFFSetField(tiff, TIFFTAG_IMAGELENGTH, image.rows);
        libtiff::TIFFSetField(tiff, TIFFTAG_SAMPLESPERPIXEL, image.channels());
        libtiff::TIFFSetField(tiff, TIFFTAG_BITSPERSAMPLE, image.depth() == CV_8U ? 8 : 16);
        libtiff::TIFFSetField(tiff, TIFFTAG_SAMPLEFORMAT, SAMPLEFORMAT_UINT);
        libtiff::TIFFSetField(tiff, TIFFTAG_PHOTOMETRIC,
                              image.channels() == 3 ? PHOTOMETRIC_RGB : PHOTOMETRIC_MINISBLACK);
        libtiff::TIFFSetField(tiff, TIFFTAG_PLANARCONFIG,
                              planar ? PLANARCONFIG_SEPARATE : PLANARCONFIG_CONTIG);
        libtiff::TIFFSetField(tiff, TIFFTAG_COMPRESSION, COMPRESSION_NONE);
        if (numSubIfds > 0) {
            std::vector<uint64_t> offsets(numSubIfds, 0);
            ASSERT_EQ(1, libtiff::TIFFSetField(tiff, TIFFTAG_SUBIFD, numSubIfds, offsets.data()));
        }
    }

    void writeTiles(libtiff::TIFF* tiff, const cv::Mat& image, int tileSize, bool planar, uint8_t padValue)
    {
        libtiff::TIFFSetField(tiff, TIFFTAG_TILEWIDTH, tileSize);
        libtiff::TIFFSetField(tiff, TIFFTAG_TILELENGTH, tileSize);
        const int width = image.cols;
        const int height = image.rows;
        std::vector<cv::Mat> planes;
        if (planar) {
            cv::split(image, planes);
        } else {
            planes.push_back(image);
        }
        for (int plane = 0; plane < static_cast<int>(planes.size()); ++plane) {
            const cv::Mat& source = planes[plane];
            cv::Mat tile(tileSize, tileSize, source.type());
            for (int y = 0; y < height; y += tileSize) {
                for (int x = 0; x < width; x += tileSize) {
                    // Every byte of the padding is padValue, whatever the depth.
                    std::fill(tile.data, tile.data + tile.total() * tile.elemSize(), padValue);
                    const cv::Rect part(x, y, std::min(tileSize, width - x), std::min(tileSize, height - y));
                    source(part).copyTo(tile(cv::Rect(0, 0, part.width, part.height)));
                    const auto tileIndex = libtiff::TIFFComputeTile(tiff, x, y, 0,
                                                                    static_cast<uint16_t>(plane));
                    const auto size = static_cast<libtiff::tmsize_t>(tile.total() * tile.elemSize());
                    ASSERT_EQ(size, libtiff::TIFFWriteEncodedTile(tiff, tileIndex, tile.data, size));
                }
            }
        }
    }
}

cv::Mat gdaltest::makeRgbImage(int width, int height, int seed)
{
    cv::Mat image(height, width, CV_8UC3);
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            image.at<cv::Vec3b>(y, x) = cv::Vec3b(static_cast<uint8_t>(x + seed),
                                                  static_cast<uint8_t>(y),
                                                  static_cast<uint8_t>((x / 7 + y / 5) * 13 + seed));
        }
    }
    return image;
}

cv::Mat gdaltest::makeGrayImage16(int width, int height, int seed)
{
    cv::Mat image(height, width, CV_16UC1);
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            image.at<uint16_t>(y, x) = static_cast<uint16_t>(x * 97 + y * 131 + seed);
        }
    }
    return image;
}

void gdaltest::writeTiledDirectory(libtiff::TIFF* tiff, const cv::Mat& image, int tileSize, bool planar,
                                   uint8_t padValue, uint32_t subFileType, int numSubIfds)
{
    ASSERT_NO_FATAL_FAILURE(setCommonFields(tiff, image, planar, subFileType, numSubIfds));
    ASSERT_NO_FATAL_FAILURE(writeTiles(tiff, image, tileSize, planar, padValue));
    ASSERT_EQ(1, libtiff::TIFFWriteDirectory(tiff));
}

void gdaltest::writeStripedDirectory(libtiff::TIFF* tiff, const cv::Mat& image, uint32_t subFileType,
                                     int numSubIfds)
{
    ASSERT_NO_FATAL_FAILURE(setCommonFields(tiff, image, false, subFileType, numSubIfds));
    libtiff::TIFFSetField(tiff, TIFFTAG_ROWSPERSTRIP, 16);
    for (int y = 0; y < image.rows; ++y) {
        cv::Mat row = image.row(y).clone();
        ASSERT_EQ(1, libtiff::TIFFWriteScanline(tiff, row.data, static_cast<uint32_t>(y), 0));
    }
    ASSERT_EQ(1, libtiff::TIFFWriteDirectory(tiff));
}

void gdaltest::writeTiledRgbDirectory(libtiff::TIFF* tiff, const cv::Mat& image, int tileSize, bool planar,
                                      uint8_t padValue)
{
    ASSERT_EQ(CV_8UC3, image.type());
    libtiff::TIFFSetField(tiff, TIFFTAG_IMAGEWIDTH, image.cols);
    libtiff::TIFFSetField(tiff, TIFFTAG_IMAGELENGTH, image.rows);
    libtiff::TIFFSetField(tiff, TIFFTAG_SAMPLESPERPIXEL, 3);
    libtiff::TIFFSetField(tiff, TIFFTAG_BITSPERSAMPLE, 8);
    libtiff::TIFFSetField(tiff, TIFFTAG_PHOTOMETRIC, PHOTOMETRIC_RGB);
    libtiff::TIFFSetField(tiff, TIFFTAG_PLANARCONFIG, planar ? PLANARCONFIG_SEPARATE : PLANARCONFIG_CONTIG);
    libtiff::TIFFSetField(tiff, TIFFTAG_COMPRESSION, COMPRESSION_NONE);
    ASSERT_NO_FATAL_FAILURE(writeTiles(tiff, image, tileSize, planar, padValue));
}

void gdaltest::writeTiledRgbTiff(const std::string& path, int width, int height, int tileSize, cv::Mat& image,
                                 bool planar, uint8_t padValue)
{
    image = makeRgbImage(width, height);
    libtiff::TIFF* tiff = libtiff::TIFFOpen(path.c_str(), "w");
    ASSERT_TRUE(tiff != nullptr);
    // Not wrapped: the handle is closed either way, and the caller sees the failure.
    writeTiledRgbDirectory(tiff, image, tileSize, planar, padValue);
    libtiff::TIFFClose(tiff);
}
