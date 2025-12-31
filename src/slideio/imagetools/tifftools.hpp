// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#ifndef OPENCV_slideio_tifftools_HPP
#define OPENCV_slideio_tifftools_HPP

#include <map>

#include "slideio/imagetools/slideio_imagetools_def.hpp"
#include "slideio/base/resolution.hpp"
#include "slideio/base/slideio_enums.hpp"
#include "slideio/base/base.hpp"
#include <opencv2/core.hpp>
#include <string>
#include <vector>

#include "imagetools.hpp"

namespace libtiff
{
    struct tiff;
    typedef tiff TIFF;
}

namespace slideio
{
    struct TiffDirectory
    {
        int width = 0;
        int height = 0;
        bool tiled = false;
        int tileWidth = 0;
        int tileHeight = 0;
        int channels = 0;
        int bitsPerSample = 0;
        int photometric = 0;
        int YCbCrSubsampling[2];
        int subFileType = 0;
        uint32_t compression = 0;
        Compression slideioCompression;
        int dirIndex = 0;
        int64 offset = 0;
        std::string description;
        std::string software;
        std::vector<TiffDirectory> subdirectories;
        Resolution res = {};
        cv::Point2d position = {};
        bool interleaved = false;
        int rowsPerStrip = 0;
        DataType dataType = DataType::DT_Unknown;
        int stripSize = 0;
        int compressionQuality = 0;
        uint64_t byteOffset = 0;
    };

    SLIDEIO_IMAGETOOLS_EXPORTS std::ostream& operator<<(std::ostream& os, const TiffDirectory& dir);
    SLIDEIO_IMAGETOOLS_EXPORTS std::ostream& operator<<(std::ostream& os, const std::vector<TiffDirectory>& dirs);

    class SLIDEIO_IMAGETOOLS_EXPORTS  TiffTools
    {
    public:
        static libtiff::TIFF* openTiffFile(const std::string& path, bool readOnly=true);
        static void closeTiffFile(libtiff::TIFF* file);
        static void setCurrentDirectory(libtiff::TIFF* tiff, int dirIndex, int64_t dirOffset);
        static void scanTiffDirTags(libtiff::TIFF* tiff, int dirIndex, int64_t dirOffset, slideio::TiffDirectory& dir);
        static void scanTiffDir(libtiff::TIFF* tiff, int dirIndex, int64_t dirOffset, slideio::TiffDirectory& dir);
        static void scanFile(libtiff::TIFF* file, std::vector<TiffDirectory>& directories);
        static void scanFile(const std::string& filePath, std::vector<TiffDirectory>& directories);
        static void readNotRGBStripedDir(libtiff::TIFF* tiff, const TiffDirectory& dir, cv::_OutputArray output);
        static void readRegularStripedDir(libtiff::TIFF* file, const slideio::TiffDirectory& dir, cv::OutputArray output);
        static void readPlanarStripedDir(libtiff::TIFF* tiff, const TiffDirectory& dir, cv::OutputArray output);
        static void readTiledDir(libtiff::TIFF* tiff, const TiffDirectory& dir, cv::OutputArray output);
        static void readStripedDir(libtiff::TIFF* file, const slideio::TiffDirectory& dir, cv::OutputArray output);
        static void readTile(libtiff::TIFF* hFile, const slideio::TiffDirectory& dir, int tile,
            const std::vector<int>& channelIndices, cv::OutputArray output);
        static void setCurrentDirectory(libtiff::TIFF* hFile, const slideio::TiffDirectory& dir);
        static void scaleBlockToDirectory(const TiffDirectory& basisDir, const TiffDirectory& dir,
                                   const cv::Rect& basisDirRect, cv::Rect& dirBlockRect);
        static void readJ2KTile(libtiff::TIFF* hFile, const slideio::TiffDirectory& dir, int tile,
                                const std::vector<int>& channelIndices, cv::OutputArray output);
        static void readRegularTile(libtiff::TIFF* hFile, const slideio::TiffDirectory& dir, int tile,
            const std::vector<int>& channelIndices, cv::OutputArray output);
        static void readNotRGBTile(libtiff::TIFF* hFile, const slideio::TiffDirectory& dir, int tile,
            const std::vector<int>& channelIndices, cv::OutputArray output);
        static void writeDirectory(libtiff::TIFF* tiff);
        static void setTags(libtiff::TIFF* tiff, const TiffDirectory& dir);
        static void initSubDirs(libtiff::TIFF* tiff, int numDirs);
        static void writeTile(libtiff::TIFF* tiff, int x, int y, Compression compression,
            const cv::Mat& tileRaster, const EncodeParameters& parameters,
            uint8_t* buffer=nullptr, int bufferSize=0);
        static std::string readStringTag(libtiff::TIFF* tiff, uint16_t tag);
        static int getNumberOfDirectories(libtiff::TIFF* tiff);
    };
}

#endif