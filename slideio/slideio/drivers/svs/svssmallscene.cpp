// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#include "slideio/drivers/svs/svssmallscene.hpp"
#include "slideio/drivers/svs/svstools.hpp"
#include "slideio/slideio.hpp"
#include "slideio/imagetools/tifftools.hpp"

#include <opencv2/core.hpp>
#include <opencv2/imgproc.hpp>
#include <boost/format.hpp>

using namespace cv::slideio;

SVSSmallScene::SVSSmallScene(const std::string& filePath,
    const std::string& name,
    const TiffDirectory& dir,
    TIFF* hfile):
        SVSScene(filePath, name),
        m_directory(dir),
        m_dataType(DataType::DT_Unknown),
        m_hFile(hfile)
{
    m_dataType = m_directory.dataType;

    if(m_dataType==DataType::DT_None || m_dataType==DataType::DT_Unknown)
    {
        switch(dir.bitsPerSample)
        {
            case 8:
                m_dataType = m_directory.dataType = DataType::DT_Byte;
            break;
            case 16:
                m_dataType = m_directory.dataType = DataType::DT_UInt16;
            break;
            default:
                m_dataType = DataType::DT_Unknown;
        }
    }
    m_magnification = SVSTools::extractMagnifiation(dir.description);
}


cv::Rect SVSSmallScene::getRect() const
{
    cv::Rect rect = { 0,0, m_directory.width, m_directory.height };
    return rect;
}

int SVSSmallScene::getNumChannels() const
{
    return m_directory.channels;
}

DataType SVSSmallScene::getChannelDataType(int ) const
{
    return m_dataType;
}

Resolution SVSSmallScene::getResolution() const
{
    return m_directory.res;
}

double SVSSmallScene::getMagnification() const
{
    return m_magnification;
}


void SVSSmallScene::readResampledBlockChannels(const cv::Rect& blockRect, const cv::Size& blockSize,
    const std::vector<int>& channelIndices, cv::OutputArray output)
{
    if (m_hFile == nullptr)
        throw std::runtime_error("SVSDriver: Invalid file header by raster reading operation");

    cv::Mat wholeDirRaster;
    if(channelIndices.empty())
    {
        TiffTools::readStripedDir(m_hFile, m_directory, wholeDirRaster);
    }
    else
    {
        cv::Mat dirRaster;
        TiffTools::readStripedDir(m_hFile, m_directory, dirRaster);
        if(channelIndices.size()==1)
        {
            cv::extractChannel(dirRaster, wholeDirRaster, channelIndices[0]);
        }
        else
        {
            std::vector<cv::Mat> channelRasters;
            channelRasters.reserve(channelIndices.size());
            for (const auto& channelIndex : channelIndices)
            {
                cv::Mat channelRaster;
                cv::extractChannel(dirRaster, channelRaster, channelIndex);
                channelRasters.push_back(channelRaster);
            }
            cv::merge(channelRasters, wholeDirRaster);
        }
    }
    cv::Mat blockRaster = wholeDirRaster(blockRect);
    cv::resize(blockRaster, output, blockSize);
}
