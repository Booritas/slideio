// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.

#include "slideio/transformer/medianblurfilterwrap.hpp"
#include "slideio/transformer/medianblurfilter.hpp"
using namespace slideio;


MedianBlurFilterWrap::MedianBlurFilterWrap() : m_filter(std::make_shared<MedianBlurFilter>()) {
}

int MedianBlurFilterWrap::getKernelSize() const {
    return m_filter->getKernelSize();
}

void MedianBlurFilterWrap::setKernelSize(int kernelSize) {
    m_filter->setKernelSize(kernelSize);
}

TransformationType MedianBlurFilterWrap::getType() const {
    return m_filter->getType();
}

std::shared_ptr<MedianBlurFilter> MedianBlurFilterWrap::getFilter() const {
    return m_filter;
}
