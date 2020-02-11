// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#include "slideio/imagetools/imagetools.hpp"
#include "slideio/slideio.hpp"

#include <boost/format.hpp>
#include <boost/filesystem.hpp>
#include <gdal/gdal.h>


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
            if(isValidDataType(dataType)){
                
            }
            const int cvDt = toOpencvType(dataType);
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
