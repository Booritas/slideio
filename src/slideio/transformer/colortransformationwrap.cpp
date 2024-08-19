// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.

#include "slideio/transformer/colortransformationwrap.hpp"
#include "slideio/transformer/colortransformation.hpp"
using namespace slideio;

ColorTransformationWrap::ColorTransformationWrap() {
	m_filter = std::make_shared<ColorTransformation>();
}

ColorSpace ColorTransformationWrap::getColorSpace() const {
	return m_filter->getColorSpace();
}

void ColorTransformationWrap::setColorSpace(ColorSpace colorSpace) {
	m_filter->setColorSpace(colorSpace);
}

TransformationType ColorTransformationWrap::getType() const {
	return m_filter->getType();
}

std::shared_ptr<ColorTransformation> ColorTransformationWrap::getFilter() const {
    return m_filter;
}
