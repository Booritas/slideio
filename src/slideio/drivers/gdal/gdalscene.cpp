// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#include "slideio/base/exceptions.hpp"
#include "slideio/drivers/gdal/gdalscene.hpp"
#include "slideio/slideio/slideio.hpp"
#include "slideio/base/resolution.hpp"
#include "slideio/core/tools/cvtools.hpp"
#include "slideio/core/tools/tools.hpp"


slideio::GDALScene::GDALScene(const std::string& path) : m_hFile(nullptr)
{
    m_hFile = openFile(path);
    m_filePath = path;
    init();
}

slideio::GDALScene::GDALScene(GDALDatasetH ds, const std::string& path) : m_hFile(ds), m_filePath(path)
{
    init();
}

slideio::GDALScene::~GDALScene()
{
    closeFile(m_hFile);
    m_hFile = nullptr;
}

std::string slideio::GDALScene::getFilePath() const
{
    return m_filePath;
}


int slideio::GDALScene::getNumChannels() const
{
    if(m_hFile==nullptr) {
        RAISE_RUNTIME_ERROR << "GDALDriver: Invalid file header by channel number query";
    }
    int channels = GDALGetRasterCount(m_hFile);
    return channels;
}

slideio::DataType slideio::GDALScene::getChannelDataType(int channel) const
{
    if(m_hFile==nullptr) {
        RAISE_RUNTIME_ERROR << "GDALDriver: Invalid file header by data type query";
    }
    GDALRasterBandH hBand = GDALGetRasterBand(m_hFile, channel+1);
    if(hBand==nullptr) {
        RAISE_RUNTIME_ERROR << "GDALDriver:  Cannot get raster band by query of data type";
    }
    const GDALDataType dt = GDALGetRasterDataType(hBand);
    return dataTypeFromGDALDataType(dt);
}

slideio::Resolution  slideio::GDALScene::getResolution() const
{
    double adfGeoTransform[6];
    slideio::Resolution res = {0,0};
    if(GDALGetGeoTransform(m_hFile, adfGeoTransform ) == CE_None )
    {
        res.x = adfGeoTransform[1];
        res.y = adfGeoTransform[5];
    }
    return res;
}

double slideio::GDALScene::getMagnification() const
{
    return 0;
}

GDALDatasetH slideio::GDALScene::openFile(const std::string& filePath)
{
    Tools::throwIfPathNotExist(filePath, ":GDALScene::openFile");
    GDALDatasetH hfile = GDALOpen(filePath.c_str(), GA_ReadOnly);
    if(hfile==nullptr){
        RAISE_RUNTIME_ERROR <<"Cannot open file with GDAL driver:" << filePath;
    }
    return hfile;
}

inline void slideio::GDALScene::closeFile(GDALDatasetH hfile)
{
    if(hfile!=nullptr)
        GDALClose(hfile);
}

slideio::DataType slideio::GDALScene::dataTypeFromGDALDataType(GDALDataType dt)
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


std::string slideio::GDALScene::getName() const
{
    return std::string();
}

cv::Rect slideio::GDALScene::getRect() const
{
    if (m_hFile == nullptr) {
        RAISE_RUNTIME_ERROR << "GDALDriver: Invalid file header by scene size query";
    }
    cv::Rect rect;
    rect.x = 0;
    rect.y = 0;
    rect.width = GDALGetRasterXSize(m_hFile);
    rect.height = GDALGetRasterYSize(m_hFile);
    return rect;
}

void slideio::GDALScene::readResampledBlockChannelsEx(const cv::Rect& blockRect, const cv::Size& blockSize,
    const std::vector<int>& componentIndices, int zSliceIndex, int tFrameIndex, cv::OutputArray output)
{
	if (zSliceIndex != 0 || tFrameIndex != 0) {
		RAISE_RUNTIME_ERROR << "GDALDriver: Z-slice and T-frame indices are not supported";
	}
    if(m_hFile==nullptr) {
        RAISE_RUNTIME_ERROR << "GDALDriver: Invalid file header by raster reading operation";
    }
    const int numChannels = GDALGetRasterCount(m_hFile);
    const cv::Size imageSize = { GDALGetRasterXSize(m_hFile),GDALGetRasterYSize(m_hFile) };
    auto channelIndices = componentIndices;
    if(channelIndices.empty())
    {
        channelIndices.resize(numChannels);
        for (int channelIndex = 0; channelIndex < numChannels; ++channelIndex)
        {
            channelIndices[channelIndex] = channelIndex;
        }
    }
    std::vector<cv::Mat> channelRasters;
    channelRasters.reserve(channelIndices.size());
    for (const auto& channelIndex : channelIndices)
    {
        GDALRasterBandH hBand = GDALGetRasterBand(m_hFile, channelIndex + 1);
        if (hBand == nullptr) {
            RAISE_RUNTIME_ERROR << "Cannot open raster band from:" << m_filePath;
        }
        const GDALDataType dt = GDALGetRasterDataType(hBand);
        const DataType dataType = dataTypeFromGDALDataType(dt);
        if(!CVTools::isValidDataType(dataType)) {
            RAISE_RUNTIME_ERROR << "Unknown data type " << dt << "of channel " << channelIndex << " of file " << m_filePath;
        }
        const int cvDt = CVTools::toOpencvType(dataType);
        cv::Mat channelRaster; 
        int channelType = CV_MAKETYPE(cvDt, 1);
        channelRaster.create(blockSize, channelType);
        CPLErr err = GDALRasterIO(hBand, GF_Read,
            blockRect.x, blockRect.y,
            blockRect.width, blockRect.height,
            channelRaster.data,
            blockSize.width, blockSize.height,
            GDALGetRasterDataType(hBand), 0, 0);
        channelRasters.push_back(channelRaster);
        if (err != CE_None) {
            RAISE_RUNTIME_ERROR << "Cannot read raster band " << channelIndex << " from " << m_filePath;
        }
    }
    if(channelRasters.size()>1)
    {
        cv::merge(channelRasters, output);
    }
    else if(channelRasters.size()==1)
    {
        channelRasters[0].copyTo(output);
    }
}

void slideio::GDALScene::init()
{
    auto driver = GDALGetDatasetDriver(m_hFile);
    std::string driverName = GDALGetDriverShortName(driver);

    if(driverName.compare("PNG")==0)
    {
        m_compression = Compression::Png;
    }
    else if(driverName.compare("JPEG")==0)
    {
        m_compression = Compression::Jpeg;
    }
    else if(driverName.compare("GIF")==0)
    {
        m_compression = Compression::GIF;
    }
    else if(driverName.compare("BIGGIF")==0)
    {
        m_compression = Compression::BIGGIF;
    }
    else if(driverName.compare("BMP")==0)
    {
        m_compression = Compression::Uncompressed;
    }
    else if(driverName.compare("JPEG2000")==0)
    {
        m_compression = Compression::Jpeg2000;
    }
}
