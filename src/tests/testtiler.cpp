#include <gtest/gtest.h>
#include "testtiler.hpp"

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
	tileRaster.create(tileSize, CV_MAKETYPE(CV_8U, m_whiteColor.channels));
	int tileY = tileIndex / m_tilesX;
	int tileX = tileIndex % m_tilesX;
	bool evenRow = (tileY % 2) == 0;
	bool evenCol = (tileX % 2) == 0;
	cv::Scalar color = m_whiteColor;
	if(!evenCol != !evenRow)
	{
		color = m_blackColor;
	}
	cv::Mat tileMat = tileRaster.getMat();
	tileMat.setTo(color);
	return true;
}

