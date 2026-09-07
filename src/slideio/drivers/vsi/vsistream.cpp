// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#include "slideio/drivers/vsi/vsistream.hpp"
#include "slideio/core/tools/tools.hpp"
#include <codecvt>

#include "slideio/core/tools/endian.hpp"
using namespace slideio::vsi;


VSIStream::VSIStream(const std::string& filePath)
    : m_reader(std::make_shared<slideio::FileReader>(filePath)) {
}

VSIStream::VSIStream(std::shared_ptr<const slideio::FileReader> reader)
    : m_reader(std::move(reader)) {
}

std::string VSIStream::readString(size_t dataSize)
{
    std::u16string wstr(dataSize + 1, '\0');
    m_reader->readAt(m_pos, (char*)wstr.data(), dataSize);
    m_pos += dataSize;
	if (!Endian::isLittleEndian()){
        wstr = Endian::u16StringLittleToBig(wstr);
    }
    wstr.erase(std::find(wstr.begin(), wstr.end(), '\0'), wstr.end());
    return Tools::fromUnicode16(wstr);
}

int64_t VSIStream::getPos() const
{
    return static_cast<int64_t>(m_pos);
}

void VSIStream::setPos(int64_t pos)
{
    m_pos = static_cast<uint64_t>(pos);
}

int64_t VSIStream::getSize()
{
    return static_cast<int64_t>(m_reader->size());
}

void VSIStream::skipBytes(uint32_t bytes)
{
    m_pos += bytes;
}

void VSIStream::readBytes(uint8_t* buffer, uint32_t size) {
    m_reader->readAt(m_pos, buffer, size);
    m_pos += size;
}
