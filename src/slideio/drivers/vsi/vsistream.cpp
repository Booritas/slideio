// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#include "slideio/drivers/vsi/vsistream.hpp"

#include "slideio/core/tools/tools.hpp"

using namespace slideio::vsi;


std::string VSIStream::readString(size_t dataSize)
{
#if defined(WIN32)
    std::wstring wstr(dataSize + 1, '\0');
    m_stream->read((char*)wstr.data(), dataSize);
    if (m_stream->bad()) {
        RAISE_RUNTIME_ERROR << "VSI driver: error by reading stream";
    }
    wstr.erase(std::find(wstr.begin(), wstr.end(), '\0'), wstr.end());
    return Tools::fromWstring(wstr);
#else
    std::string wstr(dataSize + 1, '\0');
    m_stream->read((char*)wstr[0], dataSize);
    if (m_stream->bad()) {
        RAISE_RUNTIME_ERROR << "VSI driver: error by reading stream";
    }
    return wstr;
#endif
}

int64_t VSIStream::getPos() const
{
    return m_stream->tellg();
}

void VSIStream::setPos(int64_t pos)
{
    m_stream->seekg(pos);
    if (m_stream->bad()) {
        RAISE_RUNTIME_ERROR << "VSI driver: error by setting stream position: " << pos;
    }
}

int64_t VSIStream::getSize()
{
    if(m_size<0) {
        const auto pos = m_stream->tellg();
        m_stream->seekg(0, std::ios::end);
        m_size = m_stream->tellg();
        m_stream->seekg(pos);
        if (m_stream->bad()) {
            RAISE_RUNTIME_ERROR << "VSI driver: error by getting stream size";
        }
    }
    return m_size;
}

void VSIStream::skipBytes(uint32_t bytes)
{
    m_stream->seekg(bytes, std::ios::cur);
    if (m_stream->bad()) {
               RAISE_RUNTIME_ERROR << "VSI driver: error by skipping stream bytes";
    }
}