// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#include "slideio/drivers/vsi/vsistream.hpp"
#include "slideio/core/tools/tools.hpp"
#include <codecvt>
using namespace slideio::vsi;


VSIStream::VSIStream(std::string& filePath): m_size(-1) {
#if defined(WIN32)
    std::wstring filePathW = Tools::toWstring(filePath);
    m_stream = std::make_unique<std::ifstream>(filePathW, std::ios::binary);
#else
    m_stream = std::make_unique<std::ifstream>(filePath, std::ios::binary);
#endif
}

std::string VSIStream::readString(size_t dataSize)
{
    std::u16string wstr(dataSize + 1, '\0');
    m_stream->read((char*)wstr.data(), dataSize);
    if (m_stream->bad()) {
        RAISE_RUNTIME_ERROR << "VSI driver: error by reading stream";
    }
    wstr.erase(std::find(wstr.begin(), wstr.end(), '\0'), wstr.end());
    return Tools::fromUnicode16(wstr);
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

void VSIStream::readBytes(uint8_t* buffer, uint32_t size) {
    m_stream->read(reinterpret_cast<char*>(buffer), size);
    if (m_stream->bad()) {
        RAISE_RUNTIME_ERROR << "VSI driver: error by reading " << size << " bytes from stream";
    }
}
