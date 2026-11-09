// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#include "slideio/drivers/bif/bifsmallscene.hpp"
#include "slideio/drivers/bif/bifreadcontext.hpp"
#include "slideio/core/exceptions.hpp"
#include "slideio/core/levelinfo.hpp"
#include "slideio/core/tools/tools.hpp"
#include <opencv2/imgproc.hpp>

using namespace slideio;

BIFSmallScene::BIFSmallScene(const std::string& filePath, const std::string& name, const TiffDirectory& dir)
    : m_filePath(filePath),
      m_driverId("BIF"),
      m_name(name),
      m_directory(dir),
      m_dataType(dir.dataType),
      m_colorProfile(dir.iccProfile),
      m_contextPool([filePath]() { return std::make_unique<bif::BIFReadContext>(filePath); })
{
    if (m_dataType == DataType::DT_None || m_dataType == DataType::DT_Unknown) {
        m_dataType = dir.bitsPerSample == 16 ? DataType::DT_UInt16 : DataType::DT_Byte;
    }
    m_levels.emplace_back(0, Size(dir.width, dir.height), 1., 0., Size(dir.tileWidth, dir.tileHeight));
}

cv::Rect BIFSmallScene::getRect() const
{
    return { 0, 0, m_directory.width, m_directory.height };
}

void BIFSmallScene::readResampledBlockChannelsEx(const cv::Rect& blockRect, const cv::Size& blockSize,
    const std::vector<int>& channelIndices, int zSliceIndex, int tFrameIndex, cv::OutputArray output)
{
    if (zSliceIndex != 0 || tFrameIndex != 0) {
        RAISE_RUNTIME_ERROR << "BIF: auxiliary images have no z slices or time frames";
    }
    cv::Mat raster;
    {
        auto borrow = m_contextPool.acquire();
        TiffTools::readDirRaster(borrow.as<bif::BIFReadContext>().keeper.getHandle(), m_directory, raster);
    }
    cv::Mat channels;
    if (channelIndices.empty()) {
        channels = raster;
    }
    else {
        Tools::extractChannels(raster, channelIndices, channels);
    }
    readClampedBlock(getRect(), blockRect, blockSize, channelIndices,
        [&](const cv::Rect& rect, const cv::Size& size, cv::OutputArray block) {
            cv::resize(channels(rect), block, size);
        },
        output);
}
