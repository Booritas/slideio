// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#pragma once
#include <fstream>

#include "slideio/base/exceptions.hpp"
#include "slideio/drivers/vsi/vsi_api_def.hpp"

namespace slideio {
    namespace vsi
    {
        class SLIDEIO_VSI_EXPORTS VSIStream
        {
        public:
            VSIStream(std::ifstream& stream) : m_size(-1) {
                m_stream = &stream;
            }
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

        private:
            std::ifstream* m_stream;
            int64_t m_size;

        };
    };
}
