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

static std::string trimStart(const std::string& s) {
    auto it = std::find_if_not(s.begin(), s.end(), [](unsigned char ch) { return std::isspace(ch); });
    return std::string(it, s.end());
}

MetadataFormat CVSlide::recognizeMetadataFormat(const std::string& metadata) {
    std::string trimmed = trimStart(metadata);

    if (trimmed.empty()) {
        return MetadataFormat::None;
    }

    if (trimmed[0] == '{' || trimmed[0] == '[') {
        return MetadataFormat::JSON;
    }

    if (trimmed.size() >= 5 && trimmed.substr(0, 5) == "<?xml") {
        return MetadataFormat::XML;
    }
    if (trimmed[0] == '<') {
        return MetadataFormat::XML;
    }

    return MetadataFormat::Text;
}

std::string CVSlide::toString() const {
    std::ostringstream os;
    os << "File Path: " << getFilePath() << "\n";
	os << "Driver: " << getDriverId() << "\n";
    os << "Metadata Format: " << getMetadataFormat() << "\n";
    os << "Number of Scenes: " << getNumScenes() << "\n";
	os << "Auxiliary Image Names: " << "\n";
	for (const auto& name : getAuxImageNames()) {
		os << "  " << name << "\n";
	}
	std::string metadata = getRawMetadata();
	if (!metadata.empty()) {
		os << "Metadata: " << "\n";
		if (metadata.size() > 100) {
			os << metadata.substr(0, 100) << "...\n";
		}
		else {
			os << metadata << "\n";
		}
	}
	else {
		os << "Metadata: None\n";
	}
    for (int i = 0; i < getNumScenes(); ++i) {
        auto scene = getScene(i);
        os << "  Scene " << i << ": " << scene->getName() << "\n";
    }

    return os.str();
}