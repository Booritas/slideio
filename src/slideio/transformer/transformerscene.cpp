// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#include "transformerscene.hpp"

using namespace slideio;

TransformerScene::TransformerScene(std::shared_ptr<CVScene> originScene) : m_originScene(originScene)
{
   
}

std::string TransformerScene::getFilePath() const
{
	return m_originScene->getFilePath();
}

std::string TransformerScene::getName() const
{
    return m_originScene->getName();
}

cv::Rect TransformerScene::getRect() const
{
    return m_originScene->getRect();
}

int TransformerScene::getNumChannels() const
{
    return m_originScene->getNumChannels();
}

slideio::DataType TransformerScene::getChannelDataType(int channel) const
{
    return m_originScene->getChannelDataType(channel);
}

Resolution TransformerScene::getResolution() const
{
    return m_originScene->getResolution();
}

double TransformerScene::getMagnification() const
{
    return m_originScene->getMagnification();
}

Compression TransformerScene::getCompression() const
{
    return m_originScene->getCompression();
}

int TransformerScene::getNumZSlices() const
{
    return m_originScene->getNumZSlices();
}

int TransformerScene::getNumTFrames() const
{
    return m_originScene->getNumTFrames();
}

std::string TransformerScene::getChannelName(int channel) const
{
    return m_originScene->getChannelName(channel);
}

double TransformerScene::getZSliceResolution() const
{
    return m_originScene->getZSliceResolution();
}

double TransformerScene::getTFrameResolution() const
{
    return m_originScene->getTFrameResolution();
}

std::string TransformerScene::getRawMetadata() const
{
    return m_originScene->getRawMetadata();
}
