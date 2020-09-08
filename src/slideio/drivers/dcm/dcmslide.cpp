// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#include "slideio/drivers/dcm/dcmslide.hpp"
#include "slideio/drivers/dcm/dcmscene.hpp"

using namespace slideio;

DCMSlide::DCMSlide(const std::string& filePath)
{
}

int DCMSlide::getNumScenes() const
{
	return 0;
}

std::string DCMSlide::getFilePath() const
{
	return "";
}

std::shared_ptr<CVScene> DCMSlide::getScene(int index) const
{
	return nullptr;
}

double DCMSlide::getMagnification() const
{
	return 0;
}

Resolution DCMSlide::getResolution() const
{
	return Resolution();
}

double DCMSlide::getZSliceResolution() const
{
	return 0;
}

double DCMSlide::getTFrameResolution() const
{
	return 0;
}
