// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.

#pragma once
#include <string>
#include <memory>
#include <cstdint>

#include "tifftools.hpp"
#include "slideio/imagetools/slideio_imagetools_def.hpp"
#include "slideio/imagetools/imagetools.hpp"

namespace libtiff
{
    struct tiff;
    typedef tiff TIFF;
}

namespace slideio
{
    // The libtiff error and warning handlers are installed once, at library
    // initialisation (see installTiffMessageHandlers()); a TIFFKeeper's
    // lifetime no longer touches them.
    class SLIDEIO_IMAGETOOLS_EXPORTS TIFFKeeper
    {
    public:
        explicit TIFFKeeper(libtiff::TIFF* hFile = nullptr);
        explicit TIFFKeeper(const std::string& filePath, bool readOnly = true);
        ~TIFFKeeper();

        // An owning handle must not be copied: two owners means two closes, and the
        // second one operates on a pointer libtiff has already freed.
        TIFFKeeper(const TIFFKeeper&)            = delete;
        TIFFKeeper& operator=(const TIFFKeeper&) = delete;
        // After the move, `other` owns nothing: m_hFile is null. A moved-from keeper
        // must not be revived via reset()/openTiffFile() -- it is fit only to be
        // destroyed or move-/copy-assigned over.
        TIFFKeeper(TIFFKeeper&& other) noexcept;
        TIFFKeeper& operator=(TIFFKeeper&& other) noexcept;

        libtiff::TIFF* getHandle() const {
            return m_hFile;
        }
        bool isValid() const {
            return m_hFile != nullptr;
        }
        // Takes ownership of a raw handle, closing any handle already held. Replaces the
        // old operator=(TIFF*), which overwrote the member and leaked what it replaced.
        void reset(libtiff::TIFF* hFile = nullptr);
        // Gives up ownership without closing: the caller closes it from here on.
        libtiff::TIFF* release();
        void openTiffFile(const std::string& filePath, bool readOnly = true);
        void closeTiffFile();
        void writeDirectory();
        void setTags(const TiffDirectory& dir);
        void writeTile(int x, int y, Compression compression, const EncodeParameters& params, const cv::Mat& mat,
            uint8_t* buffer, int bufferSize);
        void readTile(const slideio::TiffDirectory& dir, int tile,
            const std::vector<int>& channelIndices, cv::OutputArray output);
        std::string readStringTag(uint16_t tag);
        void initSubDirs(int numDirs);
        void writeRawTile(int x, int y, const uint8_t* data, int size);

    private:
        libtiff::TIFF* m_hFile = nullptr;
    };

    using TIFFKeeperPtr = std::shared_ptr<TIFFKeeper>;
}
