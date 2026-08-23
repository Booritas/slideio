// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#pragma once

#include "slideio/drivers/ndpi/ndpi_api_def.hpp"
#include <memory>
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
    class NDPITIFFMessageHandler;

    // The NDPI counterpart of slideio::TIFFKeeper: an owning wrapper around a
    // libtiff::TIFF* opened by NDPITiffTools. It exists separately because the NDPI
    // driver links its own patched libtiff and routes messages through
    // NDPITIFFMessageHandler rather than TIFFMessageHandler. The two classes are
    // deliberately kept in step; see TECH_DEBT.md section 1 problem 6 for the open
    // follow-up that would collapse them onto one shared handle.
    //
    // Both constructors install an NDPITIFFMessageHandler, which swaps libtiff's
    // PROCESS-GLOBAL error and warning handlers for the keeper's lifetime and restores
    // them on destruction. Move assignment deliberately keeps the destination's own
    // handler rather than taking the source's (see the .cpp), so the one remaining
    // hazard is keepers whose lifetimes overlap out of order: with overlapping,
    // non-LIFO keeper lifetimes, one destructor can restore a handler while another
    // keeper is still alive, after which libtiff messages go to stderr instead of the
    // log. There is no dangling pointer -- both handlers are free functions -- so the
    // consequence is lost log routing, not a crash.
    class SLIDEIO_NDPI_EXPORTS NDPITIFFKeeper
    {
    public:
        explicit NDPITIFFKeeper(libtiff::TIFF* hFile = nullptr);
        // NDPITiffTools::openTiffFile has no read-write mode, so unlike TIFFKeeper's
        // (filePath, readOnly) constructor this one takes no readOnly flag.
        explicit NDPITIFFKeeper(const std::string& filePath);
        ~NDPITIFFKeeper();

        // An owning handle must not be copied: two owners means two closes, and the
        // second one operates on a pointer libtiff has already freed. Copy was already
        // ill-formed before this was written -- but only incidentally, because the
        // unique_ptr member made the implicit copy constructor deleted. Saying it here
        // makes it the contract rather than a side effect of a member's type.
        NDPITIFFKeeper(const NDPITIFFKeeper&)            = delete;
        NDPITIFFKeeper& operator=(const NDPITIFFKeeper&) = delete;
        // After the move, `other` owns nothing: m_hFile is null and m_messageHandler
        // has been transferred away, so `other` holds no message handler either. A
        // moved-from keeper must not be revived via reset()/openTiffFile() -- doing so
        // would hand it a live TIFF handle with no handler installed. It is fit only
        // to be destroyed or move-assigned over.
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
        // Shared initialiser for m_messageHandler, used by both constructors.
        void initMessageHandler();

        libtiff::TIFF* m_hFile = nullptr;
        // unique_ptr, not TIFFKeeper's shared_ptr: the handler is never shared, and
        // move transfers it just as well.
        std::unique_ptr<NDPITIFFMessageHandler> m_messageHandler;
    };
}

#if defined(_MSC_VER)
#pragma warning( pop )
#endif
