// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#pragma once

#include "slideio/drivers/ome-tiff/ot_api_def.hpp"
#include "slideio/core/dimensions.hpp"

namespace slideio
{
	namespace ometiff
	{
		const std::string DimC = "C";
		const std::string DimZ = "Z";
		const std::string DimT = "T";

		class SLIDEIO_OMETIFF_EXPORTS OTDimensions : public slideio::Dimensions
		{
		public:
			OTDimensions() = default;
			OTDimensions(const std::string& dimensionOrder, int numChannels, int numZSlices, int numTFrames,
				int samplesPerPixel);
			void init(const std::string& dimensionOrder, int numChannels, int numZSlices, int numTFrames,
				int samplesPerPixel);
		};
	}
}