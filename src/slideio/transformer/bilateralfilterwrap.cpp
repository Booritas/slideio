// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#include "slideio/transformer/bilateralfilterwrap.hpp"
#include "slideio/transformer/bilateralfilter.hpp"

using namespace slideio;

BilateralFilterWrap::BilateralFilterWrap() {
    m_filter = std::make_shared<BilateralFilter>();
}

int BilateralFilterWrap::getDiameter() const {
    return m_filter->getDiameter();
}

void BilateralFilterWrap::setDiameter(int diameter) {
    m_filter->setDiameter(diameter);
}

double BilateralFilterWrap::getSigmaColor() const {
    return m_filter->getSigmaColor();
}

void BilateralFilterWrap::setSigmaColor(double sigmaColor) {
    m_filter->setSigmaColor(sigmaColor);
}

double BilateralFilterWrap::getSigmaSpace() const {
    return m_filter->getSigmaSpace();
}

void BilateralFilterWrap::setSigmaSpace(double sigmaSpace) {
    m_filter->setSigmaSpace(sigmaSpace);
}

TransformationType BilateralFilterWrap::getType() const {
    return m_filter->getType();
}

std::shared_ptr<BilateralFilter> BilateralFilterWrap::getFilter() const {
    return m_filter;
}
