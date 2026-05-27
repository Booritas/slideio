// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#pragma once
#include "slideio/imagetools/slideio_imagetools_def.hpp"
#include "slideio/base/randomaccessstream.hpp"
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
        // Associate the originating stream and its URI (the "main" file) so that a
        // getOrOpen() request for that URI is satisfied by opening libtiff from the
        // stream instead of from a local path. Requests for any OTHER name while a
        // stream is set are rejected (multi-file OME-TIFF over a remote stream is a
        // documented v1 limitation). Held here so the stream outlives every TIFF*.
        void setMainStream(const std::string& mainUri, std::shared_ptr<RandomAccessStream> stream) {
            m_mainUri = mainUri;
            m_mainStream = std::move(stream);
        }
        bool hasStream() const { return static_cast<bool>(m_mainStream); }
    private:
        std::unordered_map<std::string, std::shared_ptr<libtiff::TIFF>> m_openFiles;
		int m_openFileCounter = 0;
        std::string m_mainUri;
        std::shared_ptr<RandomAccessStream> m_mainStream;
    };

}
