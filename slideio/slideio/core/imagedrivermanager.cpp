// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#include "slideio/core/imagedrivermanager.hpp"
#include "slideio/drivers/gdal/gdalimagedriver.hpp"
#include "slideio/drivers/svs/svsimagedriver.hpp"
#include "slideio/drivers/czi/cziimagedriver.hpp"

using namespace cv::slideio;
std::map<std::string, cv::Ptr<ImageDriver>> ImageDriverManager::driverMap;


ImageDriverManager::ImageDriverManager()
{
}

ImageDriverManager::~ImageDriverManager()
{
}

std::vector<std::string> ImageDriverManager::getDriverIDs()
{
    initialize();
    std::vector<std::string> ids;
    for(const auto drv : driverMap){
        ids.push_back(drv.first);
    }
    return ids;
}

void ImageDriverManager::initialize()
{
    if(driverMap.empty())
    {
        {
            GDALImageDriver* driver = new GDALImageDriver;
            cv::Ptr<ImageDriver> gdal(driver);
            driverMap[gdal->getID()] = gdal;
        }
        {
            SVSImageDriver* driver = new SVSImageDriver;
            cv::Ptr<ImageDriver> svs(driver);
            driverMap[svs->getID()] = svs;
        }
        {
            cv::Ptr<ImageDriver> driver(new CZIImageDriver);
            driverMap[driver->getID()] = driver;
        }
    }
}

cv::Ptr<Slide> ImageDriverManager::openSlide(const std::string& filePath, const std::string& driverName)
{
    auto it = driverMap.find(driverName);
    if(it==driverMap.end())
        throw std::runtime_error("ImageDriverManager: Unknown driver " + driverName);
    cv::Ptr<slideio::ImageDriver> driver = it->second;
    return driver->openFile(filePath);
}
