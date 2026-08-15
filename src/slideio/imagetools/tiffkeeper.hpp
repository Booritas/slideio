// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.

#pragma once
#include <string>

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
    class TIFFMessageHandler;

    // Both constructors install a TIFFMessageHandler, which swaps libtiff's
    // PROCESS-GLOBAL error and warning handlers for the keeper's lifetime and restores
    // them on destruction. With overlapping, non-LIFO keeper lifetimes one destructor can
    // restore a handler while another keeper is still alive, after which libtiff messages
    // go to stderr instead of the log. There is no dangling pointer -- both handlers are
    // static functions -- so the consequence is lost log routing, not a crash.
    class SLIDEIO_IMAGETOOLS_EXPORTS TIFFKeeper
    {
    public:
        TIFFKeeper(libtiff::TIFF* hfile = nullptr);
        TIFFKeeper(const std::string& filePath, bool readOnly = true);
        ~TIFFKeeper();
        libtiff::TIFF* getHandle() const {
            return m_hFile;
        }
        bool isValid() const {
            return getHandle() != nullptr;
        }
        operator libtiff::TIFF* () const {
            return getHandle();
        }
        TIFFKeeper& operator = (libtiff::TIFF* hFile) {
            m_hFile = hFile;
            return *this;
        }
        libtiff::TIFF* release() {
            libtiff::TIFF* handle = m_hFile;
            m_hFile = nullptr;
            return handle;
        }
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
        libtiff::TIFF* m_hFile;
        std::shared_ptr<TIFFMessageHandler> m_messageHandler;

    };
}

#define TIFFKeeperPtr std::shared_ptr<slideio::TIFFKeeper>
