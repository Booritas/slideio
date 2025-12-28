// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#include "slideio/imagetools/imagetools.hpp"
#include "slideio/core/tools/cvtools.hpp"
#include "slideio/base/exceptions.hpp"
#include "slideio/imagetools/fiwrapper.hpp"



void slideio::ImageTools::readGDALImageSubDataset(const std::string& filePath, int pageIndex, cv::OutputArray output)
{
    FIWrapper wrapper(filePath);
    if (!wrapper.isValid()) {
        RAISE_RUNTIME_ERROR << "Cannot open file: " << filePath;
	}
    const int numPages = wrapper.getNumPages();
    if (pageIndex >= numPages || pageIndex <0) {
        RAISE_RUNTIME_ERROR << "Invalid subdataset index " << pageIndex
			<< " for file " << filePath << ". Number of subdatasets: " << numPages;
    }
    std::shared_ptr<FIWrapper::Page> page = wrapper.readPage(pageIndex);
	page->readRaster(output);
}

void slideio::ImageTools::readGDALImage(const std::string& filePath, cv::OutputArray output)
{
    readGDALImageSubDataset(filePath, 0, output);
}

void slideio::ImageTools::writeRGBImage(const std::string& path, Compression compression, cv::Mat raster)
{
    // GDALAllRegister();
    // const int numChannels = raster.channels();
    // const std::map<Compression, std::string> driverMap = 
    // {
    //     std::make_pair(Compression::Png, "PNG"),
    //     std::make_pair(Compression::Jpeg, "JPEG"),
    //     std::make_pair(Compression::Uncompressed, "BMP"),
    // };
    // auto driverIt = driverMap.find(compression);
    // if(driverIt==driverMap.end())
    // {
    //     RAISE_RUNTIME_ERROR << "Not supported compression type for writing of RGB image "
    //         << path << ": " << static_cast<int>(compression);
    // }
    // const std::string driverName = driverIt->second;
    //
    // const int width = raster.cols;
    // const int height = raster.rows;
    // if(raster.empty())
    // {
    //     RAISE_RUNTIME_ERROR << "Invalid raster for file " << path;
    // }
    // if(numChannels!=1 && numChannels!=3)
    // {   RAISE_RUNTIME_ERROR << "Error writing of image file " << path 
    //         << ". Received invalid number of channels: " << numChannels;
    // }
    // GDALDatasetH dataset = nullptr;
    // try
    // {
    //     auto memoryDriver = GDALGetDriverByName("MEM");
    //     auto imageDriver = GDALGetDriverByName(driverName.c_str());
    //     dataset = GDALCreate(memoryDriver, "", width, height, numChannels, GDT_Byte, nullptr);
    //     for(int channelIndex=0; channelIndex<numChannels; ++channelIndex)
    //     {
    //         GDALRasterBandH band = GDALGetRasterBand(dataset, channelIndex + 1);
    //         if(band==nullptr)
    //         {
    //             RAISE_RUNTIME_ERROR << "Error writing of image file " << path 
    //                 << " during processing of channel " << channelIndex << ". Received null band";
    //         }
    //         cv::Mat channelRaster;
    //         cv::extractChannel(raster, channelRaster, channelIndex);
    //         CPLErr err = GDALRasterIO(band, GF_Write,
    //             0, 0, width, height,
    //             channelRaster.data,
    //             width, height, GDT_Byte,
    //          0, 0);
    //         if(err!=CE_None)
    //         {
    //             RAISE_RUNTIME_ERROR << "Error writing of image file " << path 
    //                 << " during processing of channel " << channelIndex << ". GDAL error: " << err;
    //         }
    //     }
    //     GDALDatasetH imageDateaset = GDALCreateCopy(imageDriver, path.c_str(), dataset, FALSE, nullptr, nullptr,
    //                                                 nullptr);
    //     if(imageDateaset)
    //     {
    //         GDALClose(imageDateaset);
    //     }
    //
    //     GDALClose(dataset);
    // }
    // catch(std::exception& ex)
    // {
    //     if(dataset!=nullptr)
    //     {
    //         GDALClose(dataset);
    //         dataset = nullptr;
    //     }
    //     throw ex;
    // }
}

void slideio::ImageTools::writeTiffImage(const std::string& path,cv::Mat raster)
{
    // GDALAllRegister();
    // const int numChannels = raster.channels();
    // const std::string driverName = "GTiff";
    // const int cvType = raster.type() & CV_MAT_DEPTH_MASK;
    // const DataType dt = CVTools::fromOpencvType(cvType);
    // const GDALDataType gdalType = toGdalType(dt);
    //
    // const int width = raster.cols;
    // const int height = raster.rows;
    // if (raster.empty())
    // {
    //     RAISE_RUNTIME_ERROR << "Invalid raster for file " << path;
    // }
    // GDALDatasetH dataset = nullptr;
    // try
    // {
    //     auto memoryDriver = GDALGetDriverByName("MEM");
    //     auto imageDriver = GDALGetDriverByName(driverName.c_str());
    //     dataset = GDALCreate(memoryDriver, "", width, height, numChannels, gdalType, nullptr);
    //     for (int channelIndex = 0; channelIndex < numChannels; ++channelIndex)
    //     {
    //         GDALRasterBandH band = GDALGetRasterBand(dataset, channelIndex + 1);
    //         if (band == nullptr)
    //         {
    //             RAISE_RUNTIME_ERROR << "Error writing of image file " << path
    //                 << " during processing of channel " << channelIndex << ". Received null band";
    //         }
    //         cv::Mat channelRaster;
    //         cv::extractChannel(raster, channelRaster, channelIndex);
    //         CPLErr err = GDALRasterIO(band, GF_Write,
    //             0, 0, width, height,
    //             channelRaster.data,
    //             width, height, gdalType,
    //             0, 0);
    //         if (err != CE_None)
    //         {
    //             RAISE_RUNTIME_ERROR << "Error writing of image file " << path
    //                 << " during processing of channel " << channelIndex << ". GDAL error: " << err;
    //         }
    //     }
    //     GDALDatasetH imageDateaset = GDALCreateCopy(imageDriver, path.c_str(), dataset, FALSE, nullptr, nullptr,
    //         nullptr);
    //     if (imageDateaset)
    //     {
    //         GDALClose(imageDateaset);
    //     }
    //
    //     GDALClose(dataset);
    // }
    // catch (std::exception& ex)
    // {
    //     if (dataset != nullptr)
    //     {
    //         GDALClose(dataset);
    //         dataset = nullptr;
    //     }
    //     throw ex;
    // }
}
