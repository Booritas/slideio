// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.

#ifndef OPENCV_slideio_tiffkeeper_HPP
#define OPENCV_slideio_tiffkeeper_HPP
#include <string>
#include "slideio/imagetools/slideio_imagetools_def.hpp"
#include "slideio/imagetools/imagetools.hpp"

namespace libtiff
{
    struct tiff;
    typedef tiff TIFF;
}

namespace slideio
{
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
        void openTiffFile(const std::string& filePath, bool readOnly = true);
        void closeTiffFile();

    private:
        libtiff::TIFF* m_hFile;
    };
}

#endif