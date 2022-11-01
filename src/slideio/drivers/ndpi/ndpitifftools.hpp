// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#ifndef OPENCV_slideio_ndpitifftools_HPP
#define OPENCV_slideio_ndpitifftools_HPP


#include "slideio/drivers/ndpi/ndpi_api_def.hpp"
#include "slideio/core/cvstructs.hpp"
#include "slideio/core/structs.hpp"
#include "slideio/core/slideio_enums.hpp"
#include "slideio/core/base.hpp"
#include <opencv2/core.hpp>
#include <string>
#include <vector>

namespace libtiff
{
    struct tiff;
    typedef tiff TIFF;
}

#if defined(_MSC_VER)
#pragma warning( push )
#pragma warning(disable: 4251)
#endif

namespace slideio
{
    struct SLIDEIO_NDPI_EXPORTS  NDPITiffDirectory
    {
        int width;
        int height;
        bool tiled;
        int tileWidth;
        int tileHeight;
        int channels;
        int bitsPerSample;
        int photometric;
        int YCbCrSubsampling[2];
        uint32_t compression;
        Compression slideioCompression;
        int dirIndex;
        int64 offset;
        std::string description;
        std::string userLabel;
        std::string comments;
        std::vector<NDPITiffDirectory> subdirectories;
        Resolution res;
        cv::Point2d position;
        bool interleaved;
        int rowsPerStrip;
        DataType dataType;
        int stripSize;
        double magnification;
        uint32_t blankLines;
    };


    class SLIDEIO_NDPI_EXPORTS NDPITiffTools
    {
    public:
        static libtiff::TIFF* openTiffFile(const std::string& path);
        static void closeTiffFile(libtiff::TIFF* file);
        static void scanTiffDirTags(libtiff::TIFF* tiff, int dirIndex, int64_t dirOffset, slideio::NDPITiffDirectory& dir);
        static void scanTiffDir(libtiff::TIFF* tiff, int dirIndex, int64_t dirOffset, slideio::NDPITiffDirectory& dir);
        static void scanFile(libtiff::TIFF* file, std::vector<NDPITiffDirectory>& directories);
        static void scanFile(const std::string& filePath, std::vector<NDPITiffDirectory>& directories);
        static void readNotRGBStripedDir(libtiff::TIFF* tiff, const NDPITiffDirectory& dir, cv::_OutputArray output);
        static void readRegularStripedDir(libtiff::TIFF* file, const slideio::NDPITiffDirectory& dir, cv::OutputArray output);
        static void readStripedDir(libtiff::TIFF* file, const slideio::NDPITiffDirectory& dir, cv::OutputArray output);
        static void readJpegXRTile(libtiff::TIFF* tiff, const slideio::NDPITiffDirectory& dir, int tile, const std::vector<int>& vector, cv::OutputArray output);
        static void readTile(libtiff::TIFF* hFile, const slideio::NDPITiffDirectory& dir, int tile,
                             const std::vector<int>& channelIndices, cv::OutputArray output);
        static void readJpegXRStrip(libtiff::TIFF* tiff, const NDPITiffDirectory& dir, int strip, const std::vector<int>& vector, cv::_OutputArray output);
        static void readNotRGBStrip(libtiff::TIFF* tiff, const NDPITiffDirectory& dir, int strip, const std::vector<int>& vector, cv::_OutputArray output);
        static void readRegularStrip(libtiff::TIFF* tiff, const NDPITiffDirectory& dir, int strip, const std::vector<int>& vector, cv::_OutputArray output);
        static void readStrip(libtiff::TIFF* hFile, const slideio::NDPITiffDirectory& dir, int strip,
                              const std::vector<int>& channelIndices, cv::OutputArray output);
        static void setCurrentDirectory(libtiff::TIFF* hFile, const slideio::NDPITiffDirectory& dir);
        static void decodeJxrBlock(const uint8_t* data, size_t dataBlockSize, cv::OutputArray output);
        static void test0(const std::string& path);
        static void readRegularTile(libtiff::TIFF* hFile, const slideio::NDPITiffDirectory& dir, int tile,
                                    const std::vector<int>& channelIndices, cv::OutputArray output);
        static void readNotRGBTile(libtiff::TIFF* hFile, const slideio::NDPITiffDirectory& dir, int tile,
            const std::vector<int>& channelIndices, cv::OutputArray output);
        static int computeStripHeight(int height, int rowsPerStrip, int strip);
        static cv::Size computeTileSize(const NDPITiffDirectory& dir, int tile);
        static cv::Size computeTileCounts(const NDPITiffDirectory& dir);
        static void readScanlines(libtiff::TIFF* tiff, FILE* file, const NDPITiffDirectory& dir, int firstScanline,
            int numberScanlines, const std::vector<int>& channelIndices, cv::_OutputArray output);
    };

    class  NDPITIFFKeeper
    {
    public:
        NDPITIFFKeeper(libtiff::TIFF* hfile=nullptr);
        ~NDPITIFFKeeper();
        libtiff::TIFF* getHandle() const{
            return m_hFile;
        }
        bool isValid() const{
            return getHandle() != nullptr;
        }
        operator libtiff::TIFF* () const {
            return getHandle();
        }
        NDPITIFFKeeper& operator = (libtiff::TIFF* hFile){
            m_hFile = hFile;
            return *this;
        }

    private:
        libtiff::TIFF* m_hFile;
    };
}

#if defined(_MSC_VER)
#pragma warning( pop )
#endif

#endif