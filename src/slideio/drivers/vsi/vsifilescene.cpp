// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.

#include "vsifilescene.hpp"

using namespace slideio;

 VsiFileScene::VsiFileScene(const std::string& filePath, std::shared_ptr<vsi::VSIFile>& vsiFile, int directoryIndex) :
     VSIScene(filePath, vsiFile), m_directoryIndex(directoryIndex)
 {
         init();
 }

void VsiFileScene::readResampledBlockChannels(const cv::Rect& blockRect, const cv::Size& blockSize,
    const std::vector<int>& channelIndices, cv::OutputArray output)
{
    
}

int VsiFileScene::getTileCount(void* userData)
{
    return 0;
}

bool VsiFileScene::getTileRect(int tileIndex, cv::Rect& tileRect, void* userData)
{
	return false;
}

bool VsiFileScene::readTile(int tileIndex, const std::vector<int>& channelIndices, cv::OutputArray tileRaster,
    void* userData)
{
    return false;
}

void VsiFileScene::init()
{
}
