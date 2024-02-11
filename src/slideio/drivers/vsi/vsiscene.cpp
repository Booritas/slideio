// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#include "slideio/drivers/vsi/vsiscene.hpp"
#include "slideio/drivers/vsi/vsifile.hpp"


using namespace slideio;
using namespace slideio::vsi;

VSIScene::VSIScene(const std::string& filePath, std::shared_ptr<vsi::VSIFile>& vsiFile):
    m_filePath(filePath),
    m_compression(slideio::Compression::Unknown),
    m_resolution(0., 0.),
    m_vsiFile(vsiFile),
    m_magnification(0.)
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

std::string VSIScene::getChannelName(int channel) const
{
    return m_channelNames[channel];
}

void VSIScene::initializeBlock(const cv::Size& blockSize, const std::vector<int>& channelIndices,
    cv::OutputArray output)
{
    initializeSceneBlock(blockSize, channelIndices, output);
}

