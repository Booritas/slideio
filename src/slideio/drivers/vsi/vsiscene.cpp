// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#include "slideio/drivers/vsi/vsiscene.hpp"
#include "slideio/imagetools/imagetools.hpp"


using namespace slideio;

VSIScene::VSIScene(const std::string& filePath):
    m_filePath(filePath),
    m_compression(Compression::Unknown),
    m_resolution(0., 0.),
    m_magnification(0.)
{
    init();
}

VSIScene::~VSIScene()
{
}

cv::Rect VSIScene::getRect() const
{
    return m_rect;
}

int VSIScene::getNumChannels() const
{
    return m_numChannels;
}

void VSIScene::readResampledBlockChannels(const cv::Rect& blockRect, const cv::Size& blockSize,
                                          const std::vector<int>& channelIndicesIn, cv::OutputArray output)
{
}

std::string VSIScene::getChannelName(int channel) const
{
    return m_channelNames[channel];
}


void VSIScene::init()
{
}



int VSIScene::getTileCount(void* userData)
{
    return 0;
}

bool VSIScene::getTileRect(int tileIndex, cv::Rect& tileRect, void* userData)
{
    return false;
}

bool VSIScene::readTile(int tileIndex, const std::vector<int>& channelIndices, cv::OutputArray tileRaster,
    void* userData)
{
    return false;
}

void VSIScene::initializeBlock(const cv::Size& blockSize, const std::vector<int>& channelIndices,
    cv::OutputArray output)
{
    initializeSceneBlock(blockSize, channelIndices, output);
}

