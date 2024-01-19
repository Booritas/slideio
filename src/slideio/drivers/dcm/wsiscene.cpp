// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#include "slideio/drivers/dcm/wsiscene.hpp"
#include <boost/filesystem.hpp>

#include "slideio/base/exceptions.hpp"
#include "slideio/core/tools/tools.hpp"
#include "slideio/drivers/dcm/dcmscene.hpp"

using namespace slideio;
namespace fs = boost::filesystem;


WSIScene::WSIScene() {
    
}

void WSIScene::addFile(std::shared_ptr<DCMFile>& file) {
	if(file->isAuxImage()) {
        std::shared_ptr<DCMScene> auxScene = std::make_shared<DCMScene>();
		auxScene->addFile(file);
		auxScene->init();
	    m_auxNames.push_back(file->getImageType());
		m_auxImages[file->getImageType()] = auxScene;
    } else {
		m_files.push_back(file);
	}
}

void WSIScene::init() {
	std::sort(m_files.begin(), m_files.end(), 
		[](const std::shared_ptr<DCMFile>& file1, const std::shared_ptr<DCMFile>& file2) {
		    return file1->getWidth() > file2->getWidth();
		});

	std::shared_ptr<DCMFile> baseFile = getBaseFile();
	const int baseWidth = baseFile->getWidth();

	for(auto& file : m_files) {
	    const int width = file->getWidth();
		const double scale = static_cast<double>(width) / static_cast<double>(baseWidth);
		file->setScale(scale);
	}

	m_name = baseFile->getSeriesUID();
	m_rect = cv::Rect(0, 0, baseFile->getWidth(), baseFile->getHeight());
	m_dataType = baseFile->getDataType();
	m_compression = baseFile->getCompression();
	m_numChannels = baseFile->getNumChannels();
	const int numFiles = static_cast<int>(m_files.size());
	if (numFiles == 1) {
		m_filePath = m_files[0]->getFilePath();
	}
	else if (numFiles > 0) {
		const fs::path filePath(baseFile->getFilePath());
		m_filePath = filePath.parent_path().string();
	}
	m_magnification = baseFile->getMagnification();
	m_resolution = baseFile->getResolution();
}

std::string WSIScene::getFilePath() const {
	return m_filePath;
}

std::string WSIScene::getName() const {
	return m_name;
}

cv::Rect WSIScene::getRect() const {
	return m_rect;
}

int WSIScene::getNumChannels() const {
	return m_numChannels;
}

slideio::DataType WSIScene::getChannelDataType(int channel) const {
	return m_dataType;
}

Resolution WSIScene::getResolution() const {
	return m_resolution;
}

double WSIScene::getMagnification() const {
	return m_magnification;
}

Compression WSIScene::getCompression() const {
	return m_compression;
}

std::shared_ptr<DCMFile> WSIScene::getBaseFile() const {
	if(m_files.empty()) {
	    RAISE_RUNTIME_ERROR << "DICOM WSI: No files in the scene";
	}
    return m_files[0];
}

int WSIScene::getTileCount(void* userData) {
	TilerData * data = static_cast<TilerData*>(userData);
    std::shared_ptr<DCMFile> zoomFile = m_files[data->zoomLevelIndex];
	return zoomFile->getNumFrames();
}

bool WSIScene::getTileRect(int tileIndex, cv::Rect& tileRect, void* userData) {
	TilerData* data = static_cast<TilerData*>(userData);
	std::shared_ptr<DCMFile> zoomFile = m_files[data->zoomLevelIndex];
	return zoomFile->getTileRect(tileIndex, tileRect);
}

bool WSIScene::readTile(int tileIndex, const std::vector<int>& channelIndices, cv::OutputArray tileRaster,
    void* userData) {
	TilerData* data = static_cast<TilerData*>(userData);
	std::shared_ptr<DCMFile> zoomFile = m_files[data->zoomLevelIndex];
	cv::Mat tile;
	if(zoomFile->readFrame(tileIndex, tile)) {
		Tools::extractChannels(tile, channelIndices, tileRaster);
	    return true;
    }
	return false;
}

void WSIScene::initializeBlock(const cv::Size& blockSize, const std::vector<int>& channelIndices,
    cv::OutputArray output) {
	initializeSceneBlock(blockSize, channelIndices, output);
}

void WSIScene::readResampledBlockChannelsEx(const cv::Rect& blockRect, const cv::Size& blockSize,
    const std::vector<int>& componentIndices, int zSliceIndex, int tFrameIndex, cv::OutputArray output) {
	TilerData userData;
	const double zoomX = static_cast<double>(blockSize.width) / static_cast<double>(blockRect.width);
	const double zoomY = static_cast<double>(blockSize.height) / static_cast<double>(blockRect.height);
	const double zoom = std::max(zoomX, zoomY);
	auto& files = m_files;
	userData.zoomLevelIndex = Tools::findZoomLevel(zoom, static_cast<int>(m_files.size()), [&files](int index) {
		return files[index]->getScale();
		});
	const double levelZoom = files[userData.zoomLevelIndex]->getScale();
	cv::Rect zoomLevelRect;
	Tools::scaleRect(blockRect, levelZoom, levelZoom, zoomLevelRect);
	userData.relativeZoom = levelZoom / zoom;
	userData.zSliceIndex = zSliceIndex;
	userData.tFrameIndex = tFrameIndex;
	TileComposer::composeRect(this, componentIndices, zoomLevelRect, blockSize, output, &userData);
}

std::shared_ptr<CVScene> WSIScene::getAuxImage(const std::string& imageName) const {
	auto it = m_auxImages.find(imageName);
	if(it == m_auxImages.end()) {
	    RAISE_RUNTIME_ERROR << "DICOM WSI: cannot find aux image " + imageName;
	}
    std::shared_ptr<DCMScene> scene = it->second;
    return scene;
}
