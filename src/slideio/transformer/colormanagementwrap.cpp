// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#include "slideio/transformer/colormanagementwrap.hpp"
#include "slideio/transformer/colormanagement.hpp"
using namespace slideio;

ColorManagementWrap::ColorManagementWrap() {
	m_filter = std::make_shared<ColorManagement>();
}

ColorManagementWrap::ColorManagementWrap(const ColorManagement& filter) {
	m_filter = std::make_shared<ColorManagement>(filter);
}

ColorTarget ColorManagementWrap::getTarget() const {
	return m_filter->getTarget();
}

void ColorManagementWrap::setTarget(ColorTarget target) {
	m_filter->setTarget(target);
}

RenderingIntent ColorManagementWrap::getIntent() const {
	return m_filter->getIntent();
}

void ColorManagementWrap::setIntent(RenderingIntent intent) {
	m_filter->setIntent(intent);
}

bool ColorManagementWrap::getBlackPointCompensation() const {
	return m_filter->getBlackPointCompensation();
}

void ColorManagementWrap::setBlackPointCompensation(bool value) {
	m_filter->setBlackPointCompensation(value);
}

MissingProfilePolicy ColorManagementWrap::getMissingProfilePolicy() const {
	return m_filter->getMissingProfilePolicy();
}

void ColorManagementWrap::setMissingProfilePolicy(MissingProfilePolicy policy) {
	m_filter->setMissingProfilePolicy(policy);
}

const ColorProfile& ColorManagementWrap::getSourceProfileOverride() const {
	return m_filter->getSourceProfileOverride();
}

void ColorManagementWrap::setSourceProfileOverride(const ColorProfile& profile) {
	m_filter->setSourceProfileOverride(profile);
}

TransformationType ColorManagementWrap::getType() const {
	return m_filter->getType();
}

std::shared_ptr<ColorManagement> ColorManagementWrap::getFilter() const {
    return m_filter;
}
