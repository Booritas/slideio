// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#ifndef OPENCV_slideio_ndpifile_HPP
#define OPENCV_slideio_ndpifile_HPP


#if defined(_MSC_VER)
#pragma warning( push )
#pragma warning(disable: 4251)
#endif
#include <memory>
#include <string>

#include "ndpitifftools.hpp"
#include "slideio/base/randomaccessstream.hpp"

namespace libtiff
{
    struct tiff;
    typedef tiff TIFF;
}

namespace slideio
{
    class SLIDEIO_NDPI_EXPORTS NDPIFile
    {
    public:
        NDPIFile(){
        }
        ~NDPIFile();
        void init(const std::string& filePath);
        // Stream-based initialization (I3): opens the NDPI libtiff handle from
        // the stream and routes the raw JPEG/MCU read paths through it too.
        void init(std::shared_ptr<RandomAccessStream> stream);
        const std::vector<NDPITiffDirectory>& directories() const {
            return m_directories;
        }
        const std::string getFilePath() const  {
            return m_filePath;
        }
        libtiff::TIFF* getTiffHandle()
        {
            return m_tiff;
        }
        // Stream (may be null for local-path opens).
        const std::shared_ptr<RandomAccessStream>& stream() const {
            return m_stream;
        }
        // Creates a data source for the raw JPEG/MCU read paths: stream-backed
        // when this file was opened from a stream, otherwise a local FILE*.
        // The returned source's FILE* (path mode) must be released by the caller.
        NDPIDataSource openDataSource() const;
        const NDPITiffDirectory& findZoomDirectory(double zoom, int sceneWidth, int dirBegin, int dirEnd);
    private:
        void scanFile();
        void scanAndReadHeaders();
    private:
        // m_stream is declared before m_tiff so it outlives every TIFF* and
        // every NDPIDataSource opened from it (destruction is reverse order).
        std::shared_ptr<RandomAccessStream> m_stream;
        std::string m_filePath;
        NDPITIFFKeeper m_tiff;
        std::vector<NDPITiffDirectory> m_directories;
    };
}


#if defined(_MSC_VER)
#pragma warning( pop )
#endif

#endif