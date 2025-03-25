// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#include "slideio/core/cvslide.hpp"

using namespace slideio;

std::shared_ptr<CVScene> CVSlide::getSceneByName(const std::string& name) {
	for (int i = 0; i < getNumScenes(); ++i) {
		auto scene = getScene(i);
		if (scene->getName() == name) {
			return scene;
		}
	}
	return nullptr;
}

std::shared_ptr<CVScene> CVSlide::getAuxImage(const std::string& sceneName) const
{
    throw std::runtime_error("The slide does not have any auxiliary image");
}
