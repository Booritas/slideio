// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#include "slideio/transformer/transformationex.hpp"
#include "slideio/transformer/transformationtype.hpp"

using namespace slideio;

TransformationEx::TransformationEx() {
    m_type = TransformationType::Unknown;
}

std::vector<DataType> TransformationEx::computeChannelDataTypes(const std::vector<DataType>& channels) const
{
	std::vector<DataType> copy(channels);
	return copy;
}

int TransformationEx::getInflationValue() const
{
    return 0;
}
