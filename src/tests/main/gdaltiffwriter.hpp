// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#pragma once
// Raw libtiff writers for the GDAL driver tests. They deliberately do not go
// through TiffTools' write path, so a mistake shared by a writer and the reader
// under test cannot cancel itself out.
#include <opencv2/core.hpp>
#include <cstdint>
#include <string>

namespace libtiff
{
    struct tiff;
    typedef tiff TIFF;
}

namespace gdaltest
{
    // An RGB image whose every pixel is a function of its position (and of seed,
    // so two pages of one file differ), so a tile placed in the wrong spot cannot
    // go unnoticed.
    cv::Mat makeRgbImage(int width, int height, int seed = 0);
    // The single-channel 16-bit counterpart, using the whole 16-bit range.
    cv::Mat makeGrayImage16(int width, int height, int seed = 0);

    // Writes image as one tiled, uncompressed directory and closes it with
    // TIFFWriteDirectory. image has 1 or 3 channels, CV_8U or CV_16U. The padding
    // past the right and bottom edges is padValue rather than zero, so a read that
    // lets it bleed into the page shows it. numSubIfds above 0 declares that many
    // SubIFDs: as libtiff requires, they are the next numSubIfds directories written.
    void writeTiledDirectory(libtiff::TIFF* tiff, const cv::Mat& image, int tileSize, bool planar,
                             uint8_t padValue, uint32_t subFileType = 0, int numSubIfds = 0);
    // The same for a striped (16 rows per strip), interleaved directory.
    void writeStripedDirectory(libtiff::TIFF* tiff, const cv::Mat& image, uint32_t subFileType = 0,
                               int numSubIfds = 0);

    // The original helper: fills the current directory with an RGB tiled image but
    // does not write it -- the caller, or TIFFClose, does.
    void writeTiledRgbDirectory(libtiff::TIFF* tiff, const cv::Mat& image, int tileSize, bool planar,
                                uint8_t padValue);
    // A tiled RGB tiff whose size is not a multiple of the tile, so the right and
    // bottom tiles are partial.
    void writeTiledRgbTiff(const std::string& path, int width, int height, int tileSize, cv::Mat& image,
                           bool planar = false, uint8_t padValue = 255);
}
