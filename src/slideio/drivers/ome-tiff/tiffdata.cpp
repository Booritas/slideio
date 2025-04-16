// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#include "slideio/drivers/ome-tiff/tiffdata.hpp"
#include "slideio/drivers/ome-tiff/otstructs.hpp"
#include "slideio/core/dimensions.hpp"
#include "slideio/base/log.hpp"
#include "slideio/imagetools/tifffiles.hpp"
#include "slideio/core/tools/tools.hpp"
#include <tinyxml2.h>
#include <filesystem>
#include <numeric>

#include "otdimensions.hpp"

inline bool isValueInRange(int value, const cv::Range& range) {
	return value >= range.start && value < range.end;
}

using namespace slideio;
using namespace slideio::ometiff;

void TiffData::init(const std::string& directoryPath, TIFFFiles* files, Dimensions *dims, tinyxml2::XMLElement* xmlTiffData) {
	if (xmlTiffData == nullptr) {
		RAISE_RUNTIME_ERROR << "TiffData: Unexpected xmlTiffData is null";
	}
	if (dims == nullptr) {
		RAISE_RUNTIME_ERROR << "TiffData: Unexpected dimensions is null";
	}
	if (directoryPath.empty()) {
		RAISE_RUNTIME_ERROR << "TiffData: Unexpected empty directory path";
	}
	if (files == nullptr) {
		RAISE_RUNTIME_ERROR << "TiffData: Unexpected TIFFFiles collection is null";
	}
	m_dimensions = dims;
    int firstChannel = xmlTiffData->IntAttribute("FirstC", 0);
    int firstZSlice = xmlTiffData->IntAttribute("FirstZ", 0);
    int firstTFrame = xmlTiffData->IntAttribute("FirstT", 0);

    m_channelRange = { firstChannel, firstChannel + 1 };
    m_zSliceRange = { firstZSlice, firstZSlice + 1 };
    m_tFrameRange = { firstTFrame, firstTFrame + 1 };

    m_firstIFD = xmlTiffData->IntAttribute("IFD", 0);
    m_planeCount = xmlTiffData->IntAttribute("PlaneCount", 0);

    std::list<std::pair<std::string, int>> coordList = {
        { DimC, firstChannel},
        { DimZ, firstZSlice },
        { DimT, firstTFrame }
    };
    auto coords = m_dimensions->createCoordinates(coordList);
    for (int plane = 1; plane < m_planeCount; ++plane) {
        m_dimensions->incrementCoordinates(coords);
    }
    int cIndex = m_dimensions->getDimensionIndex(DimC);
    int zIndex = m_dimensions->getDimensionIndex(DimZ);
    int tIndex = m_dimensions->getDimensionIndex(DimT);

    m_channelRange.end = coords[cIndex] + 1;
    m_zSliceRange.end = coords[zIndex] + 1;
    m_tFrameRange.end = coords[tIndex] + 1;

    if (m_channelRange.start < 0 || m_zSliceRange.start < 0
        && m_zSliceRange.start < 0 || m_firstIFD < 0 || m_planeCount <= 0) {
        RAISE_RUNTIME_ERROR << "Invalid TiffData element in the xml metadata: "
            "FirstC: " << m_channelRange.start
            << " FirstZ: " << m_zSliceRange.start
            << " FirstT: " << m_tFrameRange.start
            << " IFD: " << m_firstIFD
            << " PlaneCount: " << m_planeCount;
    }

    tinyxml2::XMLElement* uuid = xmlTiffData->FirstChildElement("UUID");
    if (!uuid) {
        SLIDEIO_LOG(WARNING) << "OTScene: missing required UUID element in the xml metadata";
    }
    const char* fileNameAttr = uuid->Attribute("FileName");
    m_filePath = (fileNameAttr == nullptr || *fileNameAttr == '\0')
        ? m_filePath
        : std::filesystem::path(directoryPath).append(fileNameAttr).string();

    m_tiff = files->getOrOpen(m_filePath);
    if (!m_tiff) {
        RAISE_RUNTIME_ERROR << "OTScene: cannot open file " << m_filePath << " with libtiff";
    }
	m_directories.resize(m_planeCount);
	for (int plane = 0; plane < m_planeCount; ++plane) {
        TiffTools::scanTiffDir(m_tiff, m_firstIFD+plane, 0, m_directories[plane]);
    }
}

const TiffDirectory& TiffData::getTiffDirectory(int plane) const {
	if (plane < 0 || plane >= m_planeCount) {
		RAISE_RUNTIME_ERROR << "TiffData: Invalid plane index: " << plane;
	}
    return m_directories[plane];
}

//void TiffData::readTile(std::vector<int>& channelIndices, int zSlice, int tFrame, int zoomLevel,
//    int tileIndex, std::vector<cv::Mat>& rasters) const {
//
//    if(!isValueInRange(zSlice, m_zSliceRange) || !isValueInRange(tFrame, m_tFrameRange)) {
//        return;
//    }
//
//    // Create a list of channel indices that are in the range of TiffData
//    std::list<int> myChannelIndices;
//    std::copy_if(channelIndices.begin(), channelIndices.end(), std::back_inserter(myChannelIndices),
//        [this](int channel) { return isValueInRange(channel, m_channelRange);});
//	// No channels in the range
//	if (myChannelIndices.empty()) {
//		return;
//	}
//
//    std::vector<int> localChannelIndices;
//	const TiffDirectory& mainDir = m_directories[0];
//	localChannelIndices.resize(mainDir.channels);
//	for (int ch = 0; ch < mainDir.channels; ++ch) {
//		localChannelIndices[ch] = ch;
//	}
//
//    while(!myChannelIndices.empty()) {
//		const int myChannelCount = static_cast<int>(myChannelIndices.size());
//		const int channelIndex = myChannelIndices.front();
//		const int channel = channelIndices[channelIndex];
//		std::list<std::pair<std::string, int>> listCoords = {
//			{ DimC, m_channelRange.start },
//			{ DimZ, m_zSliceRange.start },
//			{ DimT, m_tFrameRange.start }
//		};
//        auto coords = m_dimensions->createCoordinates(listCoords);
//		int cIndex = m_dimensions->getDimensionIndex(DimC);
//		int zIndex = m_dimensions->getDimensionIndex(DimZ);
//		int tIndex = m_dimensions->getDimensionIndex(DimT);
//		for (int plane = 0; plane < m_planeCount; ++plane) {
//			if (coords[cIndex] != channel || coords[zIndex] != zSlice || coords[tIndex] != tFrame) {
//				continue;
//			}
//			const TiffDirectory& mainDir = m_directories[plane];
//			const TiffDirectory& dir = zoomLevel==0?mainDir:mainDir.subdirectories[zoomLevel-1];
//            cv::Mat localRaster;
//			readTileChannels(dir, tileIndex, localChannelIndices, localRaster);
//            if(localChannelIndices.size() == 1) {
//				rasters[channelIndex] = localRaster;
//                myChannelIndices.remove(channelIndex);
//			}
//            else {
//                cv::Mat channelRaster;
//                for (int localChannelIndex : localChannelIndices) {
//					const int globChannel = coords[cIndex] + localChannelIndex;
//                    auto itGlobalChannel = std::find(myChannelIndices.begin(), myChannelIndices.end(), globChannel);
//					if ( itGlobalChannel!= myChannelIndices.end()) {
//                        cv::extractChannel(localRaster, channelRaster, localChannelIndex);
//						rasters[globChannel] = channelRaster;
//                        myChannelIndices.remove(globChannel);
//					}
//                }
//            }
//			m_dimensions->incrementCoordinates(coords);
//		}
//        if (myChannelCount == static_cast<int>(myChannelIndices.size())) {
//			RAISE_RUNTIME_ERROR << "TiffData: unexpected block: no channels were read";
//        }
//    }
//}

void TiffData::readTile(std::vector<int>& channelIndices, int zSlice, int tFrame, int zoomLevel,
    int tileIndex, std::vector<cv::Mat>& rasters) const {

    if (!isValueInRange(zSlice, m_zSliceRange) || !isValueInRange(tFrame, m_tFrameRange)) {
        return;
    }

    // Filter channel indices that are in the range of TiffData
    std::vector<int> myChannelIndices;
    std::copy_if(channelIndices.begin(), channelIndices.end(), std::back_inserter(myChannelIndices),
        [this](int channel) { return isValueInRange(channel, m_channelRange); });

    if (myChannelIndices.empty()) {
        return;
    }

    const TiffDirectory& mainDir = m_directories[0];
    std::vector<int> localChannelIndices(mainDir.channels);
    std::iota(localChannelIndices.begin(), localChannelIndices.end(), 0);

    std::list<std::pair<std::string, int>> listCoords = {
        { DimC, m_channelRange.start },
        { DimZ, m_zSliceRange.start },
        { DimT, m_tFrameRange.start }
    };
    auto coords = m_dimensions->createCoordinates(listCoords);
    int cIndex = m_dimensions->getDimensionIndex(DimC);
    int zIndex = m_dimensions->getDimensionIndex(DimZ);
    int tIndex = m_dimensions->getDimensionIndex(DimT);

    for (int plane = 0; plane < m_planeCount; ++plane) {
        if (coords[zIndex] != zSlice || coords[tIndex] != tFrame) {
            m_dimensions->incrementCoordinates(coords);
            continue;
        }

        const TiffDirectory& dir = (zoomLevel == 0) ? m_directories[plane] : m_directories[plane].subdirectories[zoomLevel - 1];
        cv::Mat localRaster;
        readTileChannels(dir, tileIndex, localChannelIndices, localRaster);

        for (int localChannelIndex : localChannelIndices) {
            int globChannel = coords[cIndex] + localChannelIndex;
            auto it = std::find(myChannelIndices.begin(), myChannelIndices.end(), globChannel);
            if (it != myChannelIndices.end()) {
                cv::Mat channelRaster;
                cv::extractChannel(localRaster, channelRaster, localChannelIndex);
                rasters[globChannel] = channelRaster;
                myChannelIndices.erase(it);
            }
        }

        if (myChannelIndices.empty()) {
            break;
        }

        m_dimensions->incrementCoordinates(coords);
    }

    if (!myChannelIndices.empty()) {
        RAISE_RUNTIME_ERROR << "TiffData: unexpected block: no channels were read";
    }
}

void TiffData::readTileChannels(const TiffDirectory& dir, int tileIndex, const std::vector<int>& channelIndices, cv::OutputArray raster) const {
    if (dir.tiled) {
        TiffTools::readTile(m_tiff, dir, tileIndex, channelIndices, raster);
    }
    else if (tileIndex == 0) {
		cv::Mat dirRaster;
        TiffTools::readStripedDir(m_tiff, dir, dirRaster);
        if (static_cast<int>(channelIndices.size()) == 1 && dirRaster.channels()==1 && channelIndices[0] == 0) {
            raster.assign(dirRaster);
        }
        else {
            std::vector<cv::Mat> channelRasters(channelIndices.size());
            int outputChannelIndex = 0;
            for (int internalChannelIndex : channelIndices) {
                cv::Mat channelRaster;
                cv::extractChannel(dirRaster, channelRasters[outputChannelIndex], internalChannelIndex);
                ++outputChannelIndex;
            }
			if (channelRasters.size() == 1) {
				raster.assign(channelRasters[0]);
			}
			else {
                cv::merge(channelRasters, raster);
            }
        }
    }
    else {
        RAISE_RUNTIME_ERROR << "TiffData: unexpected tile index " << tileIndex << " for striped directory";
    }
}
