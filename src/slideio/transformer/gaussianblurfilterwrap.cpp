// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#include "slideio/transformer/gaussianblurfilterwrap.hpp"
#include "slideio/transformer/gaussianblurfilter.hpp"
#include "slideio/transformer/transformationtype.hpp"

using namespace slideio;


GaussianBlurFilterWrap::GaussianBlurFilterWrap() : m_filter(std::make_shared<GaussianBlurFilter>()) {
}

int GaussianBlurFilterWrap::getKernelSizeX() const {
    return m_filter->getKernelSizeX();
}

void GaussianBlurFilterWrap::setKernelSizeX(int kernelSizeX) {
    m_filter->setKernelSizeX(kernelSizeX);
}

int GaussianBlurFilterWrap::getKernelSizeY() const {
    return m_filter->getKernelSizeY();
}

void GaussianBlurFilterWrap::setKernelSizeY(int kernelSizeY) {
    m_filter->setKernelSizeY(kernelSizeY);
}

double GaussianBlurFilterWrap::getSigmaX() const {
    return m_filter->getSigmaX();
}

void GaussianBlurFilterWrap::setSigmaX(double sigmaX) {
    m_filter->setSigmaX(sigmaX);
}

double GaussianBlurFilterWrap::getSigmaY() const {
    return m_filter->getSigmaY();
}

void GaussianBlurFilterWrap::setSigmaY(double sigmaY) {
    m_filter->setSigmaY(sigmaY);
}

TransformationType GaussianBlurFilterWrap::getType() const {
    return m_filter->getType();
}

std::shared_ptr<GaussianBlurFilter> GaussianBlurFilterWrap::getFilter() const {
    return m_filter;
}
