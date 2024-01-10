// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#include "slideio/drivers/dcm/wsiscene.hpp"

using namespace slideio;


WSIScene::WSIScene() {
    
}

void WSIScene::addFile(std::shared_ptr<DCMFile>& file) {
	m_files.push_back(file);
}

void WSIScene::init() {
}

std::string WSIScene::getFilePath() const {
	return"";
}

std::string WSIScene::getName() const {
	return"";
}

cv::Rect WSIScene::getRect() const {
	return cv::Rect();
}

int WSIScene::getNumChannels() const {
	return 0;
}

slideio::DataType WSIScene::getChannelDataType(int channel) const {
	return slideio::DataType::DT_Unknown;
}

Resolution WSIScene::getResolution() const {
	return Resolution();
}

double WSIScene::getMagnification() const {
	return 0.0;
}

Compression WSIScene::getCompression() const {
	return Compression::Unknown;
}
