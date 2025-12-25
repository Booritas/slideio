// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.

#include "slideio/imagetools/tiffkeeper.hpp"
#include "slideio/imagetools/tiffmessagehandler.hpp"
#include "tifftools.hpp"

using namespace slideio;

TIFFKeeper::TIFFKeeper(libtiff::TIFF* hfile) : m_hFile(hfile)
{
    m_messageHandler = std::make_shared<TIFFMessageHandler>();
}

TIFFKeeper::TIFFKeeper(const std::string& filePath, bool readOnly)
{
    openTiffFile(filePath, readOnly);
}


TIFFKeeper::~TIFFKeeper()
{
    if (m_hFile)
        TiffTools::closeTiffFile(m_hFile);
}

void TIFFKeeper::openTiffFile(const std::string& filePath, bool readOnly)
{
    m_hFile = TiffTools::openTiffFile(filePath, readOnly);
}

void TIFFKeeper::closeTiffFile()
{
    TiffTools::closeTiffFile(m_hFile);
    m_hFile = nullptr;
}

void TIFFKeeper::writeDirectory()
{
    TiffTools::writeDirectory(m_hFile);
}

void TIFFKeeper::setTags(const TiffDirectory& dir)
{
    TiffTools::setTags(m_hFile, dir);
}

void TIFFKeeper::writeTile(int x, int y, Compression compression, const EncodeParameters& params, const cv::Mat& tileRaster,
    uint8_t* buffer, int bufferSize)
{
    TiffTools::writeTile(m_hFile, x, y, compression, tileRaster, params, buffer, bufferSize);
}

void TIFFKeeper::readTile(const slideio::TiffDirectory& dir, int tile, const std::vector<int>& channelIndices,
    cv::OutputArray output)
{
    TiffTools::readTile(m_hFile, dir, tile, channelIndices, output);
}

std::string TIFFKeeper::readStringTag(uint16_t tag)
{
    return TiffTools::readStringTag(m_hFile, tag);
}

void TIFFKeeper::initSubDirs(int numDirs) {
	TiffTools::initSubDirs(m_hFile, numDirs);
}
