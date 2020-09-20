// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#include "slideio/drivers/zvi/zvislide.hpp"
#include "slideio/drivers/zvi/zviscene.hpp"
#include "slideio/drivers/zvi/zviutils.hpp"

using namespace slideio;

ZVISlide::ZVISlide(const std::string& filePath) : m_filePath(filePath)
{
	init();
}

int ZVISlide::getNumScenes() const
{
	return 1;
}

std::string ZVISlide::getFilePath() const
{
	return "";
}

std::shared_ptr<CVScene> ZVISlide::getScene(int index) const
{
	if(index!=0)
	{
		throw std::runtime_error("ZVIImageDriver: Invalid scene index");
	}
	return m_scene;
}

double ZVISlide::getMagnification() const
{
	return 0;
}

Resolution ZVISlide::getResolution() const
{
	return Resolution();
}

double ZVISlide::getZSliceResolution() const
{
	return 0;
}

double ZVISlide::getTFrameResolution() const
{
	return 0;
}

void ZVISlide::init()
{
	m_scene.reset(new ZVIScene(m_filePath));
}

