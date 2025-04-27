// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#include "slideio/base/exceptions.hpp"
#include "slideio/drivers/gdal/gdalslide.hpp"
#include "slideio/drivers/gdal/gdalscene.hpp"
#include <nlohmann/json.hpp>

using json = nlohmann::json;

slideio::GDALSlide::GDALSlide(GDALDatasetH ds, const std::string& filePath)
{
	m_scene.reset(new slideio::GDALScene(ds, filePath));
	readMetadata(ds);
}

slideio::GDALSlide::~GDALSlide()
{
}

int slideio::GDALSlide::getNumScenes() const
{
	return (m_scene==nullptr)?0:1;
}

std::string slideio::GDALSlide::getFilePath() const
{
	if(m_scene!=nullptr)
		return m_scene->getFilePath();
	
	std::string empty_path;
	return empty_path;
}

std::shared_ptr<slideio::CVScene> slideio::GDALSlide::getScene(int index) const
{
	if(index>=getNumScenes()) {
		RAISE_RUNTIME_ERROR << "GDAL driver: invalid scene index";
	}
	return m_scene;
}

// Trim from start (in place)
static inline void ltrim(std::string& s) {
	s.erase(s.begin(), std::find_if(s.begin(), s.end(), [](unsigned char ch) {
		return !std::isspace(ch);
		}));
}

// Trim from end (in place)
static inline void rtrim(std::string& s) {
	s.erase(std::find_if(s.rbegin(), s.rend(), [](unsigned char ch) {
		return !std::isspace(ch);
		}).base(), s.end());
}

// Trim from both ends (in place)
static inline void trim(std::string& s) {
	ltrim(s);
	rtrim(s);
}

void slideio::GDALSlide::readMetadata(GDALDatasetH ds) {
	auto driver = GDALGetDatasetDriver(ds);
	std::string driverName = GDALGetDriverShortName(driver);
{
		char** mtd = GDALGetMetadata(ds, nullptr);
		if (mtd != nullptr) {
            nlohmann::json mtdObj = json::object();
			for (auto item = mtd; item != nullptr && *item != nullptr; ++item) {
				std::string mtdStr = *item;
				std::string::size_type pos = mtdStr.find('=');
				if (pos != std::string::npos) {
					std::string key = mtdStr.substr(0, pos);
					std::string value = mtdStr.substr(pos + 1);
					trim(value);
					mtdObj[key] = value;
					if (driverName == "GTiff") {
						if (key == "TIFFTAG_IMAGEDESCRIPTION"
							&& value.size() > 0
							&& (value[0] == '{' || value[0] == '[' || value[0] == '<')) {
							m_rawMetadata = value;
							m_metadataFormat = recognizeMetadataFormat(m_rawMetadata);
							return;
						}
					}
				}
			}
			m_rawMetadata = mtdObj.dump();
			m_metadataFormat = recognizeMetadataFormat(m_rawMetadata);
		}
	}
}
