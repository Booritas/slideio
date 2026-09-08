// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#pragma once

#include "slideio/drivers/ndpi/ndpi_api_def.hpp"
#include <string>

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
    // The NDPI counterpart of slideio::TIFFKeeper: an owning wrapper around a
    // libtiff::TIFF* opened by NDPITiffTools. It exists separately because the NDPI
    // driver links its own patched libtiff and routes messages through
    // installNDPITiffMessageHandlers() rather than installTiffMessageHandlers(). The
    // two classes are deliberately kept in step; see TECH_DEBT.md section 1 problem 6
    // for the open follow-up that would collapse them onto one shared handle.
    //
    // The NDPI libtiff fork's error and warning handlers are installed once, at
    // NDPIImageDriver construction (see installNDPITiffMessageHandlers()); an
    // NDPITIFFKeeper's lifetime no longer touches them.
    class SLIDEIO_NDPI_EXPORTS NDPITIFFKeeper
    {
    public:
        explicit NDPITIFFKeeper(libtiff::TIFF* hFile = nullptr);
        // NDPITiffTools::openTiffFile has no read-write mode, so unlike TIFFKeeper's
        // (filePath, readOnly) constructor this one takes no readOnly flag.
        explicit NDPITIFFKeeper(const std::string& filePath);
        ~NDPITIFFKeeper();

        // An owning handle must not be copied: two owners means two closes, and the
        // second one operates on a pointer libtiff has already freed.
        NDPITIFFKeeper(const NDPITIFFKeeper&)            = delete;
        NDPITIFFKeeper& operator=(const NDPITIFFKeeper&) = delete;
        // After the move, `other` owns nothing: m_hFile is null. A moved-from keeper
        // must not be revived via reset()/openTiffFile() -- it is fit only to be
        // destroyed or move-assigned over.
        NDPITIFFKeeper(NDPITIFFKeeper&& other) noexcept;
        NDPITIFFKeeper& operator=(NDPITIFFKeeper&& other) noexcept;

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
        void openTiffFile(const std::string& filePath);
        void closeTiffFile();

    private:
        libtiff::TIFF* m_hFile = nullptr;
    };
}

#if defined(_MSC_VER)
#pragma warning( pop )
#endif
