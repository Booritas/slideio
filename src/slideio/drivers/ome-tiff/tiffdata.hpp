// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#pragma once
#include "slideio/drivers/ome-tiff/ot_api_def.hpp"
#include "slideio/imagetools/tiffkeeper.hpp"
#include "slideio/drivers/ome-tiff/otdimensions.hpp"


namespace slideio
{
    class TIFFFiles;
}

namespace tinyxml2
{
    class XMLElement;
}

namespace slideio
{
    namespace ometiff
    {
        class SLIDEIO_OMETIFF_EXPORTS TiffData
        {
        public:
			TiffData() = default;
			void init(const std::string& directoryPath, TIFFFiles* files, OTDimensions* dims, tinyxml2::XMLElement* xmlTiffData);
			bool isInRange(int channel, int slice, int frame) const;
			int getFirstIFD() const {
				return m_firstIFD;
			}
			int getPlaneCount() const {
				return m_planeCount;
			}
			const std::string& getFilePath() const {
				return m_filePath;
			}
			const TiffDirectory& getTiffDirectory(int plane) const;
			int getTiffDirectoryCount() const {
				return static_cast<int>(m_directories.size());
			}
            void readTile(std::vector<int>& channelIndices, int zSlice, int tFrame, int zoomLevel, int tileIndex,
                         std::vector<cv::Mat>& rasters) const;
            void readTileChannels(const TiffDirectory& dir, int tileIndex, const std::vector<int>& channelIndices, cv::OutputArray raster) const;
			const OTDimensions::Coordinates& getCoordinatesFirst() const {
				return m_coordinatesFirst;
			}
			const OTDimensions::Coordinates& getCoordinatesLast() const {
				return m_coordinatesLast;
			}
        private:
            int m_firstIFD = 0;
            int m_planeCount = 0;
            std::string m_filePath;
            libtiff::TIFF* m_tiff;
            std::vector<TiffDirectory> m_directories;
            OTDimensions* m_dimensions = nullptr;
            OTDimensions::Coordinates m_coordinatesFirst;
			OTDimensions::Coordinates m_coordinatesLast;
        };
    } 
}
