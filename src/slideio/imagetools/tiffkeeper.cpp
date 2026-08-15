// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.

#include "slideio/imagetools/tiffkeeper.hpp"
#include "slideio/imagetools/tiffmessagehandler.hpp"
#include "tifftools.hpp"

using namespace slideio;

TIFFKeeper::TIFFKeeper(libtiff::TIFF* hFile) : m_hFile(hFile)
{
    initMessageHandler();
}

TIFFKeeper::TIFFKeeper(const std::string& filePath, bool readOnly)
{
    initMessageHandler();
    openTiffFile(filePath, readOnly);
}

void TIFFKeeper::initMessageHandler()
{
    m_messageHandler = std::make_shared<TIFFMessageHandler>();
}

TIFFKeeper::TIFFKeeper(TIFFKeeper&& other) noexcept
    : m_hFile(other.m_hFile), m_messageHandler(std::move(other.m_messageHandler))
{
    other.m_hFile = nullptr;
}

TIFFKeeper& TIFFKeeper::operator=(TIFFKeeper&& other) noexcept
{
    if (this != &other) {
        // Close what we own before taking what they own, or ours leaks.
        reset(other.m_hFile);
        other.m_hFile = nullptr;
        // The handler is deliberately NOT moved. It swaps libtiff's process-global
        // error and warning handlers for its lifetime, so destroying this object's
        // handler here would restore the globals while the other keeper -- and its
        // handler -- are still alive, and libtiff messages would stop reaching the log.
        // Both objects already have one; leaving each with its own keeps the refcounts,
        // and the globals, unchanged across the assignment.
    }
    return *this;
}

TIFFKeeper::~TIFFKeeper()
{
    if (m_hFile)
        TiffTools::closeTiffFile(m_hFile);
}

void TIFFKeeper::openTiffFile(const std::string& filePath, bool readOnly)
{
    reset(TiffTools::openTiffFile(filePath, readOnly));
}

void TIFFKeeper::reset(libtiff::TIFF* hFile)
{
    if (m_hFile != hFile) {
        TiffTools::closeTiffFile(m_hFile);
        m_hFile = hFile;
    }
}

libtiff::TIFF* TIFFKeeper::release()
{
    libtiff::TIFF* handle = m_hFile;
    m_hFile = nullptr;
    return handle;
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

void TIFFKeeper::writeRawTile(int x, int y, const uint8_t* data, int size) {
	TiffTools::writeRawTile(m_hFile, x, y, data, size);
}
