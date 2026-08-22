// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.org/license.html.
#pragma once
#include "slideio/slideio/slideio_def.hpp"
#include "slideio/slideio/slide.hpp"
#include <string>
#include <vector>

/*!@mainpage SlideIO imaging library
* 
* @section intro_sec Overview
* The SlideIO library extracts information from medical and microscopy images: raster data as well as metadata.
* It reads whole slides as well as any region of a slide.
* Large slides can be effectively scaled to a smaller size.
* The library uses the internal zoom pyramids of the images to make the scaling process as fast as possible.
* A region can also be read from a pyramid level named by the caller, in the coordinate system of that level,
* which avoids the coordinate conversion and the level selection that the library performs otherwise.
* SlideIO supports 2D slides as well as 3D data sets and time series.
* Metadata is available both as the raw string embedded in the file and as a navigable tree.
* SlideIO exposes 2 c++ interfaces: opencv based interface and generic c++ interface.
* The library works with multiple image formats. Each format is implemented with a 'driver'.
* A driver can be accessed by id - unique human readable string assigned to the driver instance.
* The id can be omitted, in which case the library selects the driver by the content of the file.
* Currently the following drivers are provided:
* - SVS : Aperio SVS image format;
* - AFI : Aperio AFI image format;
* - SCN : Leica SCN image format;
* - CZI : Zeiss CZI image format;
* - ZVI : Zeiss ZVI image format;
* - NDPI : Hamamatsu NDPI image format;
* - VSI : Olympus VSI image format;
* - DCM : DICOM image format, including whole slide images;
* - QPTIFF : PerkinElmer Vectra QPTIFF image format;
* - OMETIFF : OME-TIFF image format;
* - PHTIFF : Philips TIFF image format;
* - GDAL : Generic image formats like jpeg, tiff, etc.
* @section ref_section Generic c++ Interface
* SlideIO generic c++ interface provides the global functions slideio::openSlide(), slideio::getDriverIDs(),
* slideio::setLogLevel(), slideio::getVersion() and 2 classes: slideio::Slide and slideio::Scene.
* Function slideio::openSlide() opens a slide and returns object of class slideio::Slide. The class slideio::Slide exposes methods
* for accessing of the slide properties including metadata and images. A single instance of slideio::Slide can contain multiple
* raster images that are represented by slideio::Scene class. Class slideio::Scene exposes methods for working with a single raster
* image of a slide. The class provides method for accessing to raster data and metadata.
* @code
    std::shared_ptr<slideio::Slide> slide = slideio::openSlide(path, "GDAL");
    const int numScenes = slide->getNumScenes();
    if(numScenes>0) {    
        std::shared_ptr<slideio::Scene> scene = slide->getScene(0);
        const std::tuple<int, int, int, int> sceneRect = scene->getRect();
        const int sceneWidth = std::get<2>(sceneRect);
        const int sceneHeight = std::get<3>(sceneRect);
        const int blockWidth = sceneWidth / 2;
        const int blockHeight = sceneHeight / 2;
        const int blockOriginX = sceneWidth / 4;
        const int blockOriginY = sceneHeight / 4;
        std::tuple<int, int, int, int> blockRect(blockOriginX, blockOriginY, blockWidth, blockHeight);
        const std::tuple<int, int> blockSize(blockWidth, blockHeight);
        const int numChannels = scene->getNumChannels();
        const int memSize = scene->getBlockSize(blockSize, 0, numChannels, 1, 1);
        std::vector<uint8_t> rasterData(memSize);
        scene->readBlock(blockRect, rasterData.data(), rasterData.size());
    }
* @endcode
* @section ref_opencv_api OpenCV Interface
* SlideIO OpenCV interface exposes methods for extraction information from medical slides and intensively uses classes of the OpenCV library.
* The interface exposes the following classes:
* - slideio::ImageDriverManager - implements access to image format specific drivers;
* - slideio::CVSlide - slide class that provides access to the slide data;
* - slideio::CVScene - scene class that provides access to images contained in the slides.
* @code
    std::shared_ptr<slideio::CVSlide> slide = slideio::ImageDriverManager::openSlide(path, "GDAL");
    const int numScenes = slide->getNumScenes();
    std::shared_ptr<slideio::CVScene> scene = slide->getScene(0);
    const cv::Rect sceneRect = scene->getRect();
    const int sceneWidth = sceneRect.width;
    const int sceneHeight = sceneRect.height;
    const int blockWidth = sceneWidth / 2;
    const int blockHeight = sceneHeight / 2;
    const int blockOriginX = sceneWidth / 4;
    const int blockOriginY = sceneHeight / 4;
    cv::Rect blockRect(blockOriginX, blockOriginY, blockWidth, blockHeight);
    const std::tuple<int, int> blockSize(blockWidth, blockHeight);
    const int numChannels = scene->getNumChannels();
    cv::Mat raster;
    scene->readBlock(blockRect, raster);
* @endcode 
*/

/**
* slideio: main Slideio namespace
*/
namespace  slideio
{
    /**Function for opening a slide file.
    @brief The function returns a smart pointer to an object of Slide class.
    @param path : path of the file/folder that contains the slide.
    @param driver : id of image driver, as returned by getDriverIDs(). An empty string or
    "AUTO" selects the driver by the content of the file. The function throws an exception
    if the id is unknown or if no driver accepts the file.
    */
    SLIDEIO_EXPORTS std::shared_ptr<Slide> openSlide(const std::string& path, const std::string& driver= "");
    /**@brief Returns a list of available driver ids. */
    SLIDEIO_EXPORTS std::vector<std::string> getDriverIDs();
    /**@brief Sets the log level for the library.*/
    SLIDEIO_EXPORTS void setLogLevel(const std::string& level);
    /**@brief returns version of the library.*/
    SLIDEIO_EXPORTS std::string getVersion();

}