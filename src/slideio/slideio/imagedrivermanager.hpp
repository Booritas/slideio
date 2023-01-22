// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#ifndef OPENCV_slideio_imagedrivermanager_HPP
#define OPENCV_slideio_imagedrivermanager_HPP

#include "slideio/slideio/slideio_def.hpp"
#include "slideio/core/cvslide.hpp"
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
    /**@brief Class ImageDriverManager keeps overview over specific image format drivers.
    Allows opening a slide with a specific driver.
    *
    * The class contains a set of static methods that provide access to the image format drivers.
    */
    class SLIDEIO_EXPORTS ImageDriverManager
    {
    protected:
        ImageDriverManager();
        ~ImageDriverManager();
    public:
        /**@brief returns a list of ids of available image format drivers*/
        static std::vector<std::string> getDriverIDs();
        /**@brief opens a slide and returns a smart pointer to object of slideio::CVSlide class.
         *
         * @param filePath : a path to a slide file/folder;
         * @param driver : a driver id which should be used for the opening of the slide.
         *
         * @return object of class slideio::CVSlide which provides access to the slide data.
         * @throw std::runtime_error if driver cannot be found.
         */
        static std::shared_ptr<CVSlide> openSlide(const std::string& filePath, const std::string& driver);
        /**@brief sets logging level.
         *
         * @params level : log level. Should be one of the following strings:
         * 'INFO','WARNING','ERROR','FATAL'(default).
         */
        static void setLogLevel(const std::string& level);
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