// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#include "slideio/imagetools/imagetools.hpp"
#include "slideio/core/tools/cvtools.hpp"

#include "slideio/base/exceptions.hpp"
#include "slideio/imagetools/gdal_lib.hpp"
#include <map>


static slideio::DataType dataTypeFromGDALDataType(GDALDataType dt)
{
    switch(dt)
    {
    case GDT_Unknown:
        return slideio::DataType::DT_Unknown;
    case GDT_Byte:
        return slideio::DataType::DT_Byte;
    case GDT_UInt16:
        return slideio::DataType::DT_UInt16;
    case GDT_Int16:
        return slideio::DataType::DT_Int16;
    case GDT_Int32:
        return slideio::DataType::DT_Int32;
    case GDT_Float32:
        return slideio::DataType::DT_Float32;
    case GDT_Float64:
        return slideio::DataType::DT_Float64;
    default:
        return slideio::DataType::DT_Unknown;
    }

}


void slideio::ImageTools::readGDALSubset(const std::string& filePath, cv::OutputArray output) {
    GDALDatasetH hFile = GDALOpen(filePath.c_str(), GA_ReadOnly);
    try
    {
        if(hFile==nullptr){
            RAISE_RUNTIME_ERROR << "Cannot open file:" << filePath;
        }
        const cv::Size imageSize = {GDALGetRasterXSize(hFile),GDALGetRasterYSize(hFile)};
        const int numChannels = GDALGetRasterCount(hFile);
        std::vector<cv::Mat> channels(numChannels);
        for(int channelIndex=0; channelIndex<numChannels; channelIndex++)
        {
            GDALRasterBandH hBand = GDALGetRasterBand(hFile, channelIndex+1);
            if(hBand==nullptr) {
                RAISE_RUNTIME_ERROR << "Cannot open raster band from:" << filePath;
            }
            const GDALDataType dt = GDALGetRasterDataType(hBand);
            const DataType dataType = dataTypeFromGDALDataType(dt);
            if(CVTools::isValidDataType(dataType)){
                
            }
            const int cvDt = CVTools::toOpencvType(dataType);
            cv::Mat& channel = channels[channelIndex];
            int channelType = CV_MAKETYPE(cvDt, 1);
            channel.create(imageSize, channelType);
            CPLErr err = GDALRasterIO(hBand, GF_Read,
                                      0, 0,
                                      imageSize.width, imageSize.height,
                                      channel.data,
                                      imageSize.width, imageSize.height, 
                                      GDALGetRasterDataType(hBand),0,0);
            if(err!=CE_None){
                RAISE_RUNTIME_ERROR << "Cannot read raster band from:" << filePath;
            }
        }
        cv::merge(channels, output);
        GDALClose(hFile);
    }
    catch(std::exception& exp)
    {
        if(hFile!=nullptr)
            GDALClose(hFile);
        throw exp;
    }
}

void slideio::ImageTools::readGDALImageSubDataset(const std::string& filePath, int subDatasetIndex, cv::OutputArray output)
{
    GDALAllRegister();
    GDALDatasetH hFile = GDALOpen(filePath.c_str(), GA_ReadOnly);
    std::string name = std::string("SUBDATASET_") + std::to_string(subDatasetIndex) + std::string("_NAME");
    if (hFile == nullptr) {
        RAISE_RUNTIME_ERROR << "Cannot open file:" << filePath;
    }
    try {
        char** subDatasets = GDALGetMetadata(hFile, "SUBDATASETS");
        if (subDatasets != nullptr) {
            std::vector<cv::Mat> pages;
            int page_id = 1;
            for (int i = 0; subDatasets[i] != nullptr; i++) {
                std::string subdataset = subDatasets[i];
                std::string::size_type pos = subdataset.find("=");
                if (pos != std::string::npos) {
                    std::string subDatasetName = subdataset.substr(0, pos);
                    std::string subDatasetValue = subdataset.substr(pos + 1);
                    if (subDatasetName == name) {
                        cv::Mat page;
                        readGDALSubset(subDatasetValue, output);
                        break;
                    }
                }
            }
        }
        else if(subDatasetIndex == 1){
            readGDALSubset(filePath, output);
        }
        else {
            RAISE_RUNTIME_ERROR << "Cannot find subdataset:" << subDatasetIndex << " in file:" << filePath;
        }
        GDALClose(hFile);
    }
    catch (const std::exception&) {
        if (hFile != nullptr)
            GDALClose(hFile);
        throw;
    }
}

void slideio::ImageTools::readGDALImage(const std::string& filePath, cv::OutputArray output)
{
    GDALAllRegister();
    GDALDatasetH hFile = GDALOpen(filePath.c_str(), GA_ReadOnly);
    if (hFile == nullptr) {
        RAISE_RUNTIME_ERROR << "Cannot open file:" << filePath;
    }
    try {
        char** subDatasets = GDALGetMetadata(hFile, "SUBDATASETS");
        if (subDatasets != nullptr) {
            std::vector<cv::Mat> pages;
            int page_id = 1;
            for (int i = 0; subDatasets[i] != nullptr; i++)
            {
                std::string subdataset = subDatasets[i];
                std::string::size_type pos = subdataset.find("=");
                if (pos != std::string::npos) {
                    std::string subDatasetName = subdataset.substr(0, pos);
                    std::string subDatasetValue = subdataset.substr(pos + 1);
                    std::string name = std::string("SUBDATASET_") + std::to_string(page_id) + std::string("_NAME");
                    if (subDatasetName == name) {
                        cv::Mat page;
                        readGDALSubset(subDatasetValue, page);
                        pages.push_back(page);
                        page_id++;
                    }
                }
            }
            if (!pages.empty()) {
                cv::merge(pages, output);
            }
            else {
                readGDALSubset(filePath, output);
            }
        }
        else {
            readGDALSubset(filePath, output);
        }
        GDALClose(hFile);
    }
    catch (const std::exception& ) {
        if (hFile != nullptr)
            GDALClose(hFile);
        throw;
    }
}

void slideio::ImageTools::writeRGBImage(const std::string& path, Compression compression, cv::Mat raster)
{
    GDALAllRegister();
    const int numChannels = raster.channels();
    const std::map<Compression, std::string> driverMap = 
    {
        std::make_pair(Compression::Png, "PNG"),
        std::make_pair(Compression::Jpeg, "JPEG"),
        std::make_pair(Compression::Uncompressed, "BMP"),
    };
    auto driverIt = driverMap.find(compression);
    if(driverIt==driverMap.end())
    {
        RAISE_RUNTIME_ERROR << "Not supported compression type for writing of RGB image "
            << path << ": " << static_cast<int>(compression);
    }
    const std::string driverName = driverIt->second;

    const int width = raster.cols;
    const int height = raster.rows;
    if(raster.empty())
    {
        RAISE_RUNTIME_ERROR << "Invalid raster for file " << path;
    }
    if(numChannels!=1 && numChannels!=3)
    {   RAISE_RUNTIME_ERROR << "Error writing of image file " << path 
            << ". Received invalid number of channels: " << numChannels;
    }
    GDALDatasetH dataset = nullptr;
    try
    {
        auto memoryDriver = GDALGetDriverByName("MEM");
        auto imageDriver = GDALGetDriverByName(driverName.c_str());
        dataset = GDALCreate(memoryDriver, "", width, height, numChannels, GDT_Byte, nullptr);
        for(int channelIndex=0; channelIndex<numChannels; ++channelIndex)
        {
            GDALRasterBandH band = GDALGetRasterBand(dataset, channelIndex + 1);
            if(band==nullptr)
            {
                RAISE_RUNTIME_ERROR << "Error writing of image file " << path 
                    << " during processing of channel " << channelIndex << ". Received null band";
            }
            cv::Mat channelRaster;
            cv::extractChannel(raster, channelRaster, channelIndex);
            CPLErr err = GDALRasterIO(band, GF_Write,
                0, 0, width, height,
                channelRaster.data,
                width, height, GDT_Byte,
             0, 0);
            if(err!=CE_None)
            {
                RAISE_RUNTIME_ERROR << "Error writing of image file " << path 
                    << " during processing of channel " << channelIndex << ". GDAL error: " << err;
            }
        }
        GDALDatasetH imageDateaset = GDALCreateCopy(imageDriver, path.c_str(), dataset, FALSE, nullptr, nullptr,
                                                    nullptr);
        if(imageDateaset)
        {
            GDALClose(imageDateaset);
        }

        GDALClose(dataset);
    }
    catch(std::exception& ex)
    {
        if(dataset!=nullptr)
        {
            GDALClose(dataset);
            dataset = nullptr;
        }
        throw ex;
    }
}
static GDALDataType toGdalType(slideio::DataType dt)
{
    switch(dt)
    {
    case slideio::DataType::DT_Byte: return GDT_Byte;
    case slideio::DataType::DT_Int16: return GDT_Int16;
    case slideio::DataType::DT_UInt16: return GDT_UInt16;
    case slideio::DataType::DT_Int32: return GDT_Int32;
    case slideio::DataType::DT_Float32: return GDT_Float32;
    case slideio::DataType::DT_Float64: return GDT_Float64;
    default: {
            RAISE_RUNTIME_ERROR << "toGdalType: Cannot convert type " << (int)dt << " to GDAL supported types";
        }
    }
}

void slideio::ImageTools::writeTiffImage(const std::string& path,cv::Mat raster)
{
    GDALAllRegister();
    const int numChannels = raster.channels();
    const std::string driverName = "GTiff";
    const int cvType = raster.type() & CV_MAT_DEPTH_MASK;
    const DataType dt = CVTools::fromOpencvType(cvType);
    const GDALDataType gdalType = toGdalType(dt);

    const int width = raster.cols;
    const int height = raster.rows;
    if (raster.empty())
    {
        RAISE_RUNTIME_ERROR << "Invalid raster for file " << path;
    }
    GDALDatasetH dataset = nullptr;
    try
    {
        auto memoryDriver = GDALGetDriverByName("MEM");
        auto imageDriver = GDALGetDriverByName(driverName.c_str());
        dataset = GDALCreate(memoryDriver, "", width, height, numChannels, gdalType, nullptr);
        for (int channelIndex = 0; channelIndex < numChannels; ++channelIndex)
        {
            GDALRasterBandH band = GDALGetRasterBand(dataset, channelIndex + 1);
            if (band == nullptr)
            {
                RAISE_RUNTIME_ERROR << "Error writing of image file " << path
                    << " during processing of channel " << channelIndex << ". Received null band";
            }
            cv::Mat channelRaster;
            cv::extractChannel(raster, channelRaster, channelIndex);
            CPLErr err = GDALRasterIO(band, GF_Write,
                0, 0, width, height,
                channelRaster.data,
                width, height, gdalType,
                0, 0);
            if (err != CE_None)
            {
                RAISE_RUNTIME_ERROR << "Error writing of image file " << path
                    << " during processing of channel " << channelIndex << ". GDAL error: " << err;
            }
        }
        GDALDatasetH imageDateaset = GDALCreateCopy(imageDriver, path.c_str(), dataset, FALSE, nullptr, nullptr,
            nullptr);
        if (imageDateaset)
        {
            GDALClose(imageDateaset);
        }

        GDALClose(dataset);
    }
    catch (std::exception& ex)
    {
        if (dataset != nullptr)
        {
            GDALClose(dataset);
            dataset = nullptr;
        }
        throw ex;
    }
}
