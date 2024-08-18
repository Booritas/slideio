// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#include "slideio/transformer/cannyfilterwrap.hpp"
#include "slideio/transformer/cannyfilter.hpp"
using namespace slideio;

CannyFilterWrap::CannyFilterWrap() {
	m_filter = std::make_shared<CannyFilter>();
}

double CannyFilterWrap::getThreshold1() const {
	return m_filter->getThreshold1();
}

void CannyFilterWrap::setThreshold1(double threshold1) {
	m_filter->setThreshold1(threshold1);
}

double CannyFilterWrap::getThreshold2() const {
	return m_filter->getThreshold2();
}

void CannyFilterWrap::setThreshold2(double threshold2) {
	m_filter->setThreshold2(threshold2);
}

int CannyFilterWrap::getApertureSize() const {
	return m_filter->getApertureSize();
}

void CannyFilterWrap::setApertureSize(int apertureSize) {
	m_filter->setApertureSize(apertureSize);
}

bool CannyFilterWrap::getL2Gradient() const {
	return m_filter->getL2Gradient();
}

void CannyFilterWrap::setL2Gradient(bool L2gradient) {
	m_filter->setL2Gradient(L2gradient);
}

TransformationType CannyFilterWrap::getType() const {
	return m_filter->getType();
}
