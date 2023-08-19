// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#pragma once
#include <cstdint>

#include "etsfile.hpp"
#include "vsistream.hpp"
#include "vsitags.hpp"
#include "slideio/base/slideio_enums.hpp"

namespace slideio
{
    namespace vsi
    {
        enum class Tag;

        struct TagInfo
        {
            Tag tag = Tag::UNKNOWN;
            int fieldType = 0;
            ValueType valueType = ValueType::UNSET;
            ExtendedType extendedType = ExtendedType::UNSET;
            int secondTag = -1;
            bool extended = false;
            int32_t dataSize = 0;

        };
        class VSITools
        {
        public:
            static DataType toSlideioPixelType(uint32_t vsiPixelType);
            static slideio::Compression toSlideioCompression(vsi::Compression format);
            static StackType intToStackType(int value);
            static std::string getVolumeName(Tag tag);
            static std::string getTagName(const TagInfo& tagInfo);
            static bool isArray(const TagInfo& tagInfo);
            static std::string getStackTypeName(const std::string& value);
            static std::string getDeviceSubtype(const std::string& value);
            static std::string extractTagValue(vsi::VSIStream& vsi, const vsi::TagInfo& tagInfo);
        };
        
    }
}
