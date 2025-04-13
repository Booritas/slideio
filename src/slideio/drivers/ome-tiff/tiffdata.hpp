// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
#pragma once
#include "slideio/drivers/ome-tiff/ot_api_def.hpp"
#include "slideio/imagetools/tiffkeeper.hpp"
#include <opencv2/core/types.hpp>


namespace slideio
{
    class TIFFFiles;
    class Dimensions;
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
			void init(const std::string& directoryPath, TIFFFiles* files, Dimensions* dims, tinyxml2::XMLElement* xmlTiffData);
            bool isInRange(int channel, int slice, int frame) const {
                return (channel >= m_channelRange.start && channel < m_channelRange.end &&
                    slice >= m_zSliceRange.start && slice < m_zSliceRange.end &&
                    frame >= m_tFrameRange.start && frame < m_tFrameRange.end);
            }
			int getFirstIFD() const {
				return m_firstIFD;
			}
			int getPlaneCount() const {
				return m_planeCount;
			}
			const cv::Range& getChannelRange() const {
				return m_channelRange;
			}
			const cv::Range& getZSliceRange() const {
				return m_zSliceRange;
			}
			const cv::Range& getTFrameRange() const {
				return m_tFrameRange;
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
        private:
            cv::Range m_channelRange = {};
            cv::Range m_zSliceRange = {};
            cv::Range m_tFrameRange = {};
            int m_firstIFD = 0;
            int m_planeCount = 0;
            std::string m_filePath;
            libtiff::TIFF* m_tiff;
            std::vector<TiffDirectory> m_directories;
            Dimensions* m_dimensions = nullptr;
        };
    } 
}
