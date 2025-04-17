// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#include "slideio/drivers/ome-tiff/otdimensions.hpp"

#include "slideio/base/exceptions.hpp"

using namespace slideio;
using namespace slideio::ometiff;

OTDimensions::OTDimensions(const std::string& dimensionOrder, int numChannels, int numZSlices, int numTFrames,
	int samplesPerPixel)
{
	init(dimensionOrder, numChannels, numZSlices, numTFrames, samplesPerPixel);
}

void OTDimensions::init(const std::string& dimensionOrder, int numChannels, int numZSlices, int numTFrames,
	int samplesPerPixel)
{
	if (dimensionOrder.size() != 5) {
		RAISE_RUNTIME_ERROR << "OTScene: unexpected dimension order: " << dimensionOrder;
	}
	if (numChannels <= 0 || numZSlices <= 0 || numTFrames <= 0) {
		RAISE_RUNTIME_ERROR << "OTScene: unexpected dimension size: "
			<< numChannels << ", " << numZSlices << ", " << numTFrames;
	}
	if (samplesPerPixel <= 0) {
		RAISE_RUNTIME_ERROR << "OTScene: unexpected samples per pixel: " << samplesPerPixel;
	}
	const int numDimensions = 3;
	std::array<std::string,3> labels;
	std::array<int,3> sizes;
	std::array<int,3> increments = {1,1,1};
	for (size_t index = 0, i = 2; i < dimensionOrder.size() && index < numDimensions; ++i, ++index) {
		labels[index] = dimensionOrder.substr(i, 1);
		if (labels[index] == DimC) {
			sizes[index] = numChannels;
			increments[index] = samplesPerPixel;
		}
		else if (labels[index] == DimZ) {
			sizes[index] = numZSlices;
		}
		else if (labels[index] == DimT) {
			sizes[index] = numTFrames;
		}
		else {
			RAISE_RUNTIME_ERROR << "OTScene: unsupported dimension label: " << labels[i];
		}
	}
	Dimensions::init(labels, sizes, increments);
}