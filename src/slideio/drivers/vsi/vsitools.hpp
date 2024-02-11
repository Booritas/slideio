// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#pragma once
#include <cstdint>
#include <boost/json/object.hpp>
#include "etsfile.hpp"
#include "vsistream.hpp"
#include "slideio/base/slideio_enums.hpp"


namespace slideio
{
    namespace vsi
    {
        class TagInfo;
        enum class Compression;
        enum class StackType;
        class VSITools
        {
        public:
            static DataType toSlideioPixelType(uint32_t vsiPixelType);
            static slideio::Compression toSlideioCompression(vsi::Compression format);
            static StackType intToStackType(int value);
            static std::string getVolumeName(int tag);
            static std::string getTagName(const TagInfo& tagInfo, const std::list<TagInfo>& path);
            static bool isArray(const TagInfo& tagInfo);
            static std::string getStackTypeName(const std::string& value);
            static std::string getDeviceSubtype(const std::string& value);
            static std::string extractTagValue(vsi::VSIStream& vsi, const vsi::TagInfo& tagInfo);
        private:
            static bool isTag(const boost::json::object& parentObject, int srcTag);
            static std::string getDimensionPropertyName(int tag);
            static std::string getStackPropertyName(int tag);
        };
        
    }
}
