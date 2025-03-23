// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#include "slideio/drivers/pke/pkesmallscene.hpp"
#include "slideio/drivers/pke/pketools.hpp"
#include "slideio/slideio/slideio.hpp"
#include "slideio/imagetools/tifftools.hpp"

#include <opencv2/core.hpp>
#include <opencv2/imgproc.hpp>

using namespace slideio;

PKESmallScene::PKESmallScene(const std::string& filePath,
    const std::string& name,
    const TiffDirectory& dir,
    bool auxiliary):
        PKEScene(filePath, name),
        m_directory(dir)
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
    if(!auxiliary)
    {
        //m_magnification = PKETools::extractMagnifiation(dir.description);
        //double res = PKETools::extractResolution(dir.description);
        //m_resolution = { res, res };
    }
    m_compression = m_directory.slideioCompression;
    m_levelInfo.setMagnification(0.);
    m_levelInfo.setScale(1.);
    m_levelInfo.setTileSize({ m_directory.tileWidth, m_directory.tileHeight});
    m_levelInfo.setSize({ m_directory.width, m_directory.height });
    m_levelInfo.setLevel(0);
}


cv::Rect PKESmallScene::getRect() const
{
    cv::Rect rect = { 0,0, m_directory.width, m_directory.height };
    return rect;
}

int PKESmallScene::getNumChannels() const
{
    return m_directory.channels;
}

void PKESmallScene::readResampledBlockChannelsEx(const cv::Rect& blockRect, const cv::Size& blockSize,
    const std::vector<int>& channelIndices, int zSliceIndex, int tFrameIndex, cv::OutputArray output)
{
	if (zSliceIndex != 0 || tFrameIndex != 0) {
		RAISE_RUNTIME_ERROR << "PKESmallScene: 3D and 4D images are not supported";
	}
    auto hFile = getFileHandle();

    if (hFile == nullptr) {
        RAISE_RUNTIME_ERROR << "PKEDriver: Invalid file header by raster reading operation";
    }

    cv::Mat wholeDirRaster;
    if(channelIndices.empty())
    {
        TiffTools::readStripedDir(hFile, m_directory, wholeDirRaster);
    }
    else
    {
        cv::Mat dirRaster;
        TiffTools::readStripedDir(hFile, m_directory, dirRaster);
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
