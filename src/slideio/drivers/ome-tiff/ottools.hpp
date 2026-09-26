// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#pragma once

#include "slideio/drivers/ome-tiff/ot_api_def.hpp"
#include <opencv2/core.hpp>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace tinyxml2 { class XMLElement; }

#include "otscene.hpp"
#include "slideio/core/slideio_enums.hpp"

namespace slideio
{
    namespace ometiff
    {
        class SLIDEIO_OMETIFF_EXPORTS OTTools
        {
        public:
			static DataType stringToDataType(const std::string& type);
            static double convertToMeters(double value, const std::string& units);
            static double convertToSeconds(double tResolution, const std::string& units);
            /**@brief Seconds per one of the named OME time unit, or nullopt if it is
             * not one we know. Callers converting a recorded time must refuse an
             * unreadable unit rather than assume seconds: reporting a millisecond as
             * a second is worse than reporting no time at all. */
            static std::optional<double> timeUnitToSeconds(const std::string& units);
            /**@brief PhysicalSizeZ of a Pixels element in metres, 0 if unstated. */
            static double readZSliceResolution(const tinyxml2::XMLElement* pixels);
            /**@brief PhysicalSizeT of a Pixels element in seconds, 0 if unstated or if
             * its unit cannot be read. */
            static double readTFrameResolution(const tinyxml2::XMLElement* pixels);
            /**@brief OME AcquisitionDate (an xsd:dateTime) as seconds since
             * 1970-01-01T00:00:00Z, or nullopt if it cannot be read.
             *
             * Read as UTC when the text carries no offset, so one file yields one
             * epoch whatever the timezone of the machine reading it. */
            static std::optional<int64_t> parseAcquisitionDate(const std::string& text);
            /**@brief Per-plane DeltaT values of a Pixels element, in seconds, indexed
             * (t * numZSlices + z) * numChannels + c, exactly as the file states them.
             *
             * nullopt unless every plane of the scene states a readable DeltaT: the
             * result is addressed by plane, so a gap would leave one plane reporting
             * a time that belongs to no plane at all. Applying the origin rule of
             * CVScene::getPlaneTimestamp() is the caller's job. */
            static std::optional<std::vector<double>> collectPlaneTimestamps(
                const tinyxml2::XMLElement* pixels, int numTFrames, int numChannels,
                int numZSlices);
        };
    }
}
