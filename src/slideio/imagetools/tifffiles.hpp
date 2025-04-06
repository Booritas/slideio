// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#pragma once
#include "slideio/imagetools/slideio_imagetools_def.hpp"
#include <unordered_map>
#include <memory>
#include <string>

namespace libtiff
{
    struct tiff;
    typedef tiff TIFF;
}

namespace slideio
{

    class SLIDEIO_IMAGETOOLS_EXPORTS TIFFFiles {
    public:
        TIFFFiles() = default;
        TIFFFiles(const TIFFFiles&) = delete;
        TIFFFiles& operator=(const TIFFFiles&) = delete;
        TIFFFiles(TIFFFiles&&) = delete;
        TIFFFiles& operator=(TIFFFiles&&) = delete;
        ~TIFFFiles();
        libtiff::TIFF* getOrOpen(const std::string& filename);
        void close(const std::string& filename);
        void closeAll();
		int getNumberOfOpenFiles() const { return static_cast<int>(m_openFiles.size());}
		int getOpenFileCounter() const { return m_openFileCounter; }
    private:
        std::unordered_map<std::string, std::shared_ptr<libtiff::TIFF>> m_openFiles;
		int m_openFileCounter = 0;
    };

}
