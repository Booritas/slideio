// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#ifndef OPENCV_slideio_ndpitifftools_HPP
#define OPENCV_slideio_ndpitifftools_HPP


#include "slideio/drivers/ndpi/ndpi_api_def.hpp"
#include "slideio/base/resolution.hpp"
#include "slideio/base/slideio_enums.hpp"
#include "slideio/base/base.hpp"
#include "slideio/base/randomaccessstream.hpp"
#include <opencv2/core.hpp>
#include <memory>
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
    class NDPIFile;

    struct SLIDEIO_NDPI_EXPORTS  NDPITiffDirectory
    {
        enum class Type
        {
            Tiled = 0,
            SingleStripe = 1,
            SingleStripeMCU = 2,
            Striped = 3
        };
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
        std::vector<uint64_t> mcuStarts;
        uint64_t jpegHeaderOffset;
        uint64_t jpegSOFMarker;
        uint32_t jpegHeaderSize;
        uint32_t rawStripSize = 0;
        bool auxImage = false;

        Type getType() const {
            if(tiled) {
                return Type::Tiled;
            }
            else if(tileWidth > 0 && tileHeight > 0 && !mcuStarts.empty() && slideioCompression == Compression::Jpeg) {
                return Type::SingleStripeMCU;
            }
            else if(rowsPerStrip == height) {
                return Type::SingleStripe;
            }
            else {
                return Type::Striped;
            }
        }
    };

    SLIDEIO_NDPI_EXPORTS std::ostream&  operator << (std::ostream& os, const NDPITiffDirectory::Type& type);

    // Random-access byte source for NDPI's raw (non-libtiff) read paths.
    // Backs the JPEG/MCU helpers with EITHER a local FILE* (path open, byte
    // identical to the legacy code) OR a RandomAccessStream (remote/in-memory).
    // Only the FILE* branch existed before; the stream branch makes the same
    // paths work over streams without touching the local fast path.
    class SLIDEIO_NDPI_EXPORTS NDPIDataSource
    {
    public:
        // Wraps a FILE*. When `owns` is true, the FILE* is closed on destruction.
        explicit NDPIDataSource(FILE* file, bool owns = false) : m_file(file), m_ownsFile(owns) {}
        explicit NDPIDataSource(std::shared_ptr<RandomAccessStream> stream)
            : m_stream(std::move(stream)) {}
        ~NDPIDataSource();
        NDPIDataSource(const NDPIDataSource&) = delete;
        NDPIDataSource& operator=(const NDPIDataSource&) = delete;
        NDPIDataSource(NDPIDataSource&& other) noexcept;
        NDPIDataSource& operator=(NDPIDataSource&& other) noexcept = delete;
        bool isValid() const { return m_file != nullptr || m_stream != nullptr; }
        bool isStream() const { return m_stream != nullptr; }
        FILE* file() const { return m_file; }
        // Reads `count` bytes at the current position into `buf`, advancing the
        // position. Returns the number of bytes read.
        size_t read(void* buf, size_t count);
        // Reads `count` bytes at absolute `offset` into `buf` (position-independent).
        size_t readAt(uint64_t offset, void* buf, size_t count);
        void seek(uint64_t pos);
        uint64_t pos() const { return m_pos; }
    private:
        FILE* m_file = nullptr;
        bool m_ownsFile = false;
        std::shared_ptr<RandomAccessStream> m_stream;
        uint64_t m_pos = 0;
    };

    class SLIDEIO_NDPI_EXPORTS NDPITiffTools
    {
    public:
        static libtiff::TIFF* openTiffFile(const std::string& path);
        // Stream-based overload (F3): opens an NDPI libtiff handle over an
        // arbitrary RandomAccessStream (remote/in-memory URIs). Uses NDPI-local
        // TIFFClientOpen callbacks (NOT slideio::openTiffFromStream) because the
        // NDPI driver links its own NDPI-patched libtiff fork (NDPITIFF) rather
        // than the standard libtiff used by slideio-imagetools; the handle must
        // be opened by NDPITIFF so NDPI's patched read functions work on it.
        static libtiff::TIFF* openTiffFile(std::shared_ptr<RandomAccessStream> stream);
        static void closeTiffFile(libtiff::TIFF* file);
        static cv::Size computeMCUTileSize(NDPIDataSource& src, const cv::Size& dirSize);
        static std::pair<uint64_t, uint64_t> getJpegHeaderPos(NDPIDataSource& src);
        static void readMCUTile(NDPIDataSource& src, const NDPITiffDirectory& dir, int tile, cv::OutputArray output);
        static void jpeglibDecodeTile(const uint8_t* jpg_buffer, size_t jpg_size, const cv::Size& tileSize, cv::OutputArray output);
        static void scanTiffDirTags(libtiff::TIFF* tiff, int dirIndex, int64_t dirOffset, slideio::NDPITiffDirectory& dir);
        static void updateJpegXRCompressedDirectoryMedatata(libtiff::TIFF* tiff, NDPITiffDirectory& dir);
        static void scanTiffDir(libtiff::TIFF* tiff, int dirIndex, int64_t dirOffset, slideio::NDPITiffDirectory& dir);
        static void readNotRGBStripedDir(libtiff::TIFF* tiff, const NDPITiffDirectory& dir, cv::_OutputArray output);
        static void readRegularStripedDir(libtiff::TIFF* file, const slideio::NDPITiffDirectory& dir, cv::OutputArray output);
        static void readJpegXRStripedDir(libtiff::TIFF* tiff, const NDPITiffDirectory& dir, cv::_OutputArray output);
        static void readStripedDir(libtiff::TIFF* file, const slideio::NDPITiffDirectory& dir, cv::OutputArray output);
        static void readJpegXRTile(libtiff::TIFF* tiff, const slideio::NDPITiffDirectory& dir, int tile, const std::vector<int>& vector, cv::OutputArray output);
        static void readTile(libtiff::TIFF* hFile, const slideio::NDPITiffDirectory& dir, int tile,
                             const std::vector<int>& channelIndices, cv::OutputArray output);
        static void readJpegXRStrip(libtiff::TIFF* tiff, const NDPITiffDirectory& dir, int strip, const std::vector<int>& vector, cv::_OutputArray output);
        static void readNotRGBStrip(libtiff::TIFF* tiff, const NDPITiffDirectory& dir, int strip, const std::vector<int>& vector, cv::_OutputArray output);
        static void readRegularStrip(libtiff::TIFF* tiff, const NDPITiffDirectory& dir, int strip, const std::vector<int>& vector, cv::_OutputArray output);
        static void readStripe(libtiff::TIFF* hFile, const slideio::NDPITiffDirectory& dir, int strip,
                              const std::vector<int>& channelIndices, cv::OutputArray output);
        static void setCurrentDirectory(libtiff::TIFF* hFile, const slideio::NDPITiffDirectory& dir);
        static void decodeJxrBlock(const uint8_t* data, size_t dataBlockSize, cv::OutputArray output);
        static void readRegularTile(libtiff::TIFF* hFile, const slideio::NDPITiffDirectory& dir, int tile,
                                    const std::vector<int>& channelIndices, cv::OutputArray output);
        static void readNotRGBTile(libtiff::TIFF* hFile, const slideio::NDPITiffDirectory& dir, int tile,
            const std::vector<int>& channelIndices, cv::OutputArray output);
        static int computeStripHeight(int height, int rowsPerStrip, int strip);
        static cv::Size computeTileSize(const NDPITiffDirectory& dir, int tile);
        static cv::Size computeTileCounts(const NDPITiffDirectory& dir);
        static void readJpegScanlines(libtiff::TIFF* tiff, FILE* file, const NDPITiffDirectory& dir, int firstScanline,
            int numberScanlines, const std::vector<int>& channelIndices, cv::_OutputArray output);
        static void readJpegDirectoryRegion(libtiff::TIFF* tiff, const std::string& filePath, const cv::Rect& region, const NDPITiffDirectory& dir,
            const std::vector<int>& channelIndices, cv::_OutputArray output);
        static void readDirectoryJpegHeaders(NDPIFile* ndpi, NDPITiffDirectory& dir);
        static void readUncompressedScanlines(libtiff::TIFF* tiff, FILE* file, const NDPITiffDirectory& dir, int firstScanline, int numberScanlines, const std::vector<int>& vector,
                                      cv::_OutputArray tileRaster);
    private:
        static void fixJpegHeader(const NDPITiffDirectory& dir, uint8_t* data);
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