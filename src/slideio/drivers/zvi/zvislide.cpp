// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#include "slideio/drivers/zvi/zvislide.hpp"
#include "slideio/drivers/zvi/zviscene.hpp"
#include <boost/filesystem.hpp>
#include <pole/polepp.hpp>
#include <boost/format.hpp>

using namespace slideio;

ZVISlide::ZVISlide(const std::string& filePath) : m_filePath(filePath)
{
	init();
}

int ZVISlide::getNumScenes() const
{
	return 0;
}

std::string ZVISlide::getFilePath() const
{
	return "";
}

std::shared_ptr<CVScene> ZVISlide::getScene(int index) const
{
	return nullptr;
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
    namespace fs = boost::filesystem;
    if (!fs::exists(m_filePath)) {
        throw std::runtime_error(std::string("ZVIImageDriver: File does not exist:") + m_filePath);
    }
	ole::compound_document doc(m_filePath);
	if(!doc.good())
	{
		throw std::runtime_error(
			(boost::format("Cannot open compound file %1%") % m_filePath).str());
	}
}

