// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#include "slideio/core/slideio_core_def.hpp"
#include "slideio/base/exceptions.hpp"
#include <cstdint>

namespace slideio
{
    class SLIDEIO_CORE_EXPORTS Endian
    {
    public:
        static uint16_t swapBytes(uint16_t value) {
            return (value >> 8) | (value << 8);
        }
        static uint32_t swapBytes(uint32_t value) {
            return (value >> 24) |
            ((value << 8) & 0x00FF0000) |
            ((value >> 8) & 0x0000FF00) |
            (value << 24);
        }
        static uint64_t swapBytes(uint64_t value) {
            return (value >> 56) |
                ((value << 40) & 0x00FF000000000000) |
                ((value << 24) & 0x0000FF0000000000) |
                ((value << 8) & 0x000000FF00000000) |
                ((value >> 8) & 0x00000000FF000000) |
                ((value >> 24) & 0x0000000000FF0000) |
                ((value >> 40) & 0x000000000000FF00) |
                (value << 56);
        }
        template<typename T>
        static T fromLittleEndianToNative(T value) {
            if(isLittleEndian())
                return value;
            const int nbytes = sizeof(value);
            switch(nbytes) {
                case 2:
                    return static_cast<T>(swapBytes(static_cast<uint16_t>(value)));
                case 4:
                    return static_cast<T>(swapBytes(static_cast<uint32_t>(value)));
                case 8:
                    return static_cast<T>(swapBytes(static_cast<uint64_t>(value)));
            }
            RAISE_RUNTIME_ERROR << "Endian::fromLittleEndianToNative: unsupported data size: " << nbytes;
        }
        static bool isLittleEndian() {
            uint16_t number = 0x1;
            char* numPtr = (char*)&number;
            return (numPtr[0] == 1);
        }
        static uint16_t little2BigEndian(uint16_t value) {
            return swapBytes(value);
        }
        static uint16_t bigToLittleEndian16(uint16_t bigEndianValue) {
            return swapBytes(bigEndianValue);
        }
        static std::u16string u16StringLittleToBig(const std::u16string& inLE);
    };
}