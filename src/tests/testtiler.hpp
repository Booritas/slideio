#pragma once
#include "slideio/imagetools/tilecomposer.hpp"

class TestTiler : public slideio::Tiler
{
public:
	TestTiler(int tileWidth, int tileHeight, int tilesX, int tilesY, cv::Scalar black, cv::Scalar white) :
		m_tileWidth(tileWidth),
		m_tileHeight(tileHeight),
		m_tilesX(tilesX),
		m_tilesY(tilesY),
		m_blackColor(black),
		m_whiteColor(white){}
	int getTileCount(void* userData) override;
	bool getTileRect(int tileIndex, cv::Rect& tileRect, void* userData) override;
	bool readTile(int tileIndex, const std::vector<int>& channelIndices, cv::OutputArray tileRaster,
		void* userData) override;
public:
	int m_tileWidth;
	int m_tileHeight;
	int m_tilesX;
	int m_tilesY;
	cv::Scalar m_blackColor;
	cv::Scalar m_whiteColor;
};
