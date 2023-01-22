// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#include "slideio/imagetools/imagetools.hpp"
#include "slideio/imagetools/cvtools.hpp"
#include <boost/format.hpp>
#include <boost/filesystem.hpp>
#include "slideio/imagetools/gdal_lib.hpp"


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


void slideio::ImageTools::readGDALImage(const std::string& filePath, cv::OutputArray output)
{
    //
    // read extracted page by GDAL library
    // 
    GDALAllRegister();
    GDALDatasetH hFile = GDALOpen(filePath.c_str(), GA_ReadOnly);
    try
    {
        if(hFile==nullptr)
            throw std::runtime_error(
                (boost::format("Cannot open file: %1% with GDAL") % filePath).str());
        const cv::Size imageSize = {GDALGetRasterXSize(hFile),GDALGetRasterYSize(hFile)};
        const int numChannels = GDALGetRasterCount(hFile);
        std::vector<cv::Mat> channels(numChannels);
        for(int channelIndex=0; channelIndex<numChannels; channelIndex++)
        {
            GDALRasterBandH hBand = GDALGetRasterBand(hFile, channelIndex+1);
            if(hBand==nullptr)
                throw std::runtime_error(
                    (boost::format("Cannot open raster band from: %1%") % filePath).str());
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
            if(err!=CE_None)
                throw std::runtime_error(
                    (boost::format("Cannot read raster band from: %1%") % filePath).str());
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
        throw std::runtime_error(
            (boost::format("Not supported compression type for writing of RGB image %1%: %2%")
                %path
                %static_cast<int>(compression)).str()
        );
    }
    const std::string driverName = driverIt->second;

    const int width = raster.cols;
    const int height = raster.rows;
    if(raster.empty())
    {
        throw std::runtime_error(
            (boost::format("Invalid raster for file %1%") %path).str()
        );
    }
    if(numChannels!=1 && numChannels!=3)
    {
        throw std::runtime_error(
            (boost::format("Error writing of image file %1%. Received invalid number of channels: %2%") %path %numChannels).str()
        );
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
                throw std::runtime_error(
                    (boost::format("Error writing of image file %1% during processing of channel %2%. Received null band")
                    %path %channelIndex).str()
                );
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
                throw std::runtime_error(
                    (boost::format("Error writing of image file %1% during processing of channel %2%. GDAL error: %3%")
                        %path %channelIndex %err).str()
                );
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
    default: ;
        throw std::runtime_error(
            (boost::format("toGdalType: Cannot convert type %1% to GDAL supported types") % (int)dt).str());
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
        throw std::runtime_error(
            (boost::format("Invalid raster for file %1%") % path).str()
        );
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
                throw std::runtime_error(
                    (boost::format("Error writing of image file %1% during processing of channel %2%. Received null band")
                        % path % channelIndex).str()
                );
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
                throw std::runtime_error(
                    (boost::format("Error writing of image file %1% during processing of channel %2%. GDAL error: %3%")
                        % path % channelIndex % err).str()
                );
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
