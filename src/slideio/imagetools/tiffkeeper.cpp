// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.

#include "slideio/imagetools/tiffkeeper.hpp"

#include "tifftools.hpp"

using namespace slideio;

TIFFKeeper::TIFFKeeper(libtiff::TIFF* hfile) : m_hFile(hfile)
{
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

void TIFFKeeper::setTags(const TiffDirectory& dir, bool newDirectory)
{
    TiffTools::setTags(m_hFile, dir, newDirectory);
}

void TIFFKeeper::writeTile(int x, int y, Compression compression, int quality, const cv::Mat& tileRaster)
{
    TiffTools::writeTile(m_hFile, x, y, compression, quality, tileRaster);
}

void TIFFKeeper::readTile(const slideio::TiffDirectory& dir, int tile, const std::vector<int>& channelIndices,
    cv::OutputArray output)
{
    TiffTools::readTile(m_hFile, dir, tile, channelIndices, output);
}
