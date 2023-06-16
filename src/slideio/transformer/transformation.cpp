// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#include "transformation.hpp"

using namespace slideio;
std::vector<DataType> Transformation::computeChannelDataTypes(const std::vector<DataType>& channels) const
{
	std::vector<DataType> copy(channels);
	return copy;
}

int Transformation::getInflationValue() const
{
    return 0;
}
