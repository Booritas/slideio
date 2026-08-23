// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.

#include "slideio/drivers/ndpi/ndpitiffkeeper.hpp"
#include "slideio/drivers/ndpi/ndpitiffmessagehandler.hpp"
#include "slideio/drivers/ndpi/ndpitifftools.hpp"

using namespace slideio;

NDPITIFFKeeper::NDPITIFFKeeper(libtiff::TIFF* hFile) : m_hFile(hFile)
{
    initMessageHandler();
}

NDPITIFFKeeper::NDPITIFFKeeper(const std::string& filePath)
{
    initMessageHandler();
    openTiffFile(filePath);
}

void NDPITIFFKeeper::initMessageHandler()
{
    m_messageHandler = std::make_unique<NDPITIFFMessageHandler>();
}

NDPITIFFKeeper::NDPITIFFKeeper(NDPITIFFKeeper&& other) noexcept
    : m_hFile(other.m_hFile), m_messageHandler(std::move(other.m_messageHandler))
{
    other.m_hFile = nullptr;
}

NDPITIFFKeeper& NDPITIFFKeeper::operator=(NDPITIFFKeeper&& other) noexcept
{
    if (this != &other) {
        // Close what we own before taking what they own, or ours leaks.
        reset(other.m_hFile);
        other.m_hFile = nullptr;
        // The handler is deliberately NOT moved. It swaps libtiff's process-global
        // error and warning handlers for its lifetime, so destroying this object's
        // handler here would restore the globals while the other keeper -- and its
        // handler -- are still alive, and libtiff messages would stop reaching the log.
        // Both objects already have one; leaving each with its own keeps the globals
        // unchanged across the assignment.
    }
    return *this;
}

NDPITIFFKeeper::~NDPITIFFKeeper()
{
    NDPITiffTools::closeTiffFile(m_hFile);
}

void NDPITIFFKeeper::openTiffFile(const std::string& filePath)
{
    reset(NDPITiffTools::openTiffFile(filePath));
}

void NDPITIFFKeeper::reset(libtiff::TIFF* hFile)
{
    if (m_hFile != hFile) {
        NDPITiffTools::closeTiffFile(m_hFile);
        m_hFile = hFile;
    }
}

libtiff::TIFF* NDPITIFFKeeper::release()
{
    libtiff::TIFF* handle = m_hFile;
    m_hFile = nullptr;
    return handle;
}

void NDPITIFFKeeper::closeTiffFile()
{
    NDPITiffTools::closeTiffFile(m_hFile);
    m_hFile = nullptr;
}
