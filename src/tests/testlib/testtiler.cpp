#include <gtest/gtest.h>
#include "testtiler.hpp"

//#include <opencv2/highgui.hpp>

#include "testtools.hpp"
#include "slideio/base/slideio_enums.hpp"
#include "slideio/imagetools/cvtools.hpp"

int TestTiler::getTileCount(void* userData)
{
	return m_tilesX*m_tilesY;
}

bool TestTiler::getTileRect(int tileIndex, cv::Rect& tileRect, void* userData)
{
	int tileY = tileIndex / m_tilesX;
	int tileX = tileIndex % m_tilesX;
	tileRect.x = tileX * m_tileWidth;
	tileRect.y = tileY * m_tileHeight;
	tileRect.width = m_tileWidth;
	tileRect.height = m_tileHeight;
	return true;
}

bool TestTiler::readTile(int tileIndex, const std::vector<int>& channelIndices, cv::OutputArray tileRaster,
	void* userData)
{
	cv::Size tileSize = { m_tileWidth, m_tileHeight };
	initializeBlock(tileSize, channelIndices, tileRaster);
	int tileY = tileIndex / m_tilesX;
	int tileX = tileIndex % m_tilesX;
	bool evenRow = (tileY % 2) == 0;
	bool evenCol = (tileX % 2) == 0;
	cv::Scalar color = m_whiteColor;
	if(evenCol != evenRow)
	{
		color = m_blackColor;
	}
	tileRaster.setTo(color);
	return true;
}

void TestTiler::initializeBlock(const cv::Size& blockSize, const std::vector<int>& channelIndices,
    cv::OutputArray output)
{
	const slideio::DataType dt = slideio::DataType::DT_Byte;
	int numChannels = static_cast<int>(channelIndices.size());
	if (numChannels == 0) {
		numChannels = 3;
	}
	output.create(blockSize, CV_MAKETYPE(slideio::CVTools::toOpencvType(dt), numChannels));
	output.setTo(cv::Scalar(255,0,0));
}

