// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#include "slideio/core/slideio_core_def.hpp"
#include "slideio/base/exceptions.hpp"
#include <cstdint>

namespace slideio
{
    namespace Endian
    {
        inline bool isLittleEndian() {
            uint16_t number = 0x1;
            char* numPtr = reinterpret_cast<char*>(&number);
            return (numPtr[0] == 1);
        }
        inline uint16_t swapBytes(uint16_t value) {
            return (value >> 8) | (value << 8);
        }
        inline uint32_t swapBytes(uint32_t value) {
            return (value >> 24) |
            ((value << 8) & 0x00FF0000) |
            ((value >> 8) & 0x0000FF00) |
            (value << 24);
        }
        inline uint64_t swapBytes(uint64_t value) {
            return (value >> 56) |
                ((value << 40) & 0x00FF000000000000) |
                ((value << 24) & 0x0000FF0000000000) |
                ((value << 8) & 0x000000FF00000000) |
                ((value >> 8) & 0x00000000FF000000) |
                ((value >> 24) & 0x0000000000FF0000) |
                ((value >> 40) & 0x000000000000FF00) |
                (value << 56);
        }
        inline int16_t swapBytes(int16_t value) {
            uint16_t val(0);
			memcpy(&val, &value, sizeof(val));
			val = swapBytes(val);
			memcpy(&value, &val, sizeof(val));
            return value;
        }
        inline int32_t swapBytes(int32_t value) {
            uint32_t val(0);
            memcpy(&val, &value, sizeof(val));
            val = swapBytes(val);
            memcpy(&value, &val, sizeof(val));
            return value;
        }
        inline int64_t swapBytes(int64_t value) {
            uint64_t val(0);
            memcpy(&val, &value, sizeof(val));
            val = swapBytes(val);
            memcpy(&value, &val, sizeof(val));
            return value;
        }
        inline float swapBytes(float value) {
            uint32_t val(0);
            memcpy(&val, &value, sizeof(val));
            val = swapBytes(val);
            memcpy(&value, &val, sizeof(val));
            return value;
        }
        inline double swapBytes(double value) {
            uint64_t val(0);
            memcpy(&val, &value, sizeof(val));
            val = swapBytes(val);
            memcpy(&value, &val, sizeof(val));
            return value;
        }
        template<typename T>
        T fromLittleEndianToNative(T value) {
            if(isLittleEndian())
                return value;
            return swapBytes(value);
        }
        inline uint16_t little2BigEndian(uint16_t value) {
            return swapBytes(value);
        }
        inline uint16_t bigToLittleEndian16(uint16_t bigEndianValue) {
            return swapBytes(bigEndianValue);
        }
        SLIDEIO_CORE_EXPORTS std::u16string u16StringLittleToBig(const std::u16string& inLE);
    };
}