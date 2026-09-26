// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#pragma once
#include <cstdint>
#include <optional>
#include <string>
#include <vector>
#include "slideio/drivers/vsi/vsi_api_def.hpp"
#include "etsfile.hpp"
#include "vsistream.hpp"
#include "slideio/core/slideio_enums.hpp"
#include <nlohmann/json.hpp>

using json = nlohmann::json;

namespace slideio
{
    namespace vsi
    {
        class TagInfo;
        enum class Compression;
        enum class StackType;
        class SLIDEIO_VSI_EXPORTS VSITools
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
            /**@brief Convert an Olympus VSI time unit string to seconds per raw unit.
             *
             * Accepts forms such as "10^-3s^1", "s^1", "10^-6s". Returns nullopt if
             * empty or unparseable (callers must not invent a default like 1e-3).
             */
            static std::optional<double> unitToSeconds(const std::string& unitStr);
            /**@brief True if a node carries a plane timestamp.
             *
             * TIME_VALUE shares tag 2017 with VECTOR_LAYER_VOLUME, so the tag alone
             * does not identify one: a volume walk that trusted it would read a
             * vector layer as a timestamp. A timestamp states the unit of its value
             * and a vector layer does not, which is the same guard the TIME_INCREMENT
             * overload of tag 2016 uses.
             */
            static bool isPlaneTimestampNode(const TagInfo& node);
            /**@brief Per-plane times read from one volume subtree. */
            struct PlaneTimes
            {
                /** One entry per plane, in the order the file lists them. Empty when
                 *  any timestamp node was unreadable: the list is addressed by
                 *  position, so dropping an entry would move every later plane onto
                 *  its neighbour's time. No timestamps beats wrong ones. */
                std::vector<double> timestamps;
                std::string timestampUnit;
                std::optional<double> increment;
                std::string incrementUnit;
            };
            /**@brief Reads the plane timestamps and the time increment of a volume. */
            static PlaneTimes collectPlaneTimes(const TagInfo& volume);
            /** Linear index into a TIME_VALUE list of length nT*nC*nZ.
             *
             * When orderT/C/Z are all set (>= 2 from DIMENSION_DESCRIPTION),
             * the higher order is the slower axis (T fastest / C slowest on
             * IX73-style stacks). Otherwise falls back to TZC:
             * (t*nZ+z)*nC+c.
             */
            static int planeTimestampListIndex(int t, int c, int z,
                                               int nT, int nC, int nZ,
                                               int orderT, int orderC, int orderZ);
        private:
            static bool isTag(const json& parentObject, int srcTag);
            static std::string getDimensionPropertyName(int tag);
            static std::string getStackPropertyName(int tag);
        };
        
    }
}
