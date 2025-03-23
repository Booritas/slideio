// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#pragma once
#include "slideio/core/slideio_core_def.hpp"
#include "slideio/base/exceptions.hpp"
#include <cstdint>
#include "slideio/base/slideio_enums.hpp"

namespace slideio
{
    enum class DataType;

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

        template <typename T>
		void fromLittleEndianToNative(T* data, size_t count) {
			if (isLittleEndian())
				return;
			for (size_t i = 0; i < count; ++i) {
				data[i] = swapBytes(data[i]);
			}
		}
		inline void fromLittleEndianToNative(slideio::DataType dt, void* data, size_t count) {
			if (isLittleEndian()) {
				return;
			}
            switch (dt) {
            case DataType::DT_Byte:
            case DataType::DT_Int8:
                break;
            case DataType::DT_Int16:
                fromLittleEndianToNative(static_cast<int16_t*>(data), count / sizeof(int16_t));
                break;
            case DataType::DT_Int32:
                fromLittleEndianToNative(static_cast<int32_t*>(data), count / sizeof(int32_t));
                break;
            case DataType::DT_Float32:
				fromLittleEndianToNative(static_cast<float*>(data), count / sizeof(float));
                break;
            case DataType::DT_Float64:
				fromLittleEndianToNative(static_cast<double*>(data), count / sizeof(double));
                break;
            case DataType::DT_UInt16:
				fromLittleEndianToNative(static_cast<uint16_t*>(data), count / sizeof(uint16_t));
                break;
            case DataType::DT_UInt32:
				fromLittleEndianToNative(static_cast<uint32_t*>(data), count / sizeof(uint32_t));
                break;
            case DataType::DT_Int64:
				fromLittleEndianToNative(static_cast<int64_t*>(data), count / sizeof(int64_t));
                break;
            case DataType::DT_UInt64:
				fromLittleEndianToNative(static_cast<uint64_t*>(data), count / sizeof(uint64_t));
                break;
			default:
				RAISE_RUNTIME_ERROR << "fromLittleEndianToNative: Unsupported data type " << static_cast<int>(dt);
                break;
            }
        }
    };
}
