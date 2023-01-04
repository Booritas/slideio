// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.org/license.html.
#pragma once
#include "slideio/slideio/slideio_def.hpp"
#include "slideio/slideio/slide.hpp"
#include <string>
#include <vector>

/*!@mainpage SlideioIO imaging library
* 
* @section intro_sec Overview
* The SlideIO library allows extraction infromation from medical images. It includes raster data as well as metadata.
* The library allows reading whole slides as well as any region of a slide. 
* Large slides can be effectively scaled to a smaller size.
* The library uses internal zoom pyramids of images to make the scaling process as fast as possible. 
* SlideIO supports 2D slides as well as 3D data sets and time series.
* @section ref_section Reference
* SlideIO provides a global function to open a slide:
* @code 
* std::shared_ptr<Slide> slideio::openSlide(const std::string& path, const std::string& driver).
* @endcode
* The function accepts 2 parameters: a path to the slide file or folder and driver id and returns a smart pointer to an object of #slideio::Slide class.
* Driver id is a label assigned to an image driver that processes the slide format.
* List of available driver ids can be obtained from the function 
* @code
* std::vector<std::string> slideio::getDriverIDs()
* @endcode
* Cuttently the follwing drivers are provided:
* - SVS : Aperio SVS image format;
* - AFI : Aperio AFI image format;
* - SCN : Leica SCN image format;
* - CZI : Zeiss CZI image format;
* - ZVI : Zeiss ZVI image format;
* - NDPI : Hamamatsu NDPI image format;
* - DCM : DICOM image format;
* - GDAL : Generic image formats like jpeg, tiff, etc.
*/

/**
* slideio: main Slideio namespace
*/
namespace  slideio
{
    /**Function for opening a slide file.
    The function returns a smart pointer to an object of Slide class.
    @param path: path of the file/folder that contains the slide.
    @param driver: id of image driver
    */
    SLIDEIO_EXPORTS std::shared_ptr<Slide> openSlide(const std::string& path, const std::string& driver);
    /**Returns a list of available driver ids. */
    SLIDEIO_EXPORTS std::vector<std::string> getDriverIDs();
}