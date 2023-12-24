// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#ifndef OPENCV_slideio_ndpifile_HPP
#define OPENCV_slideio_ndpifile_HPP


#if defined(_MSC_VER)
#pragma warning( push )
#pragma warning(disable: 4251)
#endif
#include <string>

#include "ndpitifftools.hpp"

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
        const NDPITiffDirectory& findZoomDirectory(double zoom, int sceneWidth, int dirBegin, int dirEnd);
    private:
        void scanFile();
    private:
        std::string m_filePath;
        NDPITIFFKeeper m_tiff;
        std::vector<NDPITiffDirectory> m_directories;
    };
}


#if defined(_MSC_VER)
#pragma warning( pop )
#endif

#endif