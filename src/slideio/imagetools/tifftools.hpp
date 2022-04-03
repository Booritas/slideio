// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#ifndef OPENCV_slideio_tifftools_HPP
#define OPENCV_slideio_tifftools_HPP

#include "slideio/slideio_def.hpp"
#include "slideio/core/cvstructs.hpp"
#include "slideio/structs.hpp"
#include "slideio/slideio_enums.hpp"
#include "slideio/base.hpp"
#include <opencv2/core.hpp>
#include <string>
#include <vector>

namespace libtiff
{
    struct tiff;
    typedef tiff TIFF;
}

namespace slideio
{
    struct TiffDirectory
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
        std::vector<TiffDirectory> subdirectories;
        Resolution res;
        cv::Point2d position;
        bool interleaved;
        int rowsPerStrip;
        DataType dataType;
        int stripSize;
    };


    class SLIDEIO_EXPORTS  TiffTools
    {
    public:
        static libtiff::TIFF* openTiffFile(const std::string& path);
        static void closeTiffFile(libtiff::TIFF* file);
        static void scanTiffDirTags(libtiff::TIFF* tiff, int dirIndex, int64_t dirOffset, slideio::TiffDirectory& dir);
        static void scanTiffDir(libtiff::TIFF* tiff, int dirIndex, int64_t dirOffset, slideio::TiffDirectory& dir);
        static void scanFile(libtiff::TIFF* file, std::vector<TiffDirectory>& directories);
        static void scanFile(const std::string& filePath, std::vector<TiffDirectory>& directories);
        static void readNotRGBStripedDir(libtiff::TIFF* tiff, const TiffDirectory& dir, cv::_OutputArray output);
        static void readRegularStripedDir(libtiff::TIFF* file, const slideio::TiffDirectory& dir, cv::OutputArray output);
        static void readStripedDir(libtiff::TIFF* file, const slideio::TiffDirectory& dir, cv::OutputArray output);
        static void readTile(libtiff::TIFF* hFile, const slideio::TiffDirectory& dir, int tile,
            const std::vector<int>& channelIndices, cv::OutputArray output);
        static void setCurrentDirectory(libtiff::TIFF* hFile, const slideio::TiffDirectory& dir);
        static void readJ2KTile(libtiff::TIFF* hFile, const slideio::TiffDirectory& dir, int tile,
            const std::vector<int>& channelIndices, cv::OutputArray output);
        static void readRegularTile(libtiff::TIFF* hFile, const slideio::TiffDirectory& dir, int tile,
            const std::vector<int>& channelIndices, cv::OutputArray output);
        static void readNotRGBTile(libtiff::TIFF* hFile, const slideio::TiffDirectory& dir, int tile,
            const std::vector<int>& channelIndices, cv::OutputArray output);
    };

    class SLIDEIO_EXPORTS TIFFKeeper
    {
    public:
        TIFFKeeper(libtiff::TIFF* hfile=nullptr);
        ~TIFFKeeper();
        libtiff::TIFF* getHandle() const{
            return m_hFile;
        }
        bool isValid() const{
            return getHandle() != nullptr;
        }
        operator libtiff::TIFF* () const {
            return getHandle();
        }
        TIFFKeeper& operator = (libtiff::TIFF* hFile){
            m_hFile = hFile;
            return *this;
        }

    private:
        libtiff::TIFF* m_hFile;
    };
}

//boost::log::basic_formatting_ostream& operator << (boost::log::basic_formatting_ostream& os, const slideio::TiffDirectory& dir);
//boost::log::basic_formatting_ostream& operator << (boost::log::basic_formatting_ostream& os, const std::vector<slideio::TiffDirectory>& dirs);

#endif