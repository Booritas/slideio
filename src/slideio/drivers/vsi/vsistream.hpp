// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#pragma once
#include <fstream>
#include "slideio/base/exceptions.hpp"
#include "slideio/drivers/vsi/vsi_api_def.hpp"
#include <cstdint>
#include <memory>

#if defined(_MSC_VER)
#pragma warning( push )
#pragma warning(disable: 4251)
#endif

namespace slideio {
    namespace vsi
    {
        class SLIDEIO_VSI_EXPORTS VSIStream
        {
        public:
            VSIStream(std::string& filePath);

            template <typename T>
            void read(T& value) {
                m_stream->read((char*)&value, sizeof(T));
                if (m_stream->bad()) {
                    RAISE_RUNTIME_ERROR << "VSI driver: error by reading stream";
                }
            }
            template <typename T>
            T readValue() {
                T value;
                m_stream->read((char*)&value, sizeof(T));
                if (m_stream->bad()) {
                    RAISE_RUNTIME_ERROR << "VSI driver: error by reading stream";
                }
                return value;
            }
            std::string readString(size_t dataSize);
            int64_t getPos() const;
            void setPos(int64_t pos);
            int64_t getSize();
            void skipBytes(uint32_t bytes);
            void readBytes(uint8_t* bytes, uint32_t size);
        private:
            std::unique_ptr<std::ifstream> m_stream;
            int64_t m_size;

        };
    };
}

#if defined(_MSC_VER)
#pragma warning( pop )
#endif
