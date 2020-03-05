// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#ifndef OPENCV_slideio_imagedrivermanager_HPP
#define OPENCV_slideio_imagedrivermanager_HPP

#include "slideio/slideio_def.hpp"
#include "slideio/core/CVSlide.hpp"
#include <opencv2/core.hpp>
#include <map>
#include <vector>

#if defined(_MSC_VER)
#pragma warning( push )
#pragma warning(disable: 4251)
#endif

namespace slideio
{
    class ImageDriver;
    class SLIDEIO_EXPORTS ImageDriverManager
    {
    protected:
        ImageDriverManager();
        ~ImageDriverManager();
    public:
        static std::vector<std::string> getDriverIDs();
        static ImageDriver* getDriver(const std::string& driverName);
        static ImageDriver* findDriver(const std::string& filePath);
        static std::shared_ptr<CVSlide> openSlide(const std::string& cs, const std::string& driver);
    protected:
        static void initialize();
    private:
        static std::map<std::string, std::shared_ptr<ImageDriver>> driverMap;
    };
}

#if defined(_MSC_VER)
#pragma warning( pop )
#endif

#endif