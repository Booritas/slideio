// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#include "slideio/core/imagedrivermanager.hpp"
#include "slideio/drivers/afi/afiimagedriver.hpp"
#include "slideio/drivers/gdal/gdalimagedriver.hpp"
#include "slideio/drivers/svs/svsimagedriver.hpp"
#include "slideio/drivers/czi/cziimagedriver.hpp"

using namespace slideio;
std::map<std::string, std::shared_ptr<ImageDriver>> ImageDriverManager::driverMap;


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
            std::shared_ptr<ImageDriver> gdal(driver);
            driverMap[gdal->getID()] = gdal;
        }
        {
            SVSImageDriver* driver = new SVSImageDriver;
            std::shared_ptr<ImageDriver> svs(driver);
            driverMap[svs->getID()] = svs;
        }
        {
            std::shared_ptr<ImageDriver> driver(new CZIImageDriver);
            driverMap[driver->getID()] = driver;
        }
        {
            std::shared_ptr<ImageDriver> driver{ std::make_shared<AFIImageDriver>() };
            driverMap[driver->getID()] = driver;
        }
    }
}

std::shared_ptr<CVSlide> ImageDriverManager::openSlide(const std::string& filePath, const std::string& driverName)
{
    initialize();
    auto it = driverMap.find(driverName);
    if(it==driverMap.end())
        throw std::runtime_error("ImageDriverManager: Unknown driver " + driverName);
    std::shared_ptr<slideio::ImageDriver> driver = it->second;
    return driver->openFile(filePath);
}
