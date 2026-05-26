// src/tests/main/memorystream.hpp
#pragma once

#include "slideio/base/randomaccessstream.hpp"
#include <cstring>
#include <stdexcept>
#include <string>
#include <vector>

namespace slideio { namespace tests {

class MemoryStream : public slideio::RandomAccessStream
{
public:
    MemoryStream(std::vector<uint8_t> data, std::string uri = "memory://")
        : m_data(std::move(data)), m_uri(std::move(uri)) {}

    uint64_t size() const override { return m_data.size(); }

    size_t read(uint64_t offset, size_t count, void* buf) override
    {
        if (offset >= m_data.size()) return 0;
        const size_t avail = m_data.size() - static_cast<size_t>(offset);
        const size_t toCopy = (count < avail) ? count : avail;
        std::memcpy(buf, m_data.data() + offset, toCopy);
        return toCopy;
    }

    std::string uri() const override { return m_uri; }

private:
    std::vector<uint8_t> m_data;
    std::string m_uri;
};

}} // namespace slideio::tests
