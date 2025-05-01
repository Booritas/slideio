// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#include "slideio/core/imagedrivermanager.hpp"
#include "slideio/core/imagedriver.hpp"
#include "slideio/drivers/afi/afiimagedriver.hpp"
#include "slideio/drivers/czi/cziimagedriver.hpp"
#include "slideio/drivers/dcm/dcmimagedriver.hpp"
#include "slideio/drivers/gdal/gdalimagedriver.hpp"
#include "slideio/drivers/scn/scnimagedriver.hpp"
#include "slideio/drivers/svs/svsimagedriver.hpp"
#include "slideio/drivers/zvi/zviimagedriver.hpp"
#include "slideio/drivers/ndpi/ndpiimagedriver.hpp"
#include "slideio/drivers/ome-tiff/otimagedriver.hpp"

using namespace slideio;
std::map<std::string, std::shared_ptr<ImageDriver>> ImageDriverManager::driverMap;

static void initLogging()
{
    static bool initLog = false;
    if (!initLog) {
        google::InitGoogleLogging("slideio");
        FLAGS_logtostderr = true;
        FLAGS_minloglevel = google::GLOG_FATAL;
        initLog = true;
    }
}

ImageDriverManager::ImageDriverManager()
{
    SLIDEIO_LOG(INFO) << "Create ImageDriverManager";
}

ImageDriverManager::~ImageDriverManager()
{
    SLIDEIO_LOG(INFO) << "Destroy ImageDriverManager";
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
    initLogging();

    if(driverMap.empty())
    {
        SLIDEIO_LOG(INFO) << "Initialization ImageDriverManager";
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
        {
            std::shared_ptr<ImageDriver> driver{ std::make_shared<SCNImageDriver>() };
            driverMap[driver->getID()] = driver;
        }
        {
            std::shared_ptr<ImageDriver> driver{ std::make_shared<DCMImageDriver>() };
            driverMap[driver->getID()] = driver;
        }
        {
            std::shared_ptr<ImageDriver> driver{ std::make_shared<ZVIImageDriver>() };
            driverMap[driver->getID()] = driver;
        }
        {
            std::shared_ptr<ImageDriver> driver{ std::make_shared<NDPIImageDriver>() };
            driverMap[driver->getID()] = driver;
        }
        {
            std::shared_ptr<ImageDriver> driver{ std::make_shared<OTImageDriver>() };
            driverMap[driver->getID()] = driver;
        }
    }
}

std::shared_ptr<CVSlide> ImageDriverManager::openSlide(const std::string& filePath, const std::string& driverName)
{
    static bool initLog = false;
    initialize();
    auto it = driverMap.find(driverName);
    if(it==driverMap.end())
        throw std::runtime_error("ImageDriverManager: Unknown driver " + driverName);
    std::shared_ptr<slideio::ImageDriver> driver = it->second;
    return driver->openFile(filePath);
}

void ImageDriverManager::setLogLevel(const std::string &level) {
    initLogging();
    if(level.compare("INFO")==0) {
        FLAGS_minloglevel = google::GLOG_INFO;
    } else if(level.compare("ERROR")==0) {
        FLAGS_minloglevel = google::GLOG_ERROR;
    } else if(level.compare("WARNING")==0) {
        FLAGS_minloglevel = google::GLOG_WARNING;
    } else if(level.compare("FATAL")==0) {
        FLAGS_minloglevel = google::GLOG_FATAL;
    }
}

